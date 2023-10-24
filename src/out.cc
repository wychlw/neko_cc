#include <iostream>
#include <string>

#include "out.hh"


namespace neko_cc
{

void _error(std::string msg)
{
	throw std::runtime_error(msg);
}

void _error(std::string msg, stream &ss, bool output_near)
{
	std::string m = msg;
	if (output_near) {
		m += "\n\t near: ";
		int i;
		for (i = 0; i < 10; i++) {
			if (ss.peek() == EOF)
				break;
			m += ss.get();
		}
		for (; i >= 0; i--) {
			ss.unget();
		}
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
	std::cout << "INFO: " << msg << std::endl;
}

void _anal_debug(std::string pos, std::string tok_name, std::string msg)
{
	std::cout << "DEBUG: " << pos << " " << tok_name << std::endl;
	if (msg.length() > 0) {
		std::cout << "\tMSG: " << msg << std::endl;
	}
}

}