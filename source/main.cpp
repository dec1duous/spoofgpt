#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <kpsm2sk.hpp>

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <vector>
#include <deque>
#include <cmath>
#include <string>
#include <sstream>
#include <utility>
#include <chrono>
#include <thread>
#include <stdexcept>

int g_inputWords = 3;

class Text
{
public:
	std::vector<std::string> voc;
	std::vector<int> seq;
	std::vector<bool> points;
	
	std::string const &operator [](float ind) {
		auto i = (int)(ind * voc.size());
		if (i == voc.size())
			return voc.back();
		return voc[i];
	}
	
	static bool isLetter(char Ch)
	{
		return (Ch >= 'A' && Ch <= 'Z') || (Ch >= 'a' && Ch <= 'z') || Ch < '\0';
	}
	
	static char toLower(char Ch)
	{
		if (Ch >= 'A' && Ch <= 'Z')
			return Ch - 'A' + 'a';

		return Ch;
	}
	
	void addWord(const char *wbeg, const char *wend)
	{
		if (wbeg == wend) return;
		bool haspoint = wend[-1] == '.';
		if (haspoint) --wend;
		if (wbeg == wend) return;
		
		std::string newst(wbeg, wend - wbeg);
		int num = -1;
		// @todo optimize
		for (int i = 0; i != voc.size(); ++i) {
			if (voc[i] == newst) {
				num = i;
				break;
			}
		}
		if (num == -1) {
			num = voc.size();
			voc.push_back(std::move(newst));
		}
		seq.push_back(num);
		points.push_back(haspoint);
	}
	
	int loadFile(const char *file)
	{
		FILE *fish = fopen(file, "rb");
		if (!fish)
			return 1;
		fseek(fish, 0, SEEK_END);
		int fsize = ftell(fish);
		if (fsize == 0)
			return 2;
		fseek(fish, 0, SEEK_SET);
		
		char *buf = new char[fsize + 1];
		buf[fsize] = '\0';
		fread(buf, 1, fsize, fish);
		
		fclose(fish);
		for (int i = 0; i < fsize; ++i)
		{
			if (!isLetter(buf[i]) && buf[i] != '.')
				buf[i] = ' ';
			buf[i] = toLower(buf[i]);
		}
		
		int wbeg = 0;
		while (true)
		{
			while (wbeg < fsize && buf[wbeg] == ' ')
				wbeg++;
			int wend = wbeg;
			while (wend < fsize && buf[wend] != ' ')
				wend++;
			if (wend != wbeg)
			{
				addWord(buf + wbeg, buf + wend);
				wbeg = wend;
			}
			else break;
		}
		
		delete[] buf;
		return 0;
	}
};

class SpoofGPT: public kpsm2sk::Network
{
protected:
	Text pTxt;
	std::mt19937 pRgen;
	
public:
	inline SpoofGPT() = default;
	
	inline SpoofGPT (const char *filename)
	{
		if (buildByText(filename) != 0)
			throw std::runtime_error("failed to load file");
	}
	
	inline int buildByText(const char *filename)
	{
		using namespace kpsm2sk;
		
		int res = pTxt.loadFile(filename);
		if (res != 0)
			return res;
		
		Integer netWordSize = pTxt.voc.size() + 1; // +1 for syntax (currently points)
		std::vector<Integer> netconf {netWordSize * g_inputWords, 0, 0, netWordSize};
		this->buildByConfig(netconf, 0.f, 1.f, 0.f);
		
		mat[2].resize(netWordSize);
		for (Integer i = 0; i != mat[2].size(); ++i)
		{
			mat[2][i].links.push_back(Connection {
				.k = 0.f, .w = 1.f, .c = 0.f,
				.addr = NodeAddr {3, i}
			});
		}
		
		return 0;
	}
	
	// make AND then OR logic about the nodes to make word following pattern
	inline void addLogicPattern (
		std::vector<kpsm2sk::Integer> const &inputNodes,
		kpsm2sk::Integer outputNode
	) {
		using namespace kpsm2sk;
		
		Node andPart {.links = {
			{.k = 0.f, .w = 1.f, .addr = {2, outputNode}}
		}};
		mat[1].push_back(andPart);
		
		for (auto i: inputNodes)
		{
			mat[0][i].links.push_back(Connection {
				.k = 1.f, .w = 1.f, .c = 0.f,
				.addr = {1, (Integer)(mat[1].size() - 1)}
			});
		}
	}
	
	inline void addWordPattern(int seqbeg, float learnMul = 0.7f)
	{
		// @todo consider points
		using namespace kpsm2sk;
		Integer netWordSize = pTxt.voc.size() + 1;
		Integer predictWordIndex = pTxt.seq[seqbeg + g_inputWords];
		
		std::vector<Integer> inputs(g_inputWords);
		for (Integer i = 0; i != g_inputWords; ++i)
		{
			Integer vocabWordIndex = pTxt.seq[seqbeg + i];
			inputs[i] = i * netWordSize + vocabWordIndex;
		}
		addLogicPattern(inputs, predictWordIndex);
	}
	
	inline void loadInput(std::deque<int> const &q)
	{
		using namespace kpsm2sk;
		
		assert(q.size() > 0 && q.size() <= g_inputWords);
		int iter = g_inputWords - q.size();
		
		Integer netWordSize = pTxt.voc.size() + 1;
		
		// @todo optimize
		for (auto &node: mat[0])
			node.s = 0.f;
		
		for (int n: q)
		{
			// @todo consider points
			mat[0][iter * netWordSize + n].s = 1.f;
			++iter;
		}
	}
	
	// pick randomly one of three most probable words
	inline int readOutput()
	{
		using namespace kpsm2sk;
		
		float probab0 = -1.f, probab1 = -1.f, probab2 = -1.f;
		int probabWord0, probabWord1, probabWord2;
		 
		for (Integer i = 0; i != pTxt.voc.size(); ++i)
		{
			auto const &node = mat.back()[i];
			if (node.s > probab0) {
				probab0 = node.s;
				probabWord0 = i;
			}
			else if (node.s > probab1) {
				probab1 = node.s;
				probabWord1 = i;
			}
			else if (node.s > probab2) {
				probab2 = node.s;
				probabWord2 = i;
			}
		}
		
		if (probab0 == -1.f || probab1 == -1.f || probab2 == -1.f)
			return pRgen() % pTxt.voc.size();
		
		float probabMul = 1.f / (probab0 + probab1 + probab2);
		probab0 *= probabMul;
		probab1 *= probabMul;
		
		float randNum = pRgen() * (1.f / (float)0xffffffff);
		if (randNum > probab0 + probab1)
			return probabWord2;
		else if (randNum > probab0)
			return probabWord1;
		return probabWord0;
	}
	
	inline Text const &getText() { return pTxt; }
};

int main(int argc, char** argv)
{
	using namespace kpsm2sk;
	
	const char *txtFile = argc > 1 ? argv[1] : "input.txt";
	SpoofGPT theNet(txtFile);
	
	float learnMul = argc > 2 ? std::atof(argv[2]) : 0.7f;
	
	if (argc > 3)
		g_inputWords = std::atoi(argv[3]);
	
	for (int i = 0; i < ((int)theNet.getText().seq.size() - g_inputWords - 1); ++i)
		theNet.addWordPattern(i, learnMul);
	
	std::deque<int> textGen;
	std::mt19937 rgen;
	
	// 'launch' the generator
	for (int i = 0; i < g_inputWords; ++i)
	{
		int ind = theNet.getText().seq[i];
		textGen.push_back(ind);
	}
	
	while (true)
	{
		std::cout << theNet.getText().voc[textGen.back()] << ' ';
		std::this_thread::sleep_for(std::chrono::milliseconds(300));
		if (textGen.size() > g_inputWords)
			textGen.pop_front();
		theNet.loadInput(textGen);
		theNet.run();
		
		int word = theNet.readOutput();
		if (word != textGen.back())
			textGen.push_back(word);
		else {
			textGen.push_back(rgen() % theNet.getText().voc.size());
			std::cout << '!';
		}
	}
}