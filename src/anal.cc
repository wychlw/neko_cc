
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cwchar>
#include <stack>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "scan.hh"
#include "tok.hh"
#include "out.hh"
#include "anal.hh"
#include "gen.hh"

namespace neko_cc
{
using std::make_shared;
}

namespace neko_cc
{

std::deque<tok_t> tok_buf;
std::unordered_map<size_t, type_t> glbl_type_map;

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

string get_label()
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

	context_t ctx(nullptr, {}, {});

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

	std::stringstream sss;
	sss << var;
	std::cout << sss.str() << std::endl;

	// now if next is an '{', then it is a function definition
	if (nxt_tok(ss).type == '{') {
		// function_definition
		// as function definition only can appera at here, we let this
		// function to handel the previous bnf
		function_definition(ss, ctx, var, args);
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
void function_definition(stream &ss, context_t &ctx, var_t function,
			 std::vector<var_t> args)
{
	anal_debug();

	if (nxt_tok(ss).type != '{') {
		error("Compound statement expected", ss, true);
	}

	reset_vreg();

	function.name = '@' + function.name;
	var_t func_var;
	type_t func_var_type;
	func_var.name = function.name;
	func_var_type.type = type_t::type_pointer;
	func_var_type.ptr_to = function.type;
	func_var.type = make_shared<type_t>(func_var_type);
	func_var.is_alloced =
		false; /* !important, or this ptr will be derefrence during the calculation */
	ctx.vars[func_var.name] = func_var;

	context_t ctx_func(&ctx, {}, {});
	std::vector<var_t> input_args;
	for (auto &i : args) {
		var_t arg_var;
		arg_var.name = get_vreg();
		arg_var.type = i.type;

		i.name = '%' + i.name;

		input_args.push_back(arg_var);
	}
	fun_env_t fun_env;
	fun_env.is_func = 1;
	fun_env.ret_type = *function.type->ret_type;
	ctx_func.fun_env = make_shared<fun_env_t>(fun_env);
	emit_func_begin(function, input_args);
	for (size_t i = 0; i < args.size(); i++) {
		args[i] = emit_alloc(*input_args[i].type, args[i].name);
		emit_store(args[i], input_args[i]);
		ctx.vars[args[i].name] = args[i];
	}
	compound_statement(ss, ctx);
	if (function.type->ret_type->type == type_t::type_basic) {
		if (is_type_void(*function.type->ret_type)) {
			emit_line("ret void");
		} else {
			emit_line("ret " +
				  get_type_repr(*function.type->ret_type) +
				  " zeroinitializer");
		}
	} else {
		emit_line("ret " + get_type_repr(*function.type->ret_type) +
			  " zeroinitializer");
	}
	emit_func_end();
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
void declaration(stream &ss, context_t &ctx)
{
	anal_debug();

	type_t type;
	declaration_specifiers(ss, ctx, type);
	if (type.type == type_t::type_unknown) {
		error("Type specifier expected", ss, true);
	}

	if (nxt_tok(ss).type == ';') {
		match(';', ss);
		return;
	}

	init_declarator(ss, ctx, type);
	while (nxt_tok(ss).type == ',') {
		match(',', ss);
		init_declarator(ss, ctx, type);
	}
	match(';', ss);
}

/*
top_declaration
	{'=' initializer} {',' init_declarator}*}? ';'
*/
void top_declaration(stream &ss, context_t &ctx, type_t type, var_t var)
{
	anal_debug();

	if (is_type_void(type)) {
		error("Cannot declare void type variable", ss, true);
	}
	var.name = '@' + var.name;
	ctx.vars[var.name] = var;

	if (nxt_tok(ss).type == '=') {
		match('=', ss);
		initializer(ss, ctx, var);
	} else {
		emit_global_decl(var);
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
bool is_declaration_specifiers(tok_t tok, context_t &ctx)
{
	return is_strong_class_specifier(tok) || is_type_qualifier(tok) ||
	       is_type_specifier(tok, ctx);
}
void declaration_specifiers(stream &ss, context_t &ctx, type_t &type)
{
	anal_debug();

	while (is_strong_class_specifier(nxt_tok(ss)) ||
	       is_type_specifier(nxt_tok(ss), ctx) ||
	       is_type_qualifier(nxt_tok(ss))) {
		if (is_strong_class_specifier(nxt_tok(ss))) {
			strong_class_specfifer(ss, ctx, type);
		} else if (is_type_specifier(nxt_tok(ss), ctx)) {
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

	var_t var;
	std::vector<var_t> args =
		declarator(ss, ctx, make_shared<type_t>(type), var);
	ctx.vars[var.name] = var;
	if (nxt_tok(ss).type == '=') {
		match('=', ss);
		initializer(ss, ctx, var);
	}
}

/*
storage_class_specifier
	{TYPEDEF | EXTERN | STATIC | AUTO | REGISTER}
*/
bool is_strong_class_specifier(tok_t tok)
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
	if (!type.is_char && !type.is_short && !type.is_int && !type.is_long &&
	    !type.is_float && !type.is_double) {
		return true;
	}
	return false;
}
bool is_type_i(const type_t &type)
{
	if (type.type != type_t::type_basic) {
		return false;
	}
	if (type.is_bool || type.is_float || type.is_double ||
	    is_type_void(type)) {
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
void type_specifier(stream &ss, context_t &ctx, type_t &type)
{
	anal_debug();

	if (!is_type_specifier(nxt_tok(ss), ctx)) {
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
bool is_type_qualifier(tok_t tok)
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
	bool hit_ident = false;
	if (nxt_tok(ss).type == tok_ident) {
		tok_t tok = get_tok(ss);
		var.name = tok.str;
		hit_ident = true;
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
				arr_len = constant_expression(ss, ctx);
				std::shared_ptr<type_t> arr_type =
					make_shared<type_t>();
				arr_type->name = get_ptr_type_name(type->name);
				arr_type->type = type_t::type_array;
				arr_type->size = type->size * arr_len;
				arr_type->ptr_to = make_shared<type_t>(*type);
				*type = *arr_type;
			} else {
				std::shared_ptr<type_t> arr_type =
					make_shared<type_t>();
				arr_type->name = get_ptr_type_name(type->name);
				arr_type->type = type_t::type_pointer;
				arr_type->size = 0;
				arr_type->ptr_to = make_shared<type_t>(*type);
				*type = *arr_type;
			}

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
					args_type.push_back(*(i.type));
				}
			}
			std::shared_ptr<type_t> func_type =
				make_shared<type_t>();
			func_type->name = "func_" + type->name;
			func_type->type = type_t::type_func;
			func_type->size = 0;
			func_type->ret_type = make_shared<type_t>(*type);
			func_type->args_type = args_type;
			*type = *func_type;
			match(')', ss);
		}
	}

	if (hit_ident) {
		var.type = type;
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
abstract_declarator
	{pointer}* direct_abstract_declarator

ret for functiion decl arg list
*/
std::vector<var_t> abstract_declarator(stream &ss, context_t &ctx,
				       std::shared_ptr<type_t> type, var_t &var)
{
	anal_debug();

	if (nxt_tok(ss).type == '*') {
		type = pointer(ss, ctx, type);
	}
	return direct_abstract_declarator(ss, ctx, type, var);
}

/*
direct_abstract_declarator
	: {IDENTIFIER}?
	| '(' abstract_declarator ')'
	| '[' ']'
	| '[' constant_expression ']'
	| direct_abstract_declarator '[' ']'
	| direct_abstract_declarator '[' constant_expression ']'
	| '(' ')'
	| '(' parameter_type_list ')'
	| direct_abstract_declarator '(' ')'
	| direct_abstract_declarator '(' parameter_type_list ')'

ret for function decl arg list
*/
bool chk_if_abstract_nested(stream &ss, context_t &ctx)
{
	if (nxt_tok(ss).type != '(') {
		return false;
	}
	std::stack<tok_t> store;
	store.push(get_tok(ss));
	if (nxt_tok(ss).type == ')') {
		return false;
	}
	if (is_declaration_specifiers(nxt_tok(ss), ctx)) {
		return false;
	}
	while (!store.empty()) {
		unget_tok(store.top());
		store.pop();
	}
	return true;
}
std::vector<var_t> direct_abstract_declarator(stream &ss, context_t &ctx,
					      std::shared_ptr<type_t> type,
					      var_t &var)
{
	anal_debug();

	std::vector<var_t> args;
	bool hit_ident = false;
	if (nxt_tok(ss).type == tok_ident) {
		tok_t tok = get_tok(ss);
		var.name = tok.str;
		hit_ident = true;
	} else if (nxt_tok(ss).type == '(' && chk_if_abstract_nested(ss, ctx)) {
		match('(', ss);
		abstract_declarator(ss, ctx, type, var);
		match(')', ss);
	} else {
		var.name = get_unnamed_var_name();
		hit_ident = true;
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
				arr_len = constant_expression(ss, ctx);
				std::shared_ptr<type_t> arr_type =
					make_shared<type_t>();
				arr_type->name = get_ptr_type_name(type->name);
				arr_type->type = type_t::type_array;
				arr_type->size = type->size * arr_len;
				arr_type->ptr_to = make_shared<type_t>(*type);
				*type = *arr_type;
			} else {
				std::shared_ptr<type_t> arr_type =
					make_shared<type_t>();
				arr_type->name = get_ptr_type_name(type->name);
				arr_type->type = type_t::type_pointer;
				arr_type->size = 0;
				arr_type->ptr_to = make_shared<type_t>(*type);
				*type = *arr_type;
			}
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
					args_type.push_back(*(i.type));
				}
			}
			std::shared_ptr<type_t> func_type =
				make_shared<type_t>();
			func_type->name = "func_" + type->name;
			func_type->type = type_t::type_func;
			func_type->size = 0;
			func_type->ret_type = make_shared<type_t>(*type);
			func_type->args_type = args_type;
			*type = *func_type;
			match(')', ss);
		}
	}

	if (hit_ident) {
		var.type = type;
	}

	return args;
}

/*
parameter_type_list
	parameter_list
*/
std::vector<var_t> parameter_type_list(stream &ss, context_t &ctx)
{
	anal_debug();

	return parameter_list(ss, ctx);
}

/*
parameter_list
	parameter_declaration {',' parameter_declaration}*
*/
std::vector<var_t> parameter_list(stream &ss, context_t &ctx)
{
	anal_debug();

	std::vector<var_t> args;
	args.push_back(parameter_declaration(ss, ctx));
	while (nxt_tok(ss).type == ',') {
		match(',', ss);
		args.push_back(parameter_declaration(ss, ctx));
	}
	return args;
}

/*
parameter_declaration
	declaration_specifiers abstract_declarator
*/
var_t parameter_declaration(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t var;
	type_t type;
	declaration_specifiers(ss, ctx, type);
	abstract_declarator(ss, ctx, make_shared<type_t>(type), var);
	return var;
}

/*
initializer
	{assignment_expression} | '{' initializer_list '}'
*/
void initializer(stream &ss, context_t &ctx, var_t &var)
{
	anal_debug();

	// if (nxt_tok(ss).type == '{') {
	// 	match('{', ss);
	// 	initializer_list(ss, ctx, var);
	// 	match('}', ss);
	// } else {
	// 	assignment_expression(ss, ctx);
	// }
}

/*
initializer_list
	initializer {}',' initializer}*
*/
void initializer_list(stream &ss, context_t &ctx, var_t &var)
{
	anal_debug();

	// initializer(ss, ctx, var);
	// while (nxt_tok(ss).type == ',') {
	// 	match(',', ss);
	// 	initializer(ss, ctx, var);
	// }
}

/*
struct_declaration_list
	{struct_declaration}+
*/
void struct_declaration_list(stream &ss, context_t &ctx, type_t &struct_type)
{
	anal_debug();

	while (nxt_tok(ss).type != '}') {
		struct_declaration(ss, ctx, struct_type);
	}

	int align_to = 1;
	for (auto i : struct_type.inner_vars) {
		struct_type.size += i.type->size;
		if (i.type->size > align_to) {
			align_to = i.type->size;
		}
		if (align_to > 8) {
			align_to = 8;
		}
		if (i.type->size % align_to != 0) {
			struct_type.size += align_to - i.type->size % align_to;
		}
	}
}

/*
struct_declaration
	specifier_qualifier_list struct_declarator_list ';'
*/
void struct_declaration(stream &ss, context_t &ctx, type_t &struct_type)
{
	anal_debug();

	type_t decl_type;
	specifier_qualifier_list(ss, ctx, decl_type);
	std::vector<var_t> inner = struct_declarator_list(ss, ctx, decl_type);
	for (auto i : inner) {
		struct_type.inner_vars.push_back(i);
	}
	match(';', ss);
}

/*
specifier_qualifier_list
	{type_specifier | type_qualifier}*
*/
bool is_specifier_qualifier_list(tok_t tok, context_t &ctx)
{
	return is_type_specifier(tok, ctx) || is_type_qualifier(tok);
}
void specifier_qualifier_list(stream &ss, context_t &ctx, type_t &type)
{
	anal_debug();

	while (is_specifier_qualifier_list(nxt_tok(ss), ctx)) {
		if (is_type_specifier(nxt_tok(ss), ctx)) {
			type_specifier(ss, ctx, type);
		} else {
			type_qualifier(ss, ctx, type);
		}
	}
}

/*
struct_declarator_list
	declarator {',' declarator}*
*/
std::vector<var_t> struct_declarator_list(stream &ss, context_t &ctx,
					  type_t &type)
{
	anal_debug();

	std::vector<var_t> ret;
	var_t tvar;
	declarator(ss, ctx, make_shared<type_t>(type), tvar);
	if (tvar.type->type == type_t::type_func) {
		error("Function cannot be declared in struct", ss, true);
	}
	ret.push_back(tvar);
	while (nxt_tok(ss).type == ',') {
		var_t itvar;
		match(',', ss);
		declarator(ss, ctx, make_shared<type_t>(type), itvar);
		if (itvar.type->type == type_t::type_func) {
			error("Function cannot be declared in struct", ss,
			      true);
		}
		ret.push_back(itvar);
	}
	return ret;
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
	while (nxt_tok(ss).type != '}') {
		if (is_declaration_specifiers(nxt_tok(ss), ctx)) {
			declaration_list(ss, ctx);
		} else {
			statement_list(ss, ctx);
		}
	}
	match('}', ss);
}

/*
declaration_list
	{declaration}+
*/
void declaration_list(stream &ss, context_t &ctx)
{
	anal_debug();

	while (is_declaration_specifiers(nxt_tok(ss), ctx)) {
		declaration(ss, ctx);
	}
}

/*
statement_list
	{statement}+
*/
void statement_list(stream &ss, context_t &ctx)
{
	anal_debug();

	while (!is_declaration_specifiers(nxt_tok(ss), ctx) &&
	       nxt_tok(ss).type != '}') {
		statement(ss, ctx);
	}
}

/*
statement
	{ compound_statement
	| expression_statement
	| selection_statement
	| iteration_statement
	| jump_statement
	}
*/
void statement(stream &ss, context_t &ctx)
{
	anal_debug();

	if (nxt_tok(ss).type == '{') {
		context_t nctx(&ctx);
		compound_statement(ss, nctx);
	} else if (is_selection_statement(nxt_tok(ss))) {
		selection_statement(ss, ctx);
	} else if (is_iteration_statement(nxt_tok(ss))) {
		// iteration_statement(ss, ctx);
	} else if (is_jump_statement(nxt_tok(ss))) {
		jump_statement(ss, ctx);
	} else {
		expression_statement(ss, ctx);
	}
}

/*
iteration_statement
	{ WHILE '(' expression ')' statement
	| DO statement WHILE '(' expression ')' ';'
	| FOR '(' expression_statement expression_statement expression ')' statement
	}
*/
bool is_iteration_statement(tok_t tok)
{
	return tok.type == tok_while || tok.type == tok_do ||
	       tok.type == tok_for;
}

/*
unary_expression
	: postfix_expression
	| INC_OP unary_expression
	| DEC_OP unary_expression
	| unary_operator cast_expression
	| SIZEOF unary_expression
	| SIZEOF '(' type_name ')'

unary_operator
	: '&'
	| '*'
	| '+'
	| '-'
	| '~'
	| '!'

*/
bool is_unary_operator(tok_t tok)
{
	return tok.type == '&' || tok.type == '*' || tok.type == '+' ||
	       tok.type == '-' || tok.type == '~' || tok.type == '!';
}
var_t unary_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	if (nxt_tok(ss).type == tok_inc) {
		match(tok_inc, ss);
		var_t rs = unary_expression(ss, ctx);
		if (!rs.is_alloced) {
			error("Cannot increment rvalue", ss, true);
		}
		var_t tmp = emit_load(rs);
		if (is_type_i(*tmp.type)) {
			var_t inc_v;
			inc_v.type = tmp.type;
			inc_v.name = '1';
			tmp = emit_add(tmp, inc_v);
		} else if (is_type_p(*tmp.type)) {
			type_t ori_type = *tmp.type;
			type_t i64_type;
			i64_type.name = "i64";
			i64_type.type = type_t::type_basic;
			i64_type.size = 8;
			i64_type.is_long = 2;
			tmp = emit_ptrtoint(tmp, i64_type);
			var_t inc_v;
			inc_v.type = make_shared<type_t>(i64_type);
			inc_v.name = '8';
			tmp = emit_add(tmp, inc_v);
			tmp = emit_inttoptr(tmp, ori_type);
		} else if (is_type_f(*tmp.type)) {
			var_t inc_v;
			inc_v.type = tmp.type;
			inc_v.name = "1.0";
			tmp = emit_fadd(tmp, inc_v);
		} else {
			error("Cannot increment this type", ss, true);
		}
		emit_store(rs, tmp);
	}
	if (nxt_tok(ss).type == tok_dec) {
		match(tok_dec, ss);
		var_t rs = unary_expression(ss, ctx);
		if (!rs.is_alloced) {
			error("Cannot decrement rvalue", ss, true);
		}
		var_t tmp = emit_load(rs);
		if (is_type_i(*tmp.type)) {
			var_t inc_v;
			inc_v.type = tmp.type;
			inc_v.name = '1';
			tmp = emit_sub(tmp, inc_v);
		} else if (is_type_p(*tmp.type)) {
			type_t ori_type = *tmp.type;
			type_t i64_type;
			i64_type.name = "i64";
			i64_type.type = type_t::type_basic;
			i64_type.size = 8;
			i64_type.is_long = 2;
			tmp = emit_ptrtoint(tmp, i64_type);
			var_t inc_v;
			inc_v.type = make_shared<type_t>(i64_type);
			inc_v.name = '8';
			tmp = emit_sub(tmp, inc_v);
			tmp = emit_inttoptr(tmp, ori_type);
		} else if (is_type_f(*tmp.type)) {
			var_t inc_v;
			inc_v.type = tmp.type;
			inc_v.name = "1.0";
			tmp = emit_fsub(tmp, inc_v);
		} else {
			error("Cannot decrement this type", ss, true);
		}
		emit_store(rs, tmp);
	}
	if (is_unary_operator(nxt_tok(ss))) {
		if (nxt_tok(ss).type == '&') {
			match('&', ss);
			var_t tmp = cast_expression(ss, ctx);
			if (tmp.type->type == type_t::type_pointer &&
			    tmp.type->ptr_to->type == type_t::type_func) {
				return tmp;
			}
			if (!tmp.is_alloced) {
				error("Cannot get address of rvalue", ss, true);
			}
			tmp.is_alloced = false;
			return tmp;
		}
		if (nxt_tok(ss).type == '*') {
			match('*', ss);
			var_t tmp = cast_expression(ss, ctx);
			if (tmp.is_alloced) {
				tmp = emit_load(tmp);
			}
			if (!is_type_p(*tmp.type)) {
				error("Cannot dereference non-pointer type", ss,
				      true);
			}
			if (tmp.type->ptr_to->type == type_t::type_func) {
				return tmp;
			}
			return emit_load(tmp);
		}
		if (nxt_tok(ss).type == '+') {
			match('+', ss);
			var_t tmp = cast_expression(ss, ctx);
			if (tmp.is_alloced) {
				tmp = emit_load(tmp);
			}
			if (tmp.type->type != type_t::type_basic) {
				error("Cannot use unary '+' on non-basic type",
				      ss, true);
			}
			return tmp;
		}
		if (nxt_tok(ss).type == '-') {
			match('+', ss);
			var_t tmp = cast_expression(ss, ctx);
			if (tmp.is_alloced) {
				tmp = emit_load(tmp);
			}
			if (tmp.type->type != type_t::type_basic) {
				error("Cannot use unary '-' on non-basic type",
				      ss, true);
			}
			if (is_type_i(*tmp.type)) {
				var_t zero;
				zero.type = tmp.type;
				zero.name = '0';
				tmp = emit_sub(zero, tmp);
			} else if (is_type_f(*tmp.type)) {
				var_t zero;
				zero.type = tmp.type;
				zero.name = "0.0";
				tmp = emit_fsub(zero, tmp);
			} else {
				error("Cannot use unary '-' on this type", ss,
				      true);
			}
			return tmp;
		}
		if (nxt_tok(ss).type == '~') {
			match('~', ss);
			var_t tmp = cast_expression(ss, ctx);
			if (tmp.is_alloced) {
				tmp = emit_load(tmp);
			}
			if (!is_type_i(*tmp.type)) {
				error("Cannot use unary '~' on non-basic type",
				      ss, true);
			}

			var_t minus_one;
			minus_one.type = tmp.type;
			minus_one.name = "-1";
			tmp = emit_xor(tmp, minus_one);
			return tmp;
		}
		if (nxt_tok(ss).type == '!') {
			match('!', ss);
			var_t tmp = cast_expression(ss, ctx);
			if (tmp.is_alloced) {
				tmp = emit_load(tmp);
			}
			if (!is_type_i(*tmp.type)) {
				error("Cannot use unary '!' on non-basic type",
				      ss, true);
			}
			var_t zero;
			zero.type = tmp.type;
			zero.name = '0';
			tmp = emit_eq(tmp, zero);
			return tmp;
		}
	}
	if (nxt_tok(ss).type == tok_sizeof) {
		match(tok_sizeof, ss);
		var_t tmp;
		if (nxt_tok(ss).type == '(') {
			match('(', ss);
			tmp.type = make_shared<type_t>(type_name(ss, ctx));
			match(')', ss);
		} else {
			tmp = unary_expression(ss, ctx);
		}
		if (tmp.is_alloced) {
			tmp = emit_load(tmp);
		}
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		var_t ret;
		ret.type = make_shared<type_t>(i64_type);
		ret.name = std::to_string(tmp.type->size);
		return ret;
	}
	return postfix_expression(ss, ctx);
}

/*
cast_expression
	: unary_expression
	| '(' specifier_qualifier_list {abstract_declarator}? ')' cast_expression
*/
var_t cast_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	if (nxt_tok(ss).type == '(') {
		match('(', ss);
		type_t type = type_name(ss, ctx);
		match(')', ss);
		var_t tmp = cast_expression(ss, ctx);
		if (tmp.is_alloced) {
			tmp = emit_load(tmp);
		}
		if (type.type != type_t::type_basic &&
		    type.type != type_t::type_pointer) {
			error("Cannot cast to non-basic type", ss, true);
		}
		var_t ret = emit_conv_to(tmp, type);
		return ret;
	}
	return unary_expression(ss, ctx);
}

/*
type_name
	specifier_qualifier_list {abstract_declarator}?
*/
type_t type_name(stream &ss, context_t &ctx)
{
	type_t type;
	specifier_qualifier_list(ss, ctx, type);
	if (nxt_tok(ss).type != ')') {
		var_t var;
		abstract_declarator(ss, ctx, make_shared<type_t>(type), var);
		type = *(var.type);
	}
	return type;
}

/*
primary_expression
	  IDENTIFIER
	| CONSTANT
	| STRING_LITERAL
	| '(' expression ')'
*/
var_t primary_expression(stream &ss, context_t &ctx)
{
	if (nxt_tok(ss).type == tok_ident) {
		tok_t tok = get_tok(ss);
		string real_name = '%' + tok.str;
		var_t var = ctx.get_var(real_name);
		if (var.type->type != type_t::type_unknown) {
			return var;
		}
		// try global var
		real_name = '@' + tok.str;
		var = ctx.get_var(real_name);
		if (var.type->type != type_t::type_unknown) {
			return var;
		}
		// try enum const
		real_name = tok.str;
		int val = ctx.get_enum(real_name);
		if (val >= 0) {
			var_t var;
			var.type = make_shared<type_t>();
			var.type->type = type_t::type_basic;
			var.type->name = "i32";
			var.type->size = 4;
			var.type->is_long = 1;
			var.name = std::to_string(val);
			return var;
		}
		error("Unknown identifier", ss, true);
	}
	if (nxt_tok(ss).type == tok_int_lit) {
		tok_t tok = get_tok(ss);
		var_t var;
		var.type = make_shared<type_t>();
		var.type->type = type_t::type_basic;
		var.type->name = "i32";
		var.type->size = 4;
		var.type->is_long = 1;
		var.name = tok.str;
		return var;
	}
	if (nxt_tok(ss).type == tok_float_lit) {
		tok_t tok = get_tok(ss);
		var_t var;
		var.type = make_shared<type_t>();
		var.type->type = type_t::type_basic;
		var.type->name = "f64";
		var.type->size = 8;
		var.type->is_long = 2;
		var.name = tok.str;
		return var;
	}
	if (nxt_tok(ss).type == tok_string_lit) {
		tok_t tok = get_tok(ss);
		type_t char_type;
		char_type.type = type_t::type_basic;
		char_type.name = "i8";
		char_type.size = 1;
		char_type.is_char = true;
		type_t arr_type;
		arr_type.type = type_t::type_array;
		arr_type.name = get_ptr_type_name(char_type.name);
		arr_type.size = char_type.size * tok.str.size();
		arr_type.ptr_to = make_shared<type_t>(char_type);
		var_t var;
		var.type = make_shared<type_t>(arr_type);
		var.name = '@' + get_unnamed_var_name();
		string init_str;
		init_str = "c";
		init_str += "\"";
		init_str += tok.str;
		init_str += "\"";
		emit_post_const_decl(var, init_str);
		return var;
	}
	if (nxt_tok(ss).type == '(') {
		match('(', ss);
		var_t var = expression(ss, ctx);
		match(')', ss);
		return var;
	}
	error("Primary expression expected", ss, true);
	throw "unreachable";
}

/*
postfix_expression
	  primary_expression
	| postfix_expression '[' expression ']'
	| postfix_expression '(' ')'
	| postfix_expression '(' argument_expression_list ')'
	| postfix_expression '.' IDENTIFIER
	| postfix_expression PTR_OP IDENTIFIER
	| postfix_expression INC_OP
	| postfix_expression DEC_OP
*/
bool is_postfix_expression_op(tok_t tok)
{
	return tok.type == '[' || tok.type == '(' || tok.type == '.' ||
	       tok.type == tok_pointer || tok.type == tok_inc ||
	       tok.type == tok_dec;
}
var_t postfix_expression(stream &ss, context_t &ctx)
{
	var_t tmp = primary_expression(ss, ctx);
	while (is_postfix_expression_op(nxt_tok(ss))) {
		if (nxt_tok(ss).type == '[') {
			if (tmp.is_alloced) {
				tmp = emit_load(tmp);
			}
			match('[', ss);
			var_t idx = expression(ss, ctx);
			if (!is_type_i(*idx.type)) {
				error("Array index must be integer", ss, true);
			}
			if (!is_type_p(*tmp.type)) {
				error("Cannot index non-pointer type", ss,
				      true);
			}
			match(']', ss);
			tmp = get_item_from_arrptr(tmp, idx);
		} else if (nxt_tok(ss).type == '(') {
			match('(', ss);
			std::vector<var_t> args;
			if (nxt_tok(ss).type != ')') {
				args = argument_expression_list(ss, ctx);
			}
			if (tmp.type->type != type_t::type_pointer ||
			    tmp.type->ptr_to->type != type_t::type_func) {
				error("Cannot call non-function type", ss,
				      true);
			}
			match(')', ss);
			tmp = emit_call(tmp, args);
		} else if (nxt_tok(ss).type == '.') {
			match('.', ss);
			tok_t tok = get_tok(ss);
			if (tok.type != tok_ident) {
				error("Identifier expected", ss, true);
			}
			if (tmp.is_alloced) {
				if (tmp.type->type != type_t::type_pointer ||
				    tmp.type->ptr_to->type !=
					    type_t::type_struct) {
					error("Cannot access member of non-struct type",
					      ss, true);
				}
				int offset = -1;
				for (int i = 0;
				     i < tmp.type->ptr_to->inner_vars.size();
				     i++) {
					if (tmp.type->ptr_to->inner_vars[i]
						    .name == tok.str) {
						offset = i;
						break;
					}
				}
				if (offset == -1) {
					error("Unknown member", ss, true);
				}
				tmp = get_item_from_structptr(tmp, offset);
				tmp.is_alloced = true;
			} else {
				if (tmp.type->type != type_t::type_struct) {
					error("Cannot access member of non-struct type",
					      ss, true);
				}
				int offset = -1;
				for (int i = 0; i < tmp.type->inner_vars.size();
				     i++) {
					if (tmp.type->inner_vars[i].name ==
					    tok.str) {
						offset = i;
						break;
					}
				}
				if (offset == -1) {
					error("Unknown member", ss, true);
				}
				tmp = get_item_from_structobj(tmp, offset);
			}
		} else if (nxt_tok(ss).type == tok_pointer) {
			match(tok_pointer, ss);
			tok_t tok = get_tok(ss);
			if (tok.type != tok_ident) {
				error("Identifier expected", ss, true);
			}
			if (tmp.is_alloced) {
				tmp = emit_load(tmp);
			}
			if (tmp.type->type != type_t::type_pointer ||
			    tmp.type->ptr_to->type != type_t::type_struct) {
				error("Cannot access member of non-struct type",
				      ss, true);
			}
			int offset = -1;
			for (int i = 0; i < tmp.type->ptr_to->inner_vars.size();
			     i++) {
				if (tmp.type->ptr_to->inner_vars[i].name ==
				    tok.str) {
					offset = i;
					break;
				}
			}
			if (offset == -1) {
				error("Unknown member", ss, true);
			}
			tmp = get_item_from_structptr(tmp, offset);
			tmp.is_alloced = true;
		} else if (nxt_tok(ss).type == tok_inc) {
			if (!tmp.is_alloced) {
				error("Cannot increment rvalue", ss, true);
			}
			match(tok_inc, ss);
			var_t ori_var = tmp;
			tmp = emit_load(tmp);
			var_t ret_var = tmp;
			if (is_type_i(*tmp.type)) {
				var_t inc_v;
				inc_v.type = tmp.type;
				inc_v.name = '1';
				tmp = emit_add(tmp, inc_v);
			} else if (is_type_p(*tmp.type)) {
				type_t ori_type = *tmp.type;
				type_t i64_type;
				i64_type.name = "i64";
				i64_type.type = type_t::type_basic;
				i64_type.size = 8;
				i64_type.is_long = 2;
				tmp = emit_ptrtoint(tmp, i64_type);
				var_t inc_v;
				inc_v.type = make_shared<type_t>(i64_type);
				inc_v.name = '8';
				tmp = emit_add(tmp, inc_v);
				tmp = emit_inttoptr(tmp, ori_type);
			} else if (is_type_f(*tmp.type)) {
				var_t inc_v;
				inc_v.type = tmp.type;
				inc_v.name = "1.0";
				tmp = emit_fadd(tmp, inc_v);
			} else {
				error("Cannot increment this type", ss, true);
			}
			emit_store(ori_var, tmp);
			tmp = ret_var;
		} else if (nxt_tok(ss).type == tok_dec) {
			if (!tmp.is_alloced) {
				error("Cannot increment rvalue", ss, true);
			}
			match(tok_dec, ss);
			var_t ori_var = tmp;
			tmp = emit_load(tmp);
			var_t ret_var = tmp;
			if (is_type_i(*tmp.type)) {
				var_t inc_v;
				inc_v.type = tmp.type;
				inc_v.name = '1';
				tmp = emit_sub(tmp, inc_v);
			} else if (is_type_p(*tmp.type)) {
				type_t ori_type = *tmp.type;
				type_t i64_type;
				i64_type.name = "i64";
				i64_type.type = type_t::type_basic;
				i64_type.size = 8;
				i64_type.is_long = 2;
				tmp = emit_ptrtoint(tmp, i64_type);
				var_t inc_v;
				inc_v.type = make_shared<type_t>(i64_type);
				inc_v.name = '8';
				tmp = emit_sub(tmp, inc_v);
				tmp = emit_inttoptr(tmp, ori_type);
			} else if (is_type_f(*tmp.type)) {
				var_t inc_v;
				inc_v.type = tmp.type;
				inc_v.name = "1.0";
				tmp = emit_fsub(tmp, inc_v);
			} else {
				error("Cannot decrement this type", ss, true);
			}
			emit_store(ori_var, tmp);
			tmp = ret_var;
		}
	}
	return tmp;
}

/*
argument_expression_list
	assignment_expression {',' assignment_expression}*
*/
std::vector<var_t> argument_expression_list(stream &ss, context_t &ctx)
{
	anal_debug();

	std::vector<var_t> args;
	args.push_back(assignment_expression(ss, ctx));
	while (nxt_tok(ss).type == ',') {
		match(',', ss);
		args.push_back(assignment_expression(ss, ctx));
	}
	return args;
}

/*
multiplicative_expression
	cast_expression { {'*' | '/' | '%' } cast_expression }*
*/
bool is_mul_op(tok_t tok)
{
	return tok.type == '*' || tok.type == '/' || tok.type == '%';
}
var_t multiplicative_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = cast_expression(ss, ctx);
	while (is_mul_op(nxt_tok(ss))) {
		if (nxt_tok(ss).type == '*') {
			match('*', ss);
			var_t rt = cast_expression(ss, ctx);
			if (rs.is_alloced) {
				rs = emit_load(rs);
			}
			if (rt.is_alloced) {
				rt = emit_load(rt);
			}
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
				error("Cannot multiply non-basic type", ss,
				      true);
			}
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
				error("Cannot multiply non-basic type", ss,
				      true);
			}
			emit_match_type(rs, rt);
			if (is_type_i(*rs.type)) {
				rs = emit_mul(rs, rt);
			} else if (is_type_f(*rs.type)) {
				rs = emit_fmul(rs, rt);
			} else {
				error("Cannot multiply this type", ss, true);
			}
		} else if (nxt_tok(ss).type == '/') {
			match('/', ss);
			var_t rt = cast_expression(ss, ctx);
			if (rs.is_alloced) {
				rs = emit_load(rs);
			}
			if (rt.is_alloced) {
				rt = emit_load(rt);
			}
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
				error("Cannot divide non-basic type", ss, true);
			}
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
				error("Cannot divide non-basic type", ss, true);
			}
			emit_match_type(rs, rt);
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				rs = emit_udiv(rs, rt);
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				rs = emit_sdiv(rs, rt);
			} else if (is_type_f(*rs.type)) {
				rs = emit_fdiv(rs, rt);
			} else {
				error("Cannot multiply this type", ss, true);
			}
		} else if (nxt_tok(ss).type == '%') {
			match('/', ss);
			var_t rt = cast_expression(ss, ctx);
			if (rs.is_alloced) {
				rs = emit_load(rs);
			}
			if (rt.is_alloced) {
				rt = emit_load(rt);
			}
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
				error("Cannot divide non-basic type", ss, true);
			}
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
				error("Cannot divide non-basic type", ss, true);
			}
			emit_match_type(rs, rt);
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				rs = emit_urem(rs, rt);
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				rs = emit_srem(rs, rt);
			} else if (is_type_f(*rs.type)) {
				rs = emit_frem(rs, rt);
			} else {
				error("Cannot multiply this type", ss, true);
			}
		}
	}
	return rs;
}

/*
additive_expression
	multiplicative_expression {('+' | '-') multiplicative_expression}*
*/
bool is_add_op(tok_t tok)
{
	return tok.type == '+' || tok.type == '-';
}
var_t additive_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = multiplicative_expression(ss, ctx);
	while (is_add_op(nxt_tok(ss))) {
		if (nxt_tok(ss).type == '+') {
			match('+', ss);
			var_t rt = multiplicative_expression(ss, ctx);
			if (rs.is_alloced) {
				rs = emit_load(rs);
			}
			if (rt.is_alloced) {
				rt = emit_load(rt);
			}
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
				error("Cannot add non-basic type", ss, true);
			}
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
				error("Cannot add non-basic type", ss, true);
			}
			emit_match_type(rs, rt);
			if (is_type_i(*rs.type)) {
				rs = emit_add(rs, rt);
			} else if (is_type_f(*rs.type)) {
				rs = emit_fadd(rs, rt);
			} else {
				error("Cannot add this type", ss, true);
			}
		} else if (nxt_tok(ss).type == '-') {
			match('-', ss);
			var_t rt = multiplicative_expression(ss, ctx);
			if (rs.is_alloced) {
				rs = emit_load(rs);
			}
			if (rt.is_alloced) {
				rt = emit_load(rt);
			}
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
				error("Cannot subtract non-basic type", ss,
				      true);
			}
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
				error("Cannot subtract non-basic type", ss,
				      true);
			}
			emit_match_type(rs, rt);
			if (is_type_i(*rs.type)) {
				rs = emit_sub(rs, rt);
			} else if (is_type_f(*rs.type)) {
				rs = emit_fsub(rs, rt);
			} else {
				error("Cannot subtract this type", ss, true);
			}
		}
	}
	return rs;
}

/*
shift_expression
	additive_expression {LEFT_OP | RIGHT_OP} additive_expression
*/
bool is_shift_op(tok_t tok)
{
	return tok.type == tok_lshift || tok.type == tok_rshift;
}
var_t shift_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = additive_expression(ss, ctx);
	while (is_shift_op(nxt_tok(ss))) {
		if (nxt_tok(ss).type == tok_lshift) {
			match(tok_lshift, ss);
			var_t rt = additive_expression(ss, ctx);
			if (rs.is_alloced) {
				rs = emit_load(rs);
			}
			if (rt.is_alloced) {
				rt = emit_load(rt);
			}
			if (!is_type_i(*rs.type)) {
				error("Cannot left shift non-basic type", ss,
				      true);
			}
			if (!is_type_i(*rt.type)) {
				error("Cannot left shift non-basic type", ss,
				      true);
			}
			emit_match_type(rs, rt);
			rs = emit_shl(rs, rt);
		} else if (nxt_tok(ss).type == tok_rshift) {
			match(tok_rshift, ss);
			var_t rt = additive_expression(ss, ctx);
			if (rs.is_alloced) {
				rs = emit_load(rs);
			}
			if (rt.is_alloced) {
				rt = emit_load(rt);
			}
			if (!is_type_i(*rs.type)) {
				error("Cannot right shift non-basic type", ss,
				      true);
			}
			if (!is_type_i(*rt.type)) {
				error("Cannot right shift non-basic type", ss,
				      true);
			}
			emit_match_type(rs, rt);
			if (rs.type->is_unsigned) {
				rs = emit_lshr(rs, rt);
			} else {
				rs = emit_ashr(rs, rt);
			}
		}
	}
	return rs;
}

/*
relational_expression
	shift_expression {('<' | '>' | LE_OP | GE_OP) shift_expression}*
*/
bool is_rel_op(tok_t tok)
{
	return tok.type == '<' || tok.type == '>' || tok.type == tok_le ||
	       tok.type == tok_ge;
}
var_t relational_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = shift_expression(ss, ctx);
	while (is_rel_op(nxt_tok(ss))) {
		if (rs.is_alloced) {
			rs = emit_load(rs);
		}
		if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
			error("Cannot compare non-basic type", ss, true);
		}
		tok_t ntok = get_tok(ss);
		var_t rt = shift_expression(ss, ctx);
		if (rt.is_alloced) {
			rt = emit_load(rt);
		}
		if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
			error("Cannot compare non-basic type", ss, true);
		}
		emit_match_type(rs, rt);
		if (ntok.type == '<') {
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				rs = emit_ult(rs, rt);
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				rs = emit_slt(rs, rt);
			} else if (is_type_f(*rs.type)) {
				rs = emit_flt(rs, rt);
			} else {
				error("Cannot compare this type", ss, true);
			}
		} else if (ntok.type == '>') {
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				rs = emit_ugt(rs, rt);
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				rs = emit_sgt(rs, rt);
			} else if (is_type_f(*rs.type)) {
				rs = emit_fgt(rs, rt);
			} else {
				error("Cannot compare this type", ss, true);
			}
		} else if (ntok.type == tok_ge) {
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				rs = emit_uge(rs, rt);
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				rs = emit_sge(rs, rt);
			} else if (is_type_f(*rs.type)) {
				rs = emit_fge(rs, rt);
			} else {
				error("Cannot compare this type", ss, true);
			}
		} else if (ntok.type == tok_le) {
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				rs = emit_ule(rs, rt);
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				rs = emit_sle(rs, rt);
			} else if (is_type_f(*rs.type)) {
				rs = emit_fle(rs, rt);
			} else {
				error("Cannot compare this type", ss, true);
			}
		}
	}
	return rs;
}

/*
equality_expression
	relational_expression {EQ_OP | NE_OP relational_expression}*
*/
bool is_eq_op(tok_t tok)
{
	return tok.type == tok_eq || tok.type == tok_ne;
}
var_t equality_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = relational_expression(ss, ctx);
	while (is_eq_op(nxt_tok(ss))) {
		if (rs.is_alloced) {
			rs = emit_load(rs);
		}
		if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
		    !is_type_p(*rs.type)) {
			error("Cannot compare non-basic type", ss, true);
		}
		tok_t ntok = get_tok(ss);
		var_t rt = relational_expression(ss, ctx);
		if (rt.is_alloced) {
			rt = emit_load(rt);
		}
		if (!is_type_i(*rt.type) && !is_type_f(*rt.type) &&
		    !is_type_p(*rt.type)) {
			error("Cannot compare non-basic type", ss, true);
		}

		if (is_type_p(*rs.type) && is_type_p(*rt.type)) {
			type_t i64_type;
			i64_type.name = "i64";
			i64_type.type = type_t::type_basic;
			i64_type.size = 8;
			i64_type.is_long = 2;
			rs = emit_ptrtoint(rs, i64_type);
			rt = emit_ptrtoint(rt, i64_type);
		} else if (!is_type_p(*rs.type) && !is_type_p(*rt.type)) {
			emit_match_type(rs, rt);
		} else {
			error("Cannot compare pointer with non-pointer", ss,
			      true);
		}
		if (ntok.type == tok_eq) {
			if (is_type_i(*rs.type)) {
				rs = emit_eq(rs, rt);
			} else if (is_type_f(*rs.type)) {
				rs = emit_feq(rs, rt);
			} else {
				error("Cannot compare this type", ss, true);
			}
		} else if (ntok.type == tok_ne) {
			if (is_type_i(*rs.type)) {
				rs = emit_ne(rs, rt);
			} else if (is_type_f(*rs.type)) {
				rs = emit_fne(rs, rt);
			} else {
				error("Cannot compare this type", ss, true);
			}
		}
	}
	return rs;
}

/*
and_expression
	equality_expression { '&' equality_expression }*
*/
var_t and_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = equality_expression(ss, ctx);
	while (nxt_tok(ss).type == '&') {
		match('&', ss);
		var_t rt = equality_expression(ss, ctx);
		if (rs.is_alloced) {
			rs = emit_load(rs);
		}
		if (rt.is_alloced) {
			rt = emit_load(rt);
		}
		if (!is_type_i(*rs.type)) {
			error("Cannot and non-int type", ss, true);
		}
		if (!is_type_i(*rt.type)) {
			error("Cannot and non-int type", ss, true);
		}
		emit_match_type(rs, rt);
		rs = emit_and(rs, rt);
	}
	return rs;
}

/*
exclusive_or_expression
	and_expression { '^' and_expression }*
*/
var_t exclusive_or_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = and_expression(ss, ctx);
	while (nxt_tok(ss).type == '^') {
		match('^', ss);
		var_t rt = and_expression(ss, ctx);
		if (rs.is_alloced) {
			rs = emit_load(rs);
		}
		if (rt.is_alloced) {
			rt = emit_load(rt);
		}
		if (!is_type_i(*rs.type)) {
			error("Cannot xor non-int type", ss, true);
		}
		if (!is_type_i(*rt.type)) {
			error("Cannot xor non-int type", ss, true);
		}
		emit_match_type(rs, rt);
		rs = emit_xor(rs, rt);
	}
	return rs;
}

/*
inclusive_or_expression
	exclusive_or_expression { '|' exclusive_or_expression }*
*/
var_t inclusive_or_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = exclusive_or_expression(ss, ctx);
	while (nxt_tok(ss).type == '|') {
		match('|', ss);
		var_t rt = exclusive_or_expression(ss, ctx);
		if (rs.is_alloced) {
			rs = emit_load(rs);
		}
		if (rt.is_alloced) {
			rt = emit_load(rt);
		}
		if (!is_type_i(*rs.type)) {
			error("Cannot or non-int type", ss, true);
		}
		if (!is_type_i(*rt.type)) {
			error("Cannot or non-int type", ss, true);
		}
		emit_match_type(rs, rt);
		rs = emit_or(rs, rt);
	}
	return rs;
}

/*
logical_and_expression
	inclusive_or_expression {AND_OP inclusive_or_expression}*
*/
var_t logical_and_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = inclusive_or_expression(ss, ctx);
	while (nxt_tok(ss).type == tok_land) {
		match(tok_land, ss);
		var_t rt = inclusive_or_expression(ss, ctx);
		if (rs.is_alloced) {
			rs = emit_load(rs);
		}
		if (rt.is_alloced) {
			rt = emit_load(rt);
		}
		if (!rs.type->is_bool) {
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
			    !is_type_p(*rs.type)) {
				error("Cannot and non-basic type", ss, true);
			}
			var_t zero;
			zero.type = rs.type;
			zero.name = "zeroinitializer";
			rs = emit_ne(rs, zero);
		}
		if (!rt.type->is_bool) {
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type) &&
			    !is_type_p(*rt.type)) {
				error("Cannot and non-basic type", ss, true);
			}
			var_t zero;
			zero.type = rt.type;
			zero.name = "zeroinitializer";
			rt = emit_ne(rt, zero);
		}
		rs = emit_and(rs, rt);
	}
	return rs;
}

/*
logical_or_expression
	logical_and_expression {OR_OP logical_and_expression}*
*/
var_t logical_or_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = logical_and_expression(ss, ctx);
	while (nxt_tok(ss).type == tok_lor) {
		match(tok_lor, ss);
		var_t rt = logical_and_expression(ss, ctx);
		if (rs.is_alloced) {
			rs = emit_load(rs);
		}
		if (rt.is_alloced) {
			rt = emit_load(rt);
		}
		if (!rs.type->is_bool) {
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
			    !is_type_p(*rs.type)) {
				error("Cannot or non-basic type", ss, true);
			}
			var_t zero;
			zero.type = rs.type;
			zero.name = "zeroinitializer";
			rs = emit_ne(rs, zero);
		}
		if (!rt.type->is_bool) {
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type) &&
			    !is_type_p(*rt.type)) {
				error("Cannot or non-basic type", ss, true);
			}
			var_t zero;
			zero.type = rt.type;
			zero.name = "zeroinitializer";
			rt = emit_ne(rt, zero);
		}
		rs = emit_or(rs, rt);
	}
	return rs;
}

/*
conditional_expression
	logical_or_expression {'?' expression ':' conditional_expression}?
*/
bool should_conv_to_first(const var_t &v1, const var_t &v2)
{
	bool conv_to_v1 = false;
	if (is_type_i(*v1.type) && is_type_i(*v2.type)) {
		if (v1.type->size < v2.type->size) {
			conv_to_v1 = true;
		} else {
			conv_to_v1 = false;
		}
	} else if (is_type_f(*v1.type) && is_type_f(*v2.type)) {
		if (v1.type->size < v2.type->size) {
			conv_to_v1 = true;
		} else {
			conv_to_v1 = false;
		}
	} else if (is_type_p(*v1.type) && is_type_p(*v2.type)) {
		return false;
	} else if (is_type_p(*v1.type) && is_type_i(*v2.type)) {
		conv_to_v1 = false;
	} else if (is_type_i(*v1.type) && is_type_p(*v2.type)) {
		conv_to_v1 = true;
	} else if (is_type_f(*v1.type) && is_type_i(*v2.type)) {
		conv_to_v1 = true;
	} else if (is_type_i(*v1.type) && is_type_f(*v2.type)) {
		conv_to_v1 = false;
	} else {
		std::stringstream v1_decl;
		std::stringstream v2_decl;
		v1.output_var(&v1_decl);
		v2.output_var(&v1_decl);
		err_msg("Cannot convert between these types\n" + v1_decl.str() +
			"\n" + v2_decl.str());
	}
	return conv_to_v1;
}
var_t conditional_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = logical_or_expression(ss, ctx);
	if (nxt_tok(ss).type == '?') {
		if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
		    !is_type_p(*rs.type)) {
			error("Cannot use non-basic type as condition", ss,
			      true);
		}
		if (!rs.type->is_bool) {
			var_t zero;
			zero.type = rs.type;
			zero.name = "zeroinitializer";
			rs = emit_ne(rs, zero);
		}
		match('?', ss);
		string label_true = get_label();
		string label_false = get_label();
		string label_true_conv = get_label();
		string label_false_conv = get_label();
		string label_end = get_label();
		emit_br(rs, label_true, label_false);

		emit_label(label_true);
		var_t rt = expression(ss, ctx);
		if (rt.is_alloced) {
			rt = emit_load(rt);
		}
		emit_br(label_true_conv);

		match(':', ss);
		emit_label(label_false);
		var_t rr = conditional_expression(ss, ctx);
		if (rr.is_alloced) {
			rr = emit_load(rr);
		}
		emit_br(label_false_conv);

		bool conv_to_rt = should_conv_to_first(rt, rr);
		bool conv_to_rr = should_conv_to_first(rr, rt);

		emit_label(label_true_conv);
		if (conv_to_rt) {
			rt = emit_conv_to(rt, *rr.type);
		}
		emit_br(label_end);

		emit_label(label_false_conv);
		if (conv_to_rr) {
			rr = emit_conv_to(rr, *rt.type);
		}
		emit_br(label_end);

		emit_label(label_end);
		rs = emit_phi(rt, label_true_conv, rr, label_false_conv);
	}

	return rs;
}

/*
assignment_operator
	{'=' | MUL_ASSIGN | DIV_ASSIGN | MOD_ASSIGN | ADD_ASSIGN
	| SUB_ASSIGN | LEFT_ASSIGN | RIGHT_ASSIGN | AND_ASSIGN
	| XOR_ASSIGN | OR_ASSIGN
    }
*/
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
void assignment_operator(stream &ss, var_t rd, var_t rs, tok_t op)
{
	anal_debug();

	if (rd.is_alloced) {
		error("Cannot assign to non-lvalue", ss, true);
	}
	if (rs.is_alloced) {
		rs = emit_load(rs);
	}
	if (op.type == '=') {
		emit_store(rd, rs);
		return;
	}
	var_t rt = emit_load(rd);
	if (is_type_p(*rs.type)) {
		if (op.type != tok_add_assign && op.type != tok_sub_assign) {
			error("Cannot assign pointer with this operator", ss,
			      true);
		}
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		rs = emit_ptrtoint(rs, i64_type);
	}
	if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
		error("Cannot assign non-basic type", ss, true);
	}
	if (is_type_p(*rt.type)) {
		if (op.type != tok_add_assign && op.type != tok_sub_assign) {
			error("Cannot assign pointer with this operator", ss,
			      true);
		}
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		rt = emit_ptrtoint(rt, i64_type);
	}
	if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
		error("Cannot assign non-basic type", ss, true);
	}
	emit_match_type(rs, rt);
	if (op.type == tok_add_assign) {
		if (is_type_i(*rs.type)) {
			rt = emit_add(rt, rs);
		} else if (is_type_f(*rs.type)) {
			rt = emit_fadd(rt, rs);
		}
	} else if (op.type == tok_sub_assign) {
		if (is_type_i(*rs.type)) {
			rt = emit_sub(rt, rs);
		} else if (is_type_f(*rs.type)) {
			rt = emit_fsub(rt, rs);
		}
	} else if (op.type == tok_mul_assign) {
		if (is_type_i(*rs.type)) {
			rt = emit_mul(rt, rs);
		} else if (is_type_f(*rs.type)) {
			rt = emit_fmul(rt, rs);
		}
	} else if (op.type == tok_div_assign) {
		if (is_type_i(*rs.type) && rs.type->is_unsigned) {
			rt = emit_udiv(rt, rs);
		} else if (is_type_i(*rs.type) && !rs.type->is_unsigned) {
			rt = emit_sdiv(rt, rs);
		} else if (is_type_f(*rs.type)) {
			rt = emit_fdiv(rt, rs);
		}
	} else if (op.type == tok_mod_assign) {
		if (is_type_i(*rs.type) && rs.type->is_unsigned) {
			rt = emit_urem(rt, rs);
		} else if (is_type_i(*rs.type) && !rs.type->is_unsigned) {
			rt = emit_srem(rt, rs);
		} else if (is_type_f(*rs.type)) {
			rt = emit_frem(rt, rs);
		}
	} else if (op.type == tok_lshift_assign) {
		if (!is_type_i(*rs.type)) {
			error("Cannot left shift non-int type", ss, true);
		}
		rt = emit_shl(rt, rs);
	} else if (op.type == tok_rshift_assign) {
		if (!is_type_i(*rs.type)) {
			error("Cannot right shift non-int type", ss, true);
		}
		if (rt.type->is_unsigned) {
			rt = emit_lshr(rt, rs);
		} else {
			rt = emit_ashr(rt, rs);
		}
	} else if (op.type == tok_and_assign) {
		if (!is_type_i(*rs.type)) {
			error("Cannot and non-int type", ss, true);
		}
		rt = emit_and(rt, rs);
	} else if (op.type == tok_xor_assign) {
		if (!is_type_i(*rs.type)) {
			error("Cannot xor non-int type", ss, true);
		}
		rt = emit_xor(rt, rs);
	} else if (op.type == tok_or_assign) {
		if (!is_type_i(*rs.type)) {
			error("Cannot or non-int type", ss, true);
		}
		rt = emit_or(rt, rs);
	}
	rt = emit_trunc_to(rt, *rd.type);
	emit_store(rd, rt);
}

/*
assignment_expression
	{ conditional_expression
	| unary_expression assignment_operator assignment_expression
	}
*/
bool chk_has_assignment_operator(stream &ss)
{
	std::stack<tok_t> store;
	while (nxt_tok(ss).type != ';' && nxt_tok(ss).type != tok_eof) {
		if (is_assignment_operatior(nxt_tok(ss))) {
			while (!store.empty()) {
				unget_tok(store.top());
				store.pop();
			}
			return true;
		}
		store.push(get_tok(ss));
	}
	while (!store.empty()) {
		unget_tok(store.top());
		store.pop();
	}
	return false;
}
var_t assignment_expression(stream &ss, context_t &ctx)
{
	anal_debug();

	if (!chk_has_assignment_operator(ss)) {
		return conditional_expression(ss, ctx);
	} else {
		var_t rd = unary_expression(ss, ctx);
		tok_t op = get_tok(ss);
		var_t rs = assignment_expression(ss, ctx);
		assignment_operator(ss, rd, rs, op);
		return rd;
	}
}

/*
expression
	assignment_expression {',' assignment_expression}*
*/
var_t expression(stream &ss, context_t &ctx)
{
	anal_debug();

	var_t rs = assignment_expression(ss, ctx);
	while (nxt_tok(ss).type == ',') {
		match(',', ss);
		var_t rs = assignment_expression(ss, ctx);
	}
	return rs;
}

/*
expression_statement
	{expression}? ';'
*/
void expression_statement(stream &ss, context_t &ctx)
{
	anal_debug();

	if (nxt_tok(ss).type == ';') {
		match(';', ss);
		return;
	}
	expression(ss, ctx);
	match(';', ss);
}

/*
jump_statement
	{ CONTINUE ';'
	| BREAK ';'
	| RETURN {expression}? ';'
	}
*/
bool is_jump_statement(tok_t tok)
{
	return tok.type == tok_continue || tok.type == tok_break ||
	       tok.type == tok_return;
}
void jump_statement(stream &ss, context_t &ctx)
{
	anal_debug();

	if (!is_jump_statement(nxt_tok(ss))) {
		error("Not a jump statement", ss, true);
	}

	if (nxt_tok(ss).type == tok_continue) {
		match(tok_continue, ss);
		match(';', ss);
		if (ctx.beg_label.empty()) {
			error("Cannot continue outside of loop", ss, true);
		}
		emit_br(ctx.beg_label);
	} else if (nxt_tok(ss).type == tok_break) {
		match(tok_break, ss);
		match(';', ss);
		if (ctx.end_label.empty()) {
			error("Cannot break outside of loop or switch", ss,
			      true);
		}
		emit_br(ctx.end_label);
	} else if (nxt_tok(ss).type == tok_return) {
		match(tok_return, ss);
		if (nxt_tok(ss).type == ';') {
			match(';', ss);
			if (is_type_void(ctx.fun_env->ret_type)) {
				emit_ret();
			} else {
				var_t zero;
				zero.type = std::make_shared<type_t>(
					ctx.fun_env->ret_type);
				zero.name = "zeroinitializer";
				emit_ret(zero);
			}
		} else {
			var_t rs = expression(ss, ctx);
			match(';', ss);
			if (rs.is_alloced) {
				rs = emit_load(rs);
			}
			if (ctx.fun_env->ret_type.type == type_t::type_basic &&
			    rs.type->type == type_t::type_basic) {
				rs = emit_conv_to(rs, ctx.fun_env->ret_type);
				emit_ret(rs);
			} else if (ctx.fun_env->ret_type == *rs.type) {
				emit_ret(rs);
			} else {
				error("Func return type not match.", ss, true);
			}
		}
	}
}

/*
selection_statement
	{ IF '(' expression ')' statement {ELSE statement}?
	}
*/
bool is_selection_statement(tok_t tok)
{
	return tok.type == tok_if || tok.type == tok_switch;
}
void selection_statement(stream &ss, context_t &ctx)
{
	anal_debug();

	string label_true = get_label();
	string label_false = get_label();
	string label_end = get_label();
	match(tok_if, ss);
	match('(', ss);
	var_t rs = expression(ss, ctx);
	if (rs.is_alloced) {
		rs = emit_load(rs);
	}
	if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
	    !is_type_p(*rs.type)) {
		error("Cannot use non-basic type as condition", ss, true);
	}
	if (!rs.type->is_bool) {
		var_t zero;
		zero.type = rs.type;
		zero.name = "zeroinitializer";
		rs = emit_ne(rs, zero);
	}
	match(')', ss);
	emit_br(rs, label_true, label_false);

	emit_label(label_true);
	statement(ss, ctx);
	if (nxt_tok(ss).type != tok_else) {
		emit_label(label_false);
		return;
	}
	emit_br(label_end);
	match(tok_else, ss);
	emit_label(label_false);
	statement(ss, ctx);
	emit_label(label_end);
}

}