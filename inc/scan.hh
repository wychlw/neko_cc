/**
 * @file scan.hh
 * @author 泠妄 (lingwang@wcysite.com)
 * @brief Lexal Scanning
 * @version 0.1
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

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

/**
 * @brief Judge if a given char is in ([a-zA-Z_])
 * 
 * @param ch given char
 * @return Judge result
 */
bool is_alpha(char ch);

/**
 * @brief Judge if a given char is in ([0-9])
 * 
 * @param ch given char
 * @return Judge result
 */
bool is_digit(char ch);

/**
 * @brief Judge if a given char is in ([a-zA-Z0-9_])
 * 
 * @param ch given char
 * @return Judge result
 */
bool is_alnum(char ch);

/**
 * @brief Judge if a given char is an operator
 * 
 * @param ch given char
 * @return Judge result
 */
bool is_op(char ch);

/**
 * @brief Judge if a given char is a white chararacter
 * The white character includes:
 *	space and tab
 *	'\r' and '\n' (we consider a new line is also part of the white char)
 *
 * @param ch given char
 * @return Judge result
 */
bool is_white(char ch);

/**
 * @brief Judge does the input stream hits a comment
 * 
 * @param ss Input stream
 */
bool is_comment(stream &ss);

/**
 * @brief Skip a *series* of white character **and comments**
 * 
 * @param ss Input stream
 */
void skip_white(stream &ss);

/**
 * @brief Match and read a given character.
 * 
 * @exception If the head of the stream doesn't match the 
 * target character,throw an expetation error.
 * 
 * @param ch Target character
 * @param ss Input stream
 */
void match_ss(char ch, stream &ss);

/**
 * @brief Get an ident
 * 
 * @param ss Input stream
 * @return std::string Ident
 */
std::string get_name(stream &ss);

/**
 * @brief Get a number
 * 
 * @param ss Input stream
 * @return std::string Number val (in 10-base system)
 */
std::string get_num(stream &ss);

/**
 * @brief Get a char (include transation char)
 * 
 * @param ss Input stream
 * @return std::string Given char
 */
std::string get_char(stream &ss);

/**
 * @brief Get a string lit
 * 
 * @param ss Input string
 * @return std::string Given string, include trans char
 */
std::string get_string(stream &ss);

/**
 * @brief Get the next token
 * 
 * @param ss Input stream
 * @return tok_t Got token
 */
tok_t scan(stream &ss);

/**
 * @brief Unget a given token
 * 
 * @param ss Input stream
 * @param tok The token want to unget
 */
void unscan(stream &ss, tok_t tok);

/**
 * @brief Unget a serie of character
 * 
 * @param ss Input stream
 * @param len How many character want to unget
 */
void unscan(stream &ss, size_t len);

}