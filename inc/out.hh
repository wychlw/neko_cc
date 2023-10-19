#pragma once

#include <string>
#include <sstream>

namespace neko_cc
{

using stream = std::basic_iostream<char>;

void error(std::string msg, stream &ss, bool output_near = false);
void info(std::string msg);

}