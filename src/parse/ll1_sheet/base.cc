#include "parse/ll1_sheet/base.hh"

namespace neko_cc
{

std::unordered_map<string, int> comp_predefine_tok = {
	{ "tok_ident", tok_ident },
	{ "tok_int_lit", tok_int_lit },
	{ "tok_float_lit", tok_float_lit },
	{ "tok_string_lit", tok_string_lit },
	{ "tok_null", tok_null },

	{ "tok_char", tok_char },
	{ "tok_short", tok_short },
	{ "tok_int", tok_int },
	{ "tok_signed", tok_signed },
	{ "tok_unsigned", tok_unsigned },
	{ "tok_long", tok_long },
	{ "tok_void", tok_void },
	{ "tok_float", tok_float },
	{ "tok_double", tok_double },
	{ "tok_const", tok_const },
	{ "tok_register", tok_register },
	{ "tok_auto", tok_auto },
	{ "tok_static", tok_static },
	{ "tok_extern", tok_extern },
	{ "tok_inline", tok_inline },
	{ "tok_restrict", tok_restrict },
	{ "tok_volatile", tok_volatile },
	{ "tok_if", tok_if },
	{ "tok_else", tok_else },
	{ "tok_while", tok_while },
	{ "tok_break", tok_break },
	{ "tok_return", tok_return },
	{ "tok_for", tok_for },
	{ "tok_goto", tok_goto },
	{ "tok_do", tok_do },
	{ "tok_continue", tok_continue },
	{ "tok_switch", tok_switch },
	{ "tok_case", tok_case },
	{ "tok_default", tok_default },
	{ "tok_struct", tok_struct },
	{ "tok_union", tok_union },
	{ "tok_enum", tok_enum },
	{ "tok_typedef", tok_typedef },
	{ "tok_sizeof", tok_sizeof },

	{ "+", '+' },
	{ "-", '-' },
	{ "*", '*' },
	{ "/", '/' },
	{ "%", '%' },
	{ "=", '=' },
	{ "!", '!' },
	{ ">", '>' },
	{ "<", '<' },
	{ "&", '&' },
	{ "|", '|' },
	{ "^", '^' },
	{ "~", '~' },
	{ "?", '?' },
	{ ":", ':' },
	{ ",", ',' },
	{ "(", '(' },
	{ ")", ')' },
	{ "{", '{' },
	{ "}", '}' },
	{ "[", '[' },
	{ "]", ']' },
	{ ";", ';' },
	{ ".", '.' },
	{ "\\", '\\' },
	{ "tok_pointer", tok_pointer },
	{ "tok_inc", tok_inc },
	{ "tok_dec", tok_dec },
	{ "tok_lshift", tok_lshift },
	{ "tok_rshift", tok_rshift },
	{ "tok_le", tok_le },
	{ "tok_ge", tok_ge },
	{ "tok_eq", tok_eq },
	{ "tok_ne", tok_ne },
	{ "tok_land", tok_land },
	{ "tok_lor", tok_lor },
	{ "tok_add_assign", tok_add_assign },
	{ "tok_sub_assign", tok_sub_assign },
	{ "tok_mul_assign", tok_mul_assign },
	{ "tok_div_assign", tok_div_assign },
	{ "tok_mod_assign", tok_mod_assign },
	{ "tok_lshift_assign", tok_lshift_assign },
	{ "tok_rshift_assign", tok_rshift_assign },
	{ "tok_and_assign", tok_and_assign },
	{ "tok_xor_assign", tok_xor_assign },
	{ "tok_or_assign", tok_or_assign },
	{ "tok_va_arg", tok_va_arg },

	{ "tok_eof", tok_eof }
};

}