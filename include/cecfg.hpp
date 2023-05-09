#ifndef CECFG_HPP
#define CECFG_HPP

#include <cstdio>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

// @todo add namespaces

namespace cecfg
{
	using String = std::string;
	
	struct Variable
	{
		enum Type
		{
			Null = 0, // reserved for future
			Int32,
			Float,
			Double,
			String
		};
		Type tp;
		::cecfg::String data;
	};
	
	struct Loader
	{
		std::unordered_map<String, Variable> vars;
		
		Variable *operator [](String const &s)
		{
			auto iter = vars.find(s);
			if (iter != vars.end())
				return &iter->second;
			return nullptr;
		}
		
		Variable const *operator [](String const &s) const
		{
			auto iter = vars.find(s);
			if (iter != vars.end())
				return &iter->second;
			return nullptr;
		}
		
		bool getOption(String const &s, void *dest, Variable::Type type)
		{
			Variable *var = (*this)[s];
			if (!var || var->tp != type)
				return false;
			
			const char *cstr = var->data.c_str();
			
			if (type == Variable::String)
				*reinterpret_cast<String *>(dest) = var->data;
			else if (type == Variable::Int32)
				*reinterpret_cast<int32_t *>(dest) = std::atoi(cstr);
			else if (type == Variable::Float)
				*reinterpret_cast<float *>(dest) = (float)std::atof(cstr);
			else if (type == Variable::Double)
				*reinterpret_cast<double *>(dest) = std::atof(cstr);
			else return false;
			
			return true;
		}
		
		int fromFile(const char *filename)
		{
			FILE *fish = std::fopen(filename, "r");
			if (!fish)
				return 1;
			
			const auto isLetter = [](char ch) {
				return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
			};
			const auto isWhitespace = [](char ch) {
				return ch == ' ' || ch == '\t';
			};
			const auto isLineSep = [](char ch) {
				return ch == '\n' || ch == '\r';
			};
			
			int retcode = 0;
			char *buf = new char[1024];
			
			while (fgets(buf, 1024, fish) != nullptr)
			{
				retcode = 2; // syntax error code
				
				Variable::Type tp;
				if (buf[0] == 's')
					tp = Variable::String;
				else if (buf[0] == 'i')
					tp = Variable::Int32;
				else if (buf[0] == 'f')
					tp = Variable::Float;
				else if (buf[0] == '#' || isWhitespace(buf[0]) || isLineSep(buf[0]))
					continue;
				else break;
				
				if (buf[1] != ':')
					break;
				
				char *namebeg = buf + 2;
				while (isWhitespace(*namebeg))
					++namebeg;
				if (!isLetter(*namebeg))
					break;
				
				char *nameend = namebeg;
				while (isLetter(*++nameend));
				
				char *databeg = nameend;
				while (isWhitespace(*++databeg));
				if (*databeg != '=')
					break;
				++databeg;
				
				char *dataend = databeg;
				while (*dataend && !isLineSep(*dataend))
					++dataend;
				
				vars[String(namebeg, nameend - namebeg)] = {
					.tp = tp,
					.data = String(databeg, dataend - databeg),
				};
				retcode = 0;
			}
			
			delete[] buf;
			fclose(fish);
			
			return retcode;
		}
	};
} // namespace cecfg

#endif // CECFG_HPP