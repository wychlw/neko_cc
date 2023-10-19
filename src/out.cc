#include <iostream>

#include "out.hh"

namespace neko_cc
{

void error(std::string msg, stream &ss, bool output_near)
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

void info(std::string msg)
{
	std::cout <<"INFO: "<< msg << std::endl;
}

}