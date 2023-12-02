/**
 * @file tok.cc
 * @author 泠妄 (lingwang@wcysite.com)
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#include "tok.hh"

namespace neko_cc
{

std::unordered_map<std::string, int> tok_map = { { "NULL", tok_null },
						 { "char", tok_char },
						 { "short", tok_short },
						 { "int", tok_int },
						 { "signed", tok_signed },
						 { "unsigned", tok_unsigned },
						 { "long", tok_long },
						 { "void", tok_void },
						 { "float", tok_float },
						 { "double", tok_double },
						 { "const", tok_const },
						 { "register", tok_register },
						 { "auto", tok_auto },
						 { "static", tok_static },
						 { "extern", tok_extern },
						 { "inline", tok_inline },
						 { "restrict", tok_restrict },
						 { "volatile", tok_volatile },
						 { "if", tok_if },
						 { "else", tok_else },
						 { "while", tok_while },
						 { "break", tok_break },
						 { "return", tok_return },
						 { "for", tok_for },
						 { "goto", tok_goto },
						 { "do", tok_do },
						 { "continue", tok_continue },
						 { "switch", tok_switch },
						 { "case", tok_case },
						 { "default", tok_default },
						 { "struct", tok_struct },
						 { "union", tok_union },
						 { "enum", tok_enum },
						 { "typedef", tok_typedef },
						 { "sizeof", tok_sizeof },
						 // unused?
						 { "->", tok_pointer },
						 { "++", tok_inc },
						 { "--", tok_dec },
						 { "<<", tok_lshift },
						 { ">>", tok_rshift },
						 { "<=", tok_le },
						 { ">=", tok_ge },
						 { "==", tok_eq },
						 { "!=", tok_ne },
						 { "&&", tok_land },
						 { "||", tok_lor },
						 { "+=", tok_add_assign },
						 { "-=", tok_sub_assign },
						 { "*=", tok_mul_assign },
						 { "/=", tok_div_assign },
						 { "%=", tok_mod_assign },
						 { "<<=", tok_lshift_assign },
						 { ">>=", tok_rshift_assign },
						 { "&=", tok_and_assign },
						 { "^=", tok_xor_assign },
						 { "|", tok_or_assign },
						 { "..", tok_enum_begin },
						 { "...", tok_va_arg }

};

std::string back_tok_map(int type)
{
	if (type == tok_eof) {
		return "EOF";
	}
	if (type == tok_null) {
		return "NULL";
	}
	if (type == tok_empty) {
		return "EMPTY";
	}
	if (type == tok_ident) {
		return "ident";
	}
	if (type == tok_int_lit) {
		return "int lit";
	}
	if (type == tok_float_lit) {
		return "float lit";
	}
	if (type == tok_string_lit) {
		return "string lit";
	}
	for (auto [k, v] : tok_map) {
		if (v == type) {
			return k;
		}
	}
	return std::string{ (char)type };
}

}