
#include <cstddef>
#include <cstdio>
#include <exception>

#include <string>
#include <unordered_set>

#include "scan.hh"
#include "out.hh"
#include "tok.hh"

namespace neko_cc
{

bool is_alpha(char ch)
{
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
	       ch == '_';
}

bool is_digit(char ch)
{
	return ch >= '0' && ch <= '9';
}

bool is_alnum(char ch)
{
	return is_alpha(ch) || is_digit(ch);
}

bool is_white(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static std::unordered_set<char> op_set = { '+', '-', '*', '/', '%', '=', '!',
					   '>', '<', '&', '|', '^', '~', '?',
					   ':', ',', '(', ')', '{', '}', '[',
					   ']', ';', '.', '\\' };
bool is_op(char ch)
{
	return op_set.count(ch) == 1;
}

bool is_comment(stream &ss)
{
	if (ss.peek() != '/') {
		return false;
	}
	ss.get();
	if (ss.peek() == '/' || ss.peek() == '*') {
		ss.unget();
		return true;
	}
	ss.unget();
	return false;
}

void skip_multiline_comment(stream &ss)
{
	ss.get();

	bool end = false;
	while (!end) {
		if (ss.peek() == EOF) {
			error("unterminated comment", ss, true);
		}
		if (ss.peek() == '*') {
			ss.get();
			if (ss.peek() == '/') {
				ss.get();
				end = true;
			}
		} else {
			ss.get();
		}
	}
	ss.get();
}

void skip_comment(stream &ss)
{
	while (is_comment(ss)) {
		ss.get();
		if (ss.peek() == '/') {
			while (ss.peek() != '\n') {
				ss.get();
			}
			ss.get();
		} else {
			skip_multiline_comment(ss);
		}
	}
}

void skip_white(stream &ss)
{
	while (is_white(ss.peek()) || is_comment(ss)) {
		if (is_comment(ss)) {
			skip_comment(ss);
		} else {
			ss.get();
		}
	}
}

void match_ss(char ch, stream &ss)
{
	if (ss.peek() != ch) {
		error(std::string("expected ") + ch, ss, true);
	}
	ss.get();
	skip_white(ss);
}

std::string get_name(stream &ss)
{
	std::string res = "";
	skip_white(ss);
	if (!is_alpha(ss.peek())) {
		error("name error", ss, true);
	}
	while (is_alnum(ss.peek())) {
		res += ss.get();
	}
	skip_white(ss);
	return res;
}

std::string get_num(stream &ss)
{
	std::string res = "";
	skip_white(ss);
	if (!is_digit(ss.peek())) {
		error("number error", ss, true);
	}
	while (is_digit(ss.peek())) {
		res += ss.get();
	}
	skip_white(ss);
	return res;
}

static std::string get_trans_char(stream &ss)
{
	std::string res = "";
	if (is_digit(ss.peek())) {
		int ch = 0;
		for (int i = 0; i < 3; i++) {
			if (is_digit(ss.peek())) {
				ch *= 8;
				ch += ss.get() - '0';
			} else {
				break;
			}
		}
		if (ch >= 256) {
			error("Too big for a char", ss, true);
		}
		res += ch;
	} else {
		res += ss.get();
	}
	return res;
}

std::string get_char(stream &ss)
{
	std::string res = "";
	match_ss('\'', ss);
	if (ss.peek() == '\\') {
		ss.get();
		res += get_trans_char(ss);
	} else {
		res += ss.get();
	}
	match_ss('\'', ss);
	return res;
}

std::string get_string(stream &ss)
{
	std::string res = "";
	match_ss('"', ss);
	while (ss.peek() != '"') {
		if (ss.peek() == EOF) {
			error("unterminated string", ss, true);
		}
		if (ss.peek() == '\\') {
			res += ss.get();
		}
		res += ss.get();
	}
	match_ss('"', ss);
	return res;
}

tok_t scan(stream &ss)
{
	skip_white(ss);
	std::string str;
	int type = tok_enum_end;

	if (is_alpha(ss.peek())) {
		str = get_name(ss);
		if (tok_map.count(str) == 0) {
			type = tok_ident;
		} else {
			type = tok_map[str];
		}
		return { type, str };
	}

	if (is_digit(ss.peek())) {
		str = get_num(ss);
		type = tok_int_lit;
	}
	if (ss.peek() == '.' && (type == tok_int_lit || type == tok_enum_end)) {
		ss.get();
		std::string float_part = "";
		if (is_digit(ss.peek())) {
			float_part = get_num(ss);
			type = tok_float_lit;
			str = str + '.' + float_part;
		} else {
			ss.unget();
		}
	}
	if (type == tok_int_lit || type == tok_float_lit) {
		return { type, str };
	}

	if (ss.peek() == '\'') {
		str = get_char(ss);
		type = tok_int_lit;
		return { type, str };
	}

	if (ss.peek() == '"') {
		str = get_string(ss);
		type = tok_string_lit;
		return { type, str };
	}

	if (ss.peek() == EOF) {
		return { tok_eof, "" };
	}

	if (!is_op(ss.peek())) {
		error(std::string("unknown character: ") + (char)ss.peek(), ss,
		      true);
	}
	str += ss.get();
	while (is_op(ss.peek()) && tok_map.count(str + (char)ss.peek()) == 1) {
		str += ss.get();
	}
	if (str.length() == 1) {
		type = str[0];
	} else {
		type = tok_map[str];
	}
	return { type, str };
}

void unscan(stream &ss, tok_t tok)
{
	for (size_t i = 0; i < tok.str.length(); i--) {
		ss.unget();
	}
}

void unscan(stream &ss, size_t len)
{
	while (len--) {
		ss.unget();
	}
}

}