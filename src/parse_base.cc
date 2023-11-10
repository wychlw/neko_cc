/**
 * @file parse_base.cc
 * @author 泠妄 (lingwang@wcysite.com)
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#include <deque>

#include "parse.hh"
#include "out.hh"

namespace neko_cc
{
std::deque<tok_t> tok_buf;

tok_t nxt_tok(stream &ss)
{
	if (tok_buf.empty()) {
		tok_buf.push_back(scan(ss));
	}
	return tok_buf.front();
}

tok_t get_tok(stream &ss)
{
	tok_t t = nxt_tok(ss);
	tok_buf.pop_front();
	info("GOT TOKEN: " + t.str);
	return t;
}

void unget_tok(tok_t tok)
{
	tok_buf.push_front(tok);
}

void match(int tok, stream &ss)
{
	tok_t t = get_tok(ss);
	if (t.type != tok) {
		error(std::string("expected ") + back_tok_map(tok) +
			      " but got " + t.str,
		      ss, true);
	}
}

std::string get_label()
{
	static size_t label_cnt = 0;
	return "L" + std::to_string(++label_cnt);
}

std::string get_unnamed_var_name()
{
	static size_t unnamed_var_cnt = 0;
	return std::string("unnamed_var_") + std::to_string(++unnamed_var_cnt);
}

std::string get_ptr_type_name(std::string base_name)
{
	return base_name + "_ptr";
}

std::string get_func_type_name(std::string base_name)
{
	return base_name + "_rfunc";
}

bool is_declaration_specifiers(tok_t tok, context_t &ctx)
{
	return is_strong_class_specifier(tok) || is_type_qualifier(tok) ||
	       is_type_specifier(tok, ctx);
}

bool is_strong_class_specifier(tok_t tok)
{
	return tok.type == tok_typedef || tok.type == tok_extern ||
	       tok.type == tok_static || tok.type == tok_auto ||
	       tok.type == tok_register;
}

bool is_type_specifier(tok_t tok, context_t &ctx)
{
	if (tok.type == tok_ident) {
		type_t type = ctx.get_type(tok.str);
		if (type.type != type_t::type_unknown) {
			return true;
		}
	}
	return tok.type == tok_void || tok.type == tok_char ||
	       tok.type == tok_short || tok.type == tok_int ||
	       tok.type == tok_long || tok.type == tok_float ||
	       tok.type == tok_double || tok.type == tok_signed ||
	       tok.type == tok_unsigned || tok.type == tok_struct ||
	       tok.type == tok_union || tok.type == tok_enum;
}

bool is_type_void(const type_t &type)
{
	if (type.type != type_t::type_basic) {
		return false;
	}
	if (!type.is_bool && !type.is_char && !type.is_short && !type.is_int &&
	    !type.is_long && !type.is_float && !type.is_double) {
		return true;
	}
	return false;
}
bool is_type_i(const type_t &type)
{
	if (type.type != type_t::type_basic) {
		return false;
	}
	if (type.is_float || type.is_double || is_type_void(type)) {
		return false;
	}
	return true;
}
bool is_type_f(const type_t &type)
{
	if (type.type != type_t::type_basic) {
		return false;
	}
	return type.is_float || type.is_double;
}
bool is_type_p(const type_t &type)
{
	return type.type == type_t::type_pointer ||
	       type.type == type_t::type_array;
}
void try_regulate_basic(stream &ss, type_t &type)
{
	if (type.has_signed && type.is_unsigned) {
		error("Invalid type specifier", ss, true);
	}
	if (type.is_bool >= 2 || type.is_char >= 2 || type.is_short >= 2 ||
	    type.is_int >= 2 || type.is_float >= 2 || type.is_double >= 2) {
		error("Duplicate type specifier", ss, true);
	}
	if (type.is_long >= 3 || (type.is_double >= 1 && type.is_long >= 2)) {
		error("Too long for a type", ss, true);
	}
	if (type.is_bool && (type.is_char || type.is_short || type.is_int ||
			     type.is_long || type.is_float || type.is_double)) {
		error("Duplicate type specifier", ss, true);
	}
	if (type.is_char && (type.is_short || type.is_int || type.is_long ||
			     type.is_float || type.is_double)) {
		error("Duplicate type specifier", ss, true);
	}
	if (type.is_short &&
	    (type.is_long || type.is_float || type.is_double)) {
		error("Duplicate type specifier", ss, true);
	}
	if ((type.is_int || type.is_long) &&
	    (type.is_float || type.is_double)) {
		error("Duplicate type specifier", ss, true);
	}
	if (type.is_float && type.is_double) {
		error("Duplicate type specifier", ss, true);
	}
	if (type.has_signed &&
	    !(type.is_char || type.is_short || type.is_int || type.is_long)) {
		type.is_int++;
	}
	type.has_signed = 0;
	if (type.is_short && type.is_int) {
		type.is_int = 0;
	}
	if (type.is_long && type.is_int) {
		type.is_int = 0;
	}

	if (type.is_bool) {
		type.size = 1;
		type.name = "bool";
	} else if (type.is_char) {
		type.size = 1;
		type.name = "char";
	} else if (type.is_short) {
		type.size = 2;
		type.name = "short";
	} else if (type.is_int) {
		type.size = 4;
		type.name = "int";
	} else if (type.is_long == 1 && !type.is_double) {
		type.size = 8;
		type.name = "long";
	} else if (type.is_long == 2) {
		type.size = 8;
		type.name = "long long";
	} else if (type.is_float) {
		type.size = 4;
		type.name = "float";
	} else if (type.is_double) {
		type.size = 8;
		type.name = "double";
	} else if (type.is_long == 1 && type.is_double) {
		type.size = 16;
		type.name = "long double";
	}
}

bool is_type_qualifier(tok_t tok)
{
	return tok.type == tok_const || tok.type == tok_volatile;
}

bool is_selection_statement(tok_t tok)
{
	return tok.type == tok_if || tok.type == tok_switch;
}

bool is_iteration_statement(tok_t tok)
{
	return tok.type == tok_while || tok.type == tok_do ||
	       tok.type == tok_for;
}

bool is_jump_statement(tok_t tok)
{
	return tok.type == tok_continue || tok.type == tok_break ||
	       tok.type == tok_return;
}

bool is_specifier_qualifier_list(tok_t tok, context_t &ctx)
{
	return is_type_specifier(tok, ctx) || is_type_qualifier(tok);
}

bool is_unary_operator(tok_t tok)
{
	return tok.type == '&' || tok.type == '*' || tok.type == '+' ||
	       tok.type == '-' || tok.type == '~' || tok.type == '!';
}

bool is_assignment_operatior(tok_t tok)
{
	if (tok.type == '=' || tok.type == tok_mul_assign ||
	    tok.type == tok_div_assign || tok.type == tok_mod_assign ||
	    tok.type == tok_add_assign || tok.type == tok_sub_assign ||
	    tok.type == tok_lshift_assign || tok.type == tok_rshift_assign ||
	    tok.type == tok_and_assign || tok.type == tok_xor_assign ||
	    tok.type == tok_or_assign) {
		return true;
	}
	return false;
}

bool is_postfix_expression_op(tok_t tok)
{
	return tok.type == '[' || tok.type == '(' || tok.type == '.' ||
	       tok.type == tok_pointer || tok.type == tok_inc ||
	       tok.type == tok_dec;
}

bool is_mul_op(tok_t tok)
{
	return tok.type == '*' || tok.type == '/' || tok.type == '%';
}

bool is_add_op(tok_t tok)
{
	return tok.type == '+' || tok.type == '-';
}

bool is_shift_op(tok_t tok)
{
	return tok.type == tok_lshift || tok.type == tok_rshift;
}

bool is_rel_op(tok_t tok)
{
	return tok.type == '<' || tok.type == '>' || tok.type == tok_le ||
	       tok.type == tok_ge;
}

bool is_eq_op(tok_t tok)
{
	return tok.type == tok_eq || tok.type == tok_ne;
}

bool should_conv_to_first(const var_t &v1, const var_t &v2)
{
	bool conv_v1 = false;
	if (is_type_i(*v1.type) && is_type_i(*v2.type)) {
		if (v1.type->size < v2.type->size) {
			conv_v1 = true;
		} else {
			conv_v1 = false;
		}
	} else if (is_type_f(*v1.type) && is_type_f(*v2.type)) {
		if (v1.type->size < v2.type->size) {
			conv_v1 = true;
		} else {
			conv_v1 = false;
		}
	} else if (is_type_p(*v1.type) && is_type_p(*v2.type)) {
		conv_v1 = false;
	} else if (is_type_p(*v1.type) && is_type_i(*v2.type)) {
		conv_v1 = true;
	} else if (is_type_i(*v1.type) && is_type_p(*v2.type)) {
		conv_v1 = false;
	} else if (is_type_f(*v1.type) && is_type_i(*v2.type)) {
		conv_v1 = false;
	} else if (is_type_i(*v1.type) && is_type_f(*v2.type)) {
		conv_v1 = true;
	} else {
		err_msg("Cannot convert between these types\n" + v1.str() +
			"\n" + v2.str());
	}
	return !conv_v1;
}
}