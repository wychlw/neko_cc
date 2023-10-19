#pragma once

#include <istream>
#include <string>
#include <ios>

#include "tok.hh"

namespace neko_cc
{

struct tok_t {
	int type;
	std::string str;
};

using stream = std::basic_iostream<char>;

bool is_alpha(char ch);
bool is_digit(char ch);
bool is_alnum(char ch);
bool is_op(char ch);
bool is_white(char ch);

void skip_white(stream &ss);
void match_ss(char ch, stream &ss);
std::string get_name(stream &ss);
std::string get_num(stream &ss);
std::string get_char(stream &ss);
std::string get_string(stream &ss);
tok_t scan(stream &ss);

}