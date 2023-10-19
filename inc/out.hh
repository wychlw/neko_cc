#pragma once

#include <string>
#include <sstream>

namespace neko_cc
{

using stream = std::basic_iostream<char>;

class not_implemented : public std::logic_error
{
    public:
    not_implemented() : std::logic_error("not implemented") {}
};

void error(std::string msg);
void error(std::string msg, stream &ss, bool output_near = false);
void info(std::string msg);

}