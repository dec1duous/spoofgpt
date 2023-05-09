#ifndef KPSM2SK_HPP
#define KPSM2SK_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <cassert>
#include <iostream>
#include <cmath>
#include <random>

namespace kpsm2sk
{
	typedef int Integer;
	
	struct NodeAddr
	{
		Integer layer;
		Integer node;
	};
	
	struct LinkAddr
	{
		Integer layer;
		Integer node;
		Integer link;
	};
	
	struct Connection
	{
		float k;
		float w;
		float c;
		NodeAddr addr;
	};
	
	struct Node
	{
		float s; // signal
		float c; // conductivity
		std::vector<Connection> links;
	};
	
	struct tuneResult
	{
		Integer fails;
		Integer total;
		
		inline tuneResult &operator +=(const tuneResult &oth) {
			this->fails += oth.fails;
			this->total += oth.total;
			return *this;
		}
		inline tuneResult &operator -=(const tuneResult &oth) {
			this->fails -= oth.fails;
			this->total -= oth.total;
			return *this;
		}
		
		inline tuneResult operator +(const tuneResult &oth) const {
			return {this->fails + oth.fails, this->total + oth.total};
		}
		inline tuneResult operator -(const tuneResult &oth) const {
			return {this->fails - oth.fails, this->total - oth.total};
		}
	};
	
	struct tuneSet {
		std::vector<float> input;
		std::vector<float> output;
	};

	class Network
	{
	public:
		std::vector<std::vector<Node>> mat;
		
		inline Network() = default;
		
		inline Network (
			std::vector<Integer> const &config,
			float k = 0.25f,
			float w = 0.5f,
			float c = 0.0f
		) {
			buildByConfig(config, k, w, c);
		}
		
		inline Network (
			std::vector<Integer> const &config,
			std::vector<Integer> const &branching,
			float k = 0.25f,
			float w = 0.5f,
			float c = 0.0f
		) {
			buildByConfig(config, branching, k, w, c);
		}
		
		inline Integer layers() {
			return mat.size();
		}
		inline Integer nodes()
		{
			Integer n = 0;
			for (auto const &layer: mat) {
				n += layer.size();
			}
			return n;
		}
		
		enum class ConProperty
		{
			K = 1, // forward coefficient
			W = 2, // weight
			C = 3, // conductivity impact
		};
		
		inline Node &operator [](NodeAddr i) {
			return mat[i.layer][i.node];
		}
		inline Node const &operator [](NodeAddr i) const {
			return mat[i.layer][i.node];
		}
		
		inline std::vector<Node> &operator [](Integer i) {
			return mat[i];
		}
		inline std::vector<Node> const &operator [](Integer i) const {
			return mat[i];
		}
		
		inline void expandLayer (
			Integer layer,
			Integer numNodes,
			float k = 0.25f,
			float w = 0.0f,
			float c = 0.0f
		) {
			Integer prevNodes = mat[layer].size();
			if (numNodes == prevNodes)
				return;
			assert(numNodes > prevNodes);
			
			mat[layer].resize(numNodes);
			
			// Update connections of previous layer
			if (layer > 0)
				for (Integer i = prevNodes; i != numNodes; ++i)
					for (Node &node: mat[layer - 1])
						node.links.push_back(Connection {k, w, c, NodeAddr {layer, i}});
			
			// And this layer
			if (layer + 1 < mat.size())
				for (Integer i = prevNodes; i != numNodes; ++i)
					for (Integer n = 0; n != mat[layer + 1].size(); ++n)
						mat[layer][i].links.push_back(Connection {k, w, c, NodeAddr {layer + 1, n}});
		}
		
		inline void insertLayer (
			Integer layer,
			Integer numNodes,
			float k = 0.25f,
			float w = 0.0f,
			float c = 0.0f
		) {
			Integer prevNodes = mat[layer].size();
			assert(numNodes >= prevNodes);
			
			// Move addresses first
			for (Integer i = layer; i < mat.size(); ++i)
				for (Node &node: mat[i])
					for (auto &lnk: node.links)
						++lnk.addr.layer;
			
			// Then layers
			mat.resize(mat.size() + 1);
			for (Integer i = mat.size() - 1; i > layer; --i)
				mat[i] = std::move(mat[i - 1]);
			
			mat[layer] = std::vector<Node>(numNodes);
					
			// Link all nodes from next layer
			if (layer + 1 < mat.size())
				for (Integer i = 0; i < mat[layer + 1].size(); ++i)
					for (Integer n = 0; n != mat[layer].size(); ++n)
						mat[layer][n].links.push_back(
							Connection {
								// don't change the network output
								i == n ? 1.f : k,
								i == n ? 1.f : w,
								i == n ? 0.f : c,
								NodeAddr {layer + 1, i}
							}
						);
			
			// Update previous layer
			if (layer > 0)
				for (Integer i = prevNodes; i != numNodes; ++i)
					for (Node &node: mat[layer - 1])
						node.links.push_back(Connection {k, w, c, NodeAddr {layer, i}});
		}
		
		inline void buildByConfig (
			std::vector<Integer> const &config,
			float k = 0.25f,
			float w = 0.5f,
			float c = 0.0f
		) {
			const Integer layers = config.size();
			mat = std::vector<std::vector<Node>>(layers);
			
			for (Integer nLayer = 0; nLayer != layers; ++nLayer)
			{
				mat[nLayer].resize(config[nLayer]);
				
				if (nLayer + 1 == layers) break;
				
				for (Node &node: mat[nLayer])
				{
					// insert links to all nodes in next layer
					for (Integer i = 0; i < config[nLayer + 1]; ++i)
					{
						node.links.push_back(Connection {k, w, c, NodeAddr {nLayer + 1, i}});
					}
				}
			}
		}
		
		inline void buildByConfig (
			std::vector<Integer> const &config,
			std::vector<Integer> const &branching,
			float k = 0.25f,
			float w = 0.5f,
			float c = 0.0f
		) {
			const Integer layers = config.size();
			mat = std::vector<std::vector<Node>>(layers);
			
			for (Integer nLayer = 0; nLayer != layers; ++nLayer)
			{
				mat[nLayer].resize(config[nLayer]);
				
				if (nLayer + 1 == layers) break;
				
				for (Integer n = 0; n != mat[nLayer].size(); ++n)
				{
					Node &node = mat[nLayer][n];
					
					Integer i = n - branching[nLayer];
					if (i < 0) i = 0;
					
					Integer end = n + branching[nLayer];
					if (end > config[nLayer + 1])
						end = config[nLayer + 1];
					
					for (; i < end; ++i)
						node.links.push_back(Connection {k, w, c, NodeAddr {nLayer + 1, i}});
				}
			}
		}
		
		static inline float normalize(float value)
		{
			// avoid NaN and inf
			if (value >= 0.f && value <= 1.f)
				return value;
			if (value > 1.f)
				return 1.f;
			return 0.f;
		}
			
		inline void reset(Integer nLayer) {
			for (Node &node: mat[nLayer]) {
				node.s = 1.f;
				node.c = 1.f;
			}
		}
		inline void reset() {
			for (auto &node: mat[0]) {
				node.c = 1.f;
			}
			for (Integer i = 1; i < mat.size(); ++i) {
				reset(i);
			}
		}
		inline void flow(Integer nLayer) {
			for (Node &node: mat[nLayer]) {
				for (auto &lnk: node.links) {
					float tmp = lnk.k + node.s - 2.f * node.s * lnk.k;
					tmp *= node.c;
					/* if (lnk.w * tmp > 0.07f)
						std::cout << "oh my nasty {" << lnk.addr.layer  << ", " << lnk.addr.node << "} (" << lnk.k << ", " << lnk.w << "), " << node.s << " (" << (&node - mat[nLayer].data()) << ")\n"; */
						
					(*this)[lnk.addr].s *= 1.f - lnk.w * tmp;
					(*this)[lnk.addr].c *= 1.f - lnk.c * tmp;
				}
			}
		}
		inline void flow() {
			for (Integer i = 0; i + 1 < mat.size(); ++i) {
				flow(i);
			}
		}
		inline void run() {
			reset();
			flow();
		}		
		
		inline void loadInput(const std::vector<float> &input) {
			for (Integer i = 0; i != input.size(); ++i) {
				mat[0][i].s = input[i];
			}
		}
		
		inline float calculateError(const std::vector<tuneSet> &tuneData)
		{
			float err = 0.f;
			
			for (Integer i = 0; i != tuneData.size(); ++i)
			{
				loadInput(tuneData[i].input);
				run();
				for (Integer n = 0; n != mat.back().size(); ++n)
				{
					float diff = mat.back()[n].s - tuneData[i].output[n];
					err += diff * diff;
				}
			}
			return err;
		}
		
		inline float recalculateError(Integer flowBeg, std::vector<float> const &expOutput)
		{
			float err = 0.f;
			
			for (Integer n = flowBeg; n + 1 < mat.size(); ++n)
			{
				reset(n + 1);
				flow(n);
			}
			for (Integer n = 0; n != mat.back().size(); ++n)
			{
				float diff = mat.back()[n].s - expOutput[n];
				err += diff * diff;
			}
			return err;
		}
		
		// get new value for given property of the link to output signal as expected (or as near as possible)
		inline float solveDelta(NodeAddr addr, Integer numLink, float expSignal, ConProperty prop)
		{
			const Node &node = (*this)[addr];
			const auto &lnk = node.links[numLink];
			
			if (prop == ConProperty::K)
			{
				float compExp = (1.f - expSignal) / (lnk.w * node.c);
				
				// compExp == expK + node.s - 2.f * node.s * expK
				// expK - 2.f * node.s * expK == compExp - node.s
				// expK * (1 - 2 * node.s) == compExp - node.s
				
				float expK = (compExp - node.s) / (1.f - 2.f * node.s);
				return normalize(expK);
			}
			// if (prop == ConProperty::W)
			{
				float prop = (1.f - expSignal) / (node.c * (lnk.k + node.s - 2.f * node.s * lnk.k));
				return normalize(prop);
			}
			// if (prop == ConProperty::C)
			// {
			// 	float prop = (1.f - expSignal) / (node.c * (lnk.k + node.s - 2.f * node.s * lnk.k));
			// 	return normalize(prop);
			// }
		}
		
		// @todo idea, calculate expected signal from middle of network when further layers are tuned
		
		// get signal expected at given node, considering other nodes unchanged.
		// the network must be run with correct input
		inline float predictSignal(NodeAddr addr, std::vector<float> const &expOutput)
		{
			assert(expOutput.size() == mat.back().size());
			
			if (addr.layer + 1 == mat.size())
				return expOutput[addr.node];
			
			// @todo calculate when expOutput.size() > 1
			if (expOutput.size() != 1 || addr.layer + 2 != mat.size())
				return (*this)[addr].s;
			
			const Node &node = (*this)[addr];
			const auto &lnk = node.links[0];
			{
				float curSignal = 1.f - lnk.w * node.c * (lnk.k + node.s - 2.f * node.s * lnk.k);
				// get the output as if the node didn't affect it
				float clearOutput = mat.back()[0].s / curSignal;
				float expSignal = expOutput[0] / clearOutput;
				
				float compExp = (1.f - expSignal) / (lnk.w * node.c);
				
				// compExp == lnk.k + expS - 2.f * expS * lnk.k
				// expS - 2 * expS * lnk.k == compExp - lnk.k
				// expS * (1 - 2 * lnk.k) == compExp - lnk.k
				
				float expS = (compExp - lnk.k) / (1.f - 2.f * lnk.k);
				// std::cout << "l: " << addr.layer << " n: " << addr.node << " expS: " << expS << "node.s: " << node.s << '\n';
				return expS;
			}
		}
			
		// collect tuning summary for links of given node
		inline std::vector<std::vector<float>> collectTuningSummary(NodeAddr addr, ConProperty prop, const std::vector<tuneSet> &tuneData)
		{
			Node &node = (*this)[addr];
			std::vector<std::vector<float>> tuneSmr(node.links.size());
			
			for (auto &tset: tuneSmr)
				tset.reserve(tuneData.size());
			
			for (const auto &set: tuneData)
			{
				loadInput(set.input);
				run();
				
				for (Integer i = 0; i < node.links.size(); ++i)
				{
					float sig = predictSignal(node.links[i].addr, set.output);
					float p = solveDelta(addr, i, sig, prop);
					tuneSmr[i].push_back(p);
				}
			}
			return tuneSmr;
		}
		
		inline std::vector<float> collectTuningSummary(LinkAddr addr, ConProperty prop, const std::vector<tuneSet> &tuneData)
		{
			Connection &lnk = mat[addr.layer][addr.node].links[addr.link];
			std::vector<float> tuneSmr;
			
			tuneSmr.reserve(tuneData.size());
			
			for (const auto &set: tuneData)
			{
				loadInput(set.input);
				run();
				
				float sig = predictSignal(lnk.addr, set.output);
				float p = solveDelta({addr.layer, addr.node}, addr.link, sig, prop);
				tuneSmr.push_back(p);
			}
			return tuneSmr;
		}
		
		inline float tuneDeep(NodeAddr addr, Integer numLink, ConProperty prop, const std::vector<tuneSet> &tuneData, float learnMul)
		{
			Connection &lnk = mat[addr.layer][addr.node].links[numLink];
			std::vector<float> tuneSmr = collectTuningSummary({addr.layer, addr.node, numLink}, prop, tuneData);
			
			float avg = 0.f, min = 1.f, max = 0.f;
			for (float f: tuneSmr)
			{
				if (f < min) min = f;
				if (f > max) max = f;
				avg += f;
			}
			avg /= tuneSmr.size();
			
			float &p = (
				prop == ConProperty::K ? lnk.k :
			 	prop == ConProperty::W ? lnk.w :
			 	lnk.c
			);
			
			float diff = (1.f + min - max) * (avg - p) * learnMul;
			p = normalize(p + diff);
			
			return std::fabs(diff);
		}
		
		// @todo idea: try 1-3-1 network architecture
		inline float tuneDeep(NodeAddr addr, ConProperty prop, const std::vector<tuneSet> &tuneData, float learnMul)
		{
			Node &node = (*this)[addr];
			
			float res = 0.f;
			for (Integer i = 0; i != node.links.size(); ++i)
				res += tuneDeep(addr, i, prop, tuneData, learnMul);
			
			res /= node.links.size();
			return res;
		}
		
		inline tuneResult tuneShallow(NodeAddr addr, ConProperty prop, const std::vector<tuneSet> &tuneData, float learnMul)
		{
			float currentErr = calculateError(tuneData);
			Integer fails = 0;
			Integer total = 0;
			std::mt19937 rgen;
			
			Node &node = (*this)[addr];
			for (auto &lnk: node.links)
			{
				++total;
				float &value = prop == ConProperty::K ? lnk.k : prop == ConProperty::W ? lnk.w : lnk.c;
				float prevValue = value;
				float err;
				
				if (value < 1.f)
				{
					value = prevValue + learnMul;
					if (value > 1.f)
						value = 1.f;
					err = calculateError(tuneData);
					if (err < currentErr)
						continue;
					else if (err == currentErr)
					{
						++fails;
						continue;
					}
				}
				
				if (value > 0.f)
				{
					value = prevValue - learnMul;
					if (value < 0.f)
						value = 0.f;
					err = calculateError(tuneData);
					if (err < currentErr)
						continue;
					else if (err == currentErr)
					{
						++fails;
						continue;
					}
				}
				
				++fails;
				value = prevValue;
			}
			return {fails, total};
		}
	};
}

#endif
