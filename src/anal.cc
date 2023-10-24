#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cwchar>
#include <deque>

#include "scan.hh"
#include "tok.hh"
#include "out.hh"
#include "anal.hh"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace neko_cc
{
using std::make_shared;
}

namespace neko_cc
{

static std::deque<tok_t> tok_buf;
static std::unordered_map<size_t, type_t> glbl_type_map;

static tok_t nxt_tok(stream &ss)
{
	if (tok_buf.empty()) {
		tok_buf.push_back(scan(ss));
	}
	return tok_buf.front();
}

static tok_t get_tok(stream &ss)
{
	tok_t t = nxt_tok(ss);
	tok_buf.pop_front();
	return t;
}

static void unget_tok(tok_t tok)
{
	tok_buf.push_front(tok);
}

static void match(int tok, stream &ss)
{
	tok_t t = get_tok(ss);
	if (t.type != tok) {
		error(std::string("expected ") + back_tok_map(tok) +
			      " but got " + t.str,
		      ss, true);
	}
}

static size_t get_label()
{
	static size_t label_cnt = 0;
	return ++label_cnt;
}

static std::string get_unnamed_var_name()
{
	static size_t unnamed_var_cnt = 0;
	return std::string("unnamed_var_") + std::to_string(++unnamed_var_cnt);
}

static std::string get_ptr_type_name(std::string base_name)
{
	return base_name + "_ptr";
}

static std::string get_func_type_name(std::string base_name)
{
	return base_name + "_rfunc";
}

}

namespace neko_cc
{

/*
translation_unit
	{external_declaration}+
*/
void translation_unit(stream &ss)
{
	anal_debug();

	context_t ctx(nullptr, {}, {}, 0);

	while (nxt_tok(ss).type != tok_eof) {
		external_declaration(ss, ctx);
	}
}

/*
external_declaration
	function_definition | declaration
	// here the LL is breaking down, so we delay the decision after 
	// we meet the first declarator, then check whether it follows
	// a '{' or not

	// so actually this function also contain function-definition
	// and part of declaration

function-definition:
	declaration_specifiers declarator compound_statement

declaration
	declaration_specifiers {declarator {'=' initializer}? {',' init_declarator}*}? ';'
*/
void external_declaration(stream &ss, context_t &ctx)
{
	anal_debug();

	type_t type;
	// declaration_specifiers
	declaration_specifiers(ss, ctx, type);
	if (type.type == type_t::type_unknown) {
		error("Type specifier expected", ss, true);
	}
	if (type.is_register) {
		error("Register is not supported at top level declaration", ss,
		      true);
	}

	// to handel nothing is declared
	if (nxt_tok(ss).type == ';') {
		match(';', ss);
		return;
	}

	// declarator
	var_t var;
	std::vector<var_t> args =
		declarator(ss, ctx, make_shared<type_t>(type), var);

	// now if next is an '{', then it is a function definition
	if (nxt_tok(ss).type == '{') {
		// function_definition
		// as function definition only can appera at here, we let this
		// function to handel the previous bnf
		function_definition(ss, ctx, args);
	} else {
		// declaration
		// however, for a declaration, we must create a special function
		// to handel the rest part here
		top_declaration(ss, ctx, type, var);
	}
}

/*
function-definition:
	declaration_specifiers declarator compound_statement
*/
void function_definition(stream &ss, context_t &ctx, std::vector<var_t> args)
{
	anal_debug();

	if (nxt_tok(ss).type != '{') {
		error("Compound statement expected", ss, true);
	}
	context_t ctx_func(&ctx, {}, {}, 0);
	for (auto i : args) {
		ctx_func.vars[i.name] = i;
	}
	compound_statement(ss, ctx);
}

/*
constant_expression
	conditional_expression
*/
int constant_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	if (nxt_tok(ss).type == tok_int_lit) {
		tok_t tok = get_tok(ss);
		return std::stoi(tok.str);
	}
	if (nxt_tok(ss).type == tok_ident) {
		tok_t tok = get_tok(ss);
		int enum_val = ctx.get_enum(tok.str);
		if (enum_val == -1) {
			error("For now, constant ony support int lit or enum val",
			      ss, true);
		}
		return enum_val;
	}
	error("For now, constant ony support int lit or enum val", ss, true);
	return -1;
}

/*
declaration
	declaration_specifiers {init_declarator {',' init_declarator}*}? ';'
*/

/*
top_declaration
	{'=' initializer} {',' init_declarator}*}? ';'
*/
static void top_declaration(stream &ss, context_t &ctx, type_t type, var_t var)
{
	anal_debug();

	if (nxt_tok(ss).type == '=') {
		match('=', ss);
		initializer(ss, ctx, var);
	}
	while (nxt_tok(ss).type == ',') {
		match(',', ss);
		init_declarator(ss, ctx, type);
	}
	match(';', ss);
}

/*
declaration_specifiers
	{storage_class_specifier type_specifier type_qualifier}*
    // must have type_specifier
*/
void declaration_specifiers(stream &ss, context_t &ctx, type_t &type)
{
	anal_debug();

	while (is_strong_class_specifier(nxt_tok(ss)) ||
	       is_type_specifier(nxt_tok(ss), type) ||
	       is_type_qualifier(nxt_tok(ss))) {
		if (is_strong_class_specifier(nxt_tok(ss))) {
			strong_class_specfifer(ss, ctx, type);
		} else if (is_type_specifier(nxt_tok(ss), type)) {
			type_specifier(ss, ctx, type);
		} else if (is_type_qualifier(nxt_tok(ss))) {
			type_qualifier(ss, ctx, type);
		}
	}
	if (type.type == type_t::type_unknown) {
		error("Type specifier expected", ss, true);
	}
	if (type.type == type_t::type_basic) {
		try_regulate_basic(ss, type);
	}
}

/*
init_declarator
	declarator {'=' initializer}?
*/
void init_declarator(stream &ss, context_t &ctx, type_t type)
{
	anal_debug();
}

/*
storage_class_specifier
	{TYPEDEF | EXTERN | STATIC | AUTO | REGISTER}
*/
static bool is_strong_class_specifier(tok_t tok)
{
	return tok.type == tok_typedef || tok.type == tok_extern ||
	       tok.type == tok_static || tok.type == tok_auto ||
	       tok.type == tok_register;
}
void strong_class_specfifer(stream &ss, context_t &ctx, type_t &type)
{
	anal_debug();

	if (!is_strong_class_specifier(nxt_tok(ss))) {
		error("Strong class specifier expected", ss, true);
	}
	if (type.is_extern || type.is_static ||
	    type.type == type_t::type_typedef || type.is_register) {
		error("Duplicate strong class specifier", ss, true);
	}

	tok_t tok = get_tok(ss);
	if (tok.type == tok_extern) {
		type.is_extern = true;
	} else if (tok.type == tok_static) {
		type.is_static = true;
	} else if (tok.type == tok_typedef) {
		type.type = type_t::type_typedef;
	} else if (tok.type == tok_register) {
		type.is_register = true;
	}
}

/*
type_specifier
	{VOID | CHAR | SHORT | INT | LONG | FLOAT | DOUBLE
	| SIGNED | UNSIGNED | struct_or_union_specifier
	| enum_specifier | TYPE_NAME}
*/
static bool is_type_specifier(tok_t tok, const type_t type)
{
	if (type.type == type_t::type_unknown) {
		return tok.type == tok_void || tok.type == tok_char ||
		       tok.type == tok_short || tok.type == tok_int ||
		       tok.type == tok_long || tok.type == tok_float ||
		       tok.type == tok_double || tok.type == tok_signed ||
		       tok.type == tok_unsigned || tok.type == tok_struct ||
		       tok.type == tok_union || tok.type == tok_enum ||
		       tok.type == tok_ident;
	}
	return tok.type == tok_void || tok.type == tok_char ||
	       tok.type == tok_short || tok.type == tok_int ||
	       tok.type == tok_long || tok.type == tok_float ||
	       tok.type == tok_double || tok.type == tok_signed ||
	       tok.type == tok_unsigned || tok.type == tok_struct ||
	       tok.type == tok_union || tok.type == tok_enum;
}
static void try_regulate_basic(stream &ss, type_t &type)
{
	if (type.has_signed && type.is_unsigned) {
		error("Invalid type specifier", ss, true);
	}
	if (type.is_char >= 2 || type.is_short >= 2 || type.is_int >= 2 ||
	    type.is_float >= 2 || type.is_double >= 2) {
		error("Duplicate type specifier", ss, true);
	}
	if (type.is_long >= 3 || (type.is_double >= 1 && type.is_long >= 2)) {
		error("Too long for a type", ss, true);
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
}
void type_specifier(stream &ss, context_t &ctx, type_t &type)
{
	anal_debug();

	if (!is_type_specifier(nxt_tok(ss))) {
		error("Type specifier expected", ss, true);
	}
	if (type.type != type_t::type_unknown) {
		error("Duplicate type specifier", ss, true);
	}

	tok_t tok;
	if (nxt_tok(ss).type == tok_void) {
		if (type.type != type_t::type_unknown &&
		    type.type != type_t::type_basic) {
			error("Duplicate type specifier", ss, true);
		}
		tok = get_tok(ss);
		type.type = type_t::type_basic;
	} else if (nxt_tok(ss).type == tok_char) {
		if (type.type != type_t::type_unknown) {
			error("Duplicate type specifier", ss, true);
		}
		tok = get_tok(ss);
		type.type = type_t::type_basic;
		type.is_char++;
	} else if (nxt_tok(ss).type == tok_short) {
		if (type.type != type_t::type_unknown &&
		    type.type != type_t::type_basic) {
			error("Duplicate type specifier", ss, true);
		}
		tok = get_tok(ss);
		type.type = type_t::type_basic;
		type.is_short++;
	} else if (nxt_tok(ss).type == tok_int) {
		if (type.type != type_t::type_unknown &&
		    type.type != type_t::type_basic) {
			error("Duplicate type specifier", ss, true);
		}
		tok = get_tok(ss);
		type.type = type_t::type_basic;
		type.is_int++;
	} else if (nxt_tok(ss).type == tok_long) {
		if (type.type != type_t::type_unknown &&
		    type.type != type_t::type_basic) {
			error("Duplicate type specifier", ss, true);
		}
		tok = get_tok(ss);
		type.type = type_t::type_basic;
		type.is_long++;
	} else if (nxt_tok(ss).type == tok_float) {
		if (type.type != type_t::type_unknown &&
		    type.type != type_t::type_basic) {
			error("Duplicate type specifier", ss, true);
		}
		tok = get_tok(ss);
		type.type = type_t::type_basic;
		type.is_float++;
	} else if (nxt_tok(ss).type == tok_double) {
		if (type.type != type_t::type_unknown &&
		    type.type != type_t::type_basic) {
			error("Duplicate type specifier", ss, true);
		}
		tok = get_tok(ss);
		type.type = type_t::type_basic;
		type.is_double++;
	} else if (nxt_tok(ss).type == tok_signed) {
		if (type.type != type_t::type_unknown &&
		    type.type != type_t::type_basic) {
			error("Duplicate type specifier", ss, true);
		}
		tok = get_tok(ss);
		type.type = type_t::type_basic;
		type.has_signed = true;
	} else if (nxt_tok(ss).type == tok_unsigned) {
		if (type.type != type_t::type_unknown &&
		    type.type != type_t::type_basic) {
			error("Duplicate type specifier", ss, true);
		}
		tok = get_tok(ss);
		type.type = type_t::type_basic;
		type.is_unsigned = true;
	} else if (nxt_tok(ss).type == tok_struct ||
		   nxt_tok(ss).type == tok_union) {
		if (type.type != type_t::type_unknown) {
			error("Duplicate type specifier", ss, true);
		}
		struct_or_union_specifier(ss, ctx, type);
	} else if (nxt_tok(ss).type == tok_enum) {
		if (type.type != type_t::type_unknown) {
			error("Duplicate type specifier", ss, true);
		}
		enum_specifier(ss, ctx, type);
	} else if (nxt_tok(ss).type == tok_ident) {
		tok = get_tok(ss);
		type_t prev_type = ctx.get_type(tok.str);
		if (prev_type.type == type_t::type_unknown) {
			error("Unknown type name", ss, true);
		}
		if (prev_type.type == type_t::type_basic &&
		    type.type == type_t::type_basic) {
			prev_type.has_signed |= type.has_signed;
			prev_type.is_unsigned |= type.is_unsigned;
			prev_type.is_char += type.is_char;
			prev_type.is_short += type.is_short;
			prev_type.is_int += type.is_int;
			prev_type.is_long += type.is_long;
			prev_type.is_float += type.is_float;
			prev_type.is_double += type.is_double;
		} else if (type.type != type_t::type_unknown) {
			error("Duplicate type specifier", ss, true);
		}
		prev_type.is_extern = type.is_extern;
		prev_type.is_static = type.is_static;
		prev_type.is_register = type.is_register;
		prev_type.is_const = type.is_const;
		prev_type.is_volatile = type.is_volatile;
		type = prev_type;
	}
}

/*
struct_or_union_specifier
	{STRUCT | UNION} {
                       {IDENTIFIER | '{' struct_declaration_list '}'} 
                     | {IDENTIFIER '{' struct_declaration_list '}'
                     }
*/
void struct_or_union_specifier(stream &ss, context_t &ctx, type_t &type)
{
	anal_debug();

	if (nxt_tok(ss).type != tok_struct && nxt_tok(ss).type != tok_union) {
		error("struct or union expected", ss, true);
	}

	tok_t tok;
	if (nxt_tok(ss).type == tok_struct) {
		match(tok_struct, ss);
		type.type = type_t::type_struct;
	} else if (nxt_tok(ss).type == tok_union) {
		match(tok_union, ss);
		type.type = type_t::type_union;
	}

	if (nxt_tok(ss).type != tok_ident && nxt_tok(ss).type != '{') {
		error("Identifier or '{' expected", ss, true);
	}

	if (nxt_tok(ss).type == tok_ident) {
		tok = get_tok(ss);
		type.name = tok.str;
		if (nxt_tok(ss).type == '{') {
			match('{', ss);
			struct_declaration_list(ss, ctx, type);
			match('}', ss);
			ctx.types[type.name] = type;
		} else {
			type_t prev_type = ctx.get_type(type.name);
			if (prev_type.type == type_t::type_unknown) {
				error("Unknown struct or union type", ss, true);
			}
			prev_type.is_extern = type.is_extern;
			prev_type.is_static = type.is_static;
			prev_type.is_register = type.is_register;
			prev_type.is_const = type.is_const;
			prev_type.is_volatile = type.is_volatile;
			type = prev_type;
		}
	} else if (nxt_tok(ss).type == '{') {
		type.name = get_unnamed_var_name();
		type.is_unnamed = true;
		match('{', ss);
		struct_declaration_list(ss, ctx, type);
		match('}', ss);
	}
}

/*
type_qualifier
	CONST | VOLATILE
*/
static bool is_type_qualifier(tok_t tok)
{
	return tok.type == tok_const || tok.type == tok_volatile;
}
void type_qualifier(stream &ss, context_t &ctx, type_t &type)
{
	anal_debug();

	if (!is_type_qualifier(nxt_tok(ss))) {
		error("Type qualifier expected", ss, true);
	}

	tok_t tok = get_tok(ss);
	if (tok.type == tok_const) {
		type.is_const = true;
	} else if (tok.type == tok_volatile) {
		type.is_volatile = true;
	}
}

/*
declarator
	{pointer}? direct_declarator

ret for functiion decl arg list
*/
std::vector<var_t> declarator(stream &ss, context_t &ctx,
			      std::shared_ptr<type_t> type, var_t &var)
{
	anal_debug();

	if (nxt_tok(ss).type == '*') {
		type = pointer(ss, ctx, type);
	}
	return direct_declarator(ss, ctx, type, var);
}

/*
direct_declarator
	{IDENTIFIER | '(' declarator ')'} 
    {
            '[' {constant_expression}? ']' 
          | '(' {parameter_type_list | identifier_list}? ')'
    }*

ret for function decl arg list
*/
std::vector<var_t> direct_declarator(stream &ss, context_t &ctx,
				     std::shared_ptr<type_t> type, var_t &var)
{
	anal_debug();

	std::vector<var_t> args;
	if (nxt_tok(ss).type == tok_ident) {
		tok_t tok = get_tok(ss);
		var.name = tok.str;
	} else if (nxt_tok(ss).type == '(') {
		match('(', ss);
		declarator(ss, ctx, type, var);
		match(')', ss);
	} else {
		error("Identifier or '(' expected", ss, true);
	}

	bool is_arr = false, is_func = false;
	while (nxt_tok(ss).type == '[' || nxt_tok(ss).type == '(') {
		if (nxt_tok(ss).type == '[') {
			if (is_func) {
				error("Function cannot return array", ss, true);
			}
			match('[', ss);
			is_arr = true;
			int arr_len = 0;
			if (nxt_tok(ss).type != ']') {
				constant_expression(ss, ctx);
			}
			std::shared_ptr<type_t> arr_type =
				make_shared<type_t>();
			arr_type->name = get_ptr_type_name(type->name);
			arr_type->type = type_t::type_array;
			arr_type->size = type->size * arr_len;
			arr_type->ptr_to = type;
			type = arr_type;
			match(']', ss);
		} else {
			if (is_arr) {
				error("Declared as array of functions", ss,
				      true);
			}
			if (is_func) {
				error("Function cannot return function type",
				      ss, true);
			}
			match('(', ss);
			is_func = true;
			std::vector<type_t> args_type;
			if (nxt_tok(ss).type != ')') {
				args = parameter_type_list(ss, ctx);
				for (auto i : args) {
					args_type.push_back(i.type);
				}
			}
			std::shared_ptr<type_t> func_type =
				make_shared<type_t>();
			func_type->name = type->name;
			type->name = get_func_type_name(type->name);
			func_type->type = type_t::type_func;
			func_type->size = 0;
			func_type->ret_type = type;
			func_type->args_type = args_type;
			type = func_type;
			match(')', ss);
		}
	}

	return args;
}

/*
pointer
	'*' {type_qualifier}* {pointer}?
*/
std::shared_ptr<type_t> pointer(stream &ss, context_t &ctx,
				std::shared_ptr<type_t> type)
{
	anal_debug();

	if (nxt_tok(ss).type != '*') {
		error("'*' expected", ss, true);
	}
	match('*', ss);

	std::shared_ptr<type_t> ptr_type = make_shared<type_t>();
	ptr_type->name = get_ptr_type_name(type->name);
	ptr_type->type = type_t::type_pointer;
	ptr_type->size = 8;
	ptr_type->ptr_to = type;

	while (is_type_qualifier(nxt_tok(ss))) {
		type_qualifier(ss, ctx, *ptr_type);
	}

	if (nxt_tok(ss).type == '*') {
		ptr_type = pointer(ss, ctx, ptr_type);
	}
	return ptr_type;
}

/*
parameter_type_list
	parameter_list {',' '...'}?
*/
std::vector<var_t> parameter_type_list(stream &ss, context_t &ctx)
{
	anal_debug();

	std::vector<var_t> args;
	return args;
}

/*
initializer
	{assignment_expression} | '{' initializer_list {','}? '}'
*/
void initializer(stream &ss, context_t &ctx, var_t &var)
{
	anal_debug();
}

/*
struct_declaration_list
	{struct_declaration}+
*/
void struct_declaration_list(stream &ss, context_t &ctx, type_t &type)
{
	anal_debug();
	// while (nxt_tok(ss).type != '}') {
	// 	struct_declaration(ss, ctx, type);
	// }
}

void enum_specifier(stream &ss, context_t &ctx, type_t &type)
{
	anal_debug();

	if (nxt_tok(ss).type != tok_enum) {
		error("enum expected", ss, true);
	}
	match(tok_enum, ss);
}

/*
compound_statement
	'{' {declaration_list | statement_list}* '}'
*/
void compound_statement(stream &ss, context_t &ctx)
{
	anal_debug();

	match('{', ss);
	match('}', ss);
}

}