/**
 * @file tok.hh
 * @author 泠妄 (lingwang@wcysite.com)
 * @brief Define language token
 * @version 0.1
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#pragma once

#include <unordered_map>
#include <string>

namespace neko_cc
{

enum tok_enum {
	tok_enum_begin = 256,

	// literals
	tok_ident,
	tok_int_lit,
	tok_float_lit,
	tok_string_lit,
	tok_null,

	// keywords
	tok_keywords_begin,
	tok_char,
	tok_short,
	tok_int,
	tok_signed,
	tok_unsigned,
	tok_long,
	tok_void,
	tok_float,
	tok_double,
	tok_const,
	tok_register,
	tok_auto,
	tok_static,
	tok_extern,
	tok_inline,
	tok_restrict,
	tok_volatile,

	tok_if,
	tok_else,
	tok_while,
	tok_break,
	tok_return,
	tok_for,
	tok_goto,
	tok_do,
	tok_continue,
	tok_switch,
	tok_case,
	tok_default,

	tok_struct,
	tok_union,
	tok_enum,
	tok_typedef,
	tok_sizeof,

	tok_keywords_end,

	// operators
	tok_operators_begin,
	tok_pointer,
	tok_inc,
	tok_dec,
	tok_lshift,
	tok_rshift,
	tok_le,
	tok_ge,
	tok_eq,
	tok_ne,
	tok_land,
	tok_lor,
	tok_add_assign,
	tok_sub_assign,
	tok_mul_assign,
	tok_div_assign,
	tok_mod_assign,
	tok_lshift_assign,
	tok_rshift_assign,
	tok_and_assign,
	tok_xor_assign,
	tok_or_assign,
	tok_va_arg,

	tok_operators_end,

	// properties

	tok_eof,
	tok_empty,
	tok_enum_end
};

extern std::unordered_map<std::string, int> tok_map;

/**
 * @brief Get the represented string of a given type
 * 
 * @param type Given type
 * @return std::string Represented string
 */
std::string back_tok_map(int type);

}