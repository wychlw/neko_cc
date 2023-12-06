/**
 * @file out.cc
 * @author 泠妄 (lingwang@wcysite.com)
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#include <iostream>
#include <string>

#include "out.hh"

namespace neko_cc
{

log_level_t log_level;

void _error(std::string func, int line, std::string msg)
{
	throw std::runtime_error(func + ", " + std::to_string(line) + " : " +
				 msg);
}

void _error(std::string msg, stream &ss, bool output_near)
{
	std::string m = msg;

	if (output_near) {
		for (int i = 0; i < 5; i++) {
			ss.unget();
		}
		m += "\n\t near: ";
		int i = 0;
		while (ss.peek() != EOF && ss.peek() != '\r' &&
		       ss.peek() != '\n') {
			i++;
			m += ss.get();
		}
		for (; i >= 0; i--) {
			ss.unget();
		}
		m+="\n\t           ^\n";
		
	}
	throw std::runtime_error(m);
}

void _error(std::string func, int line, std::string msg, stream &ss,
	    bool output_near)
{
	_error(func + ", " + std::to_string(line) + " : " + msg, ss,
	       output_near);
}

void info(std::string msg)
{
	if (log_level > INFO) {
		return;
	}
	std::cout << "INFO: " << msg << std::endl;
}

void warn(std::string msg)
{
	if (log_level > WARN) {
		return;
	}
	std::cout << "WARN: " << msg << std::endl;
}

void _debug(std::string pos, std::string tok_name, std::string msg)
{
	if (log_level > DEBUG) {
		return;
	}
	std::cout << "DEBUG: " << pos << " " << tok_name << std::endl;
	if (msg.length() > 0) {
		std::cout << "\tMSG: " << msg << std::endl;
	}
}

}