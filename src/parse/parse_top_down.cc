/**
 * @file parse_top_down.cc
 * @author 泠妄 (lingwang@wcysite.com)
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#include "parse/parse_base.hh"
#include <algorithm>
#include <cstddef>
#include <stack>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "scan.hh"
#include "tok.hh"
#include "out.hh"
#include "parse/parse_top_down.hh"
#include "gen.hh"
#include "util.hh"

namespace neko_cc
{
using std::make_shared;

std::unordered_map<size_t, type_t> glbl_type_map;

stream *out_ss;
string post_decl;

/*
translation_unit
	{external_declaration}+
*/
void translation_unit(stream &ss, stream &out)
{
	debug();

	out_ss = &out;
	post_decl = "";

	context_t ctx(nullptr, {}, {});
	fun_env_t global_fun_env;
	global_fun_env.is_func = 0;
	ctx.fun_env = make_shared<fun_env_t>(global_fun_env);

	while (nxt_tok(ss).type != tok_eof) {
		external_declaration(ss, ctx);
	}

	*out_ss << post_decl;
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
	debug();

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
	debug();

	if (nxt_tok(ss).type != '{') {
		error("Compound statement expected", ss, true);
	}

	function.name = get_global_name(function.name);
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

		i.name = get_local_name(i.name);

		input_args.push_back(arg_var);
	}
	fun_env_t fun_env;
	fun_env.is_func = 1;
	fun_env.ret_type = *function.type->ret_type;
	ctx_func.fun_env = make_shared<fun_env_t>(fun_env);
	auto emit_tmp = emit_func_begin(function, input_args);
	*out_ss << emit_tmp.code;
	for (size_t i = 0; i < args.size(); i++) {
		auto tmp = emit_alloca(*input_args[i].type, input_args[i].name);
		args[i] = tmp.var;
		*out_ss << tmp.code;
		emit_tmp = emit_store(args[i], input_args[i]);
		*out_ss << emit_tmp.code;
		ctx_func.vars[args[i].name] = args[i];
	}
	compound_statement(ss, ctx_func);

	if (function.type->ret_type->type == type_t::type_basic) {
		if (is_type_void(*function.type->ret_type)) {
			auto tmp = emit_ret();
			*out_ss << tmp.code;
		} else {
			var_t ret_var;
			ret_var.name = "zeroinitializer";
			ret_var.type = function.type->ret_type;
			auto tmp = emit_ret(ret_var);
			*out_ss << tmp.code;
		}
	} else {
		var_t ret_var;
		ret_var.name = "zeroinitializer";
		ret_var.type = function.type->ret_type;
		auto tmp = emit_ret(ret_var);
		*out_ss << tmp.code;
	}
	emit_tmp = emit_func_end();
	*out_ss << emit_tmp.code;
}

/*
constant_expression
	conditional_expression
*/
int constant_expression(stream &ss, context_t &ctx)
{
	debug();

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
	debug();

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
	debug();

	if (is_type_void(type)) {
		error("Cannot declare void type variable", ss, true);
	}
	var.name = '@' + var.name;

	if (nxt_tok(ss).type == '=') {
		match('=', ss);
		initializer(ss, ctx, var);
	} else if (var.type->type == type_t::type_func) {
		auto tmp = emit_global_func_decl(var);
		*out_ss << tmp.code;
		var = tmp.var;
	} else {
		auto tmp = emit_global_decl(var);
		*out_ss << tmp.code;
		var = tmp.var;
	}
	ctx.vars[var.name] = var;

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
	debug();

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
	debug();

	var_t var;
	std::vector<var_t> args =
		declarator(ss, ctx, make_shared<type_t>(type), var);

	if (ctx.fun_env->is_func) {
		var.name = '%' + var.name;

	} else {
		var.name = '@' + var.name;
	}

	if (nxt_tok(ss).type == '=') {
		match('=', ss);
		initializer(ss, ctx, var);
	} else {
		if (ctx.fun_env->is_func) {
			auto tmp = emit_alloca(*var.type, var.name);
			*out_ss << tmp.code;
			var = tmp.var;
		} else {
			auto tmp = emit_global_decl(var);
			*out_ss << tmp.code;
			var = tmp.var;
		}
		ctx.vars[var.name] = var;
	}
}

/*
storage_class_specifier
	{TYPEDEF | EXTERN | STATIC | AUTO | REGISTER}
*/
void strong_class_specfifer(stream &ss, context_t &unused(ctx), type_t &type)
{
	debug();

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
void type_specifier(stream &ss, context_t &ctx, type_t &type)
{
	debug();

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
	debug();

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
void type_qualifier(stream &ss, context_t &unused(ctx), type_t &type)
{
	debug();

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
	debug();

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
	debug();

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
	debug();

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
	debug();

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
static bool chk_if_abstract_nested(stream &ss, context_t &ctx)
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
	debug();

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
	debug();

	return parameter_list(ss, ctx);
}

/*
parameter_list
	parameter_declaration {',' parameter_declaration}*
*/
std::vector<var_t> parameter_list(stream &ss, context_t &ctx)
{
	debug();

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
	debug();

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
	debug();

	if (nxt_tok(ss).type == '{') {
		throw "not implemented";
		// match('{', ss);
		// initializer_list(ss, ctx, var);
		// match('}', ss);
	} else {
		if (ctx.fun_env->is_func) {
			auto tmp = emit_alloca(*var.type, var.name);
			*out_ss << tmp.code;
			var = tmp.var;
			ctx.vars[var.name] = var;
			var_t init_var = assignment_expression(ss, ctx);
			auto emit_tmp = emit_store(var, init_var);
			*out_ss << emit_tmp.code;
		} else {
			if (nxt_tok(ss).type != tok_int_lit &&
			    nxt_tok(ss).type != tok_float_lit &&
			    nxt_tok(ss).type != tok_string_lit &&
			    nxt_tok(ss).type != tok_ident) {
				error("Initializer must be constant", ss, true);
			}
			if (nxt_tok(ss).type == tok_ident &&
			    ctx.get_enum(nxt_tok(ss).str) == -1) {
				error("Initializer must be constant", ss, true);
			}
			if (nxt_tok(ss).type == tok_int_lit ||
			    nxt_tok(ss).type == tok_ident) {
				int init_val;
				if (is_type_i(*var.type)) {
					init_val = std::stoi(get_tok(ss).str);
				} else {
					init_val =
						ctx.get_enum(get_tok(ss).str);
				}
				if (is_type_i(*var.type)) {
					auto tmp = emit_global_decl(
						var, std::to_string(init_val));
					*out_ss << tmp.code;
					var = tmp.var;
				} else if (is_type_f(*var.type)) {
					auto tmp = emit_global_decl(
						var, std::to_string(init_val) +
							     ".0");
					*out_ss << tmp.code;
					var = tmp.var;
				} else {
					error("Initializer type mismatch", ss,
					      true);
				}
			} else if (nxt_tok(ss).type == tok_float_lit) {
				string init_val = get_tok(ss).str;
				if (is_type_i(*var.type)) {
					auto tmp = emit_global_decl(
						var,
						std::to_string((int)std::stof(
							init_val)));
					*out_ss << tmp.code;
					var = tmp.var;
				} else if (is_type_f(*var.type)) {
					auto tmp =
						emit_global_decl(var, init_val);
					*out_ss << tmp.code;
					var = tmp.var;
				} else {
					error("Initializer type mismatch", ss,
					      true);
				}
			} else if (nxt_tok(ss).type == tok_string_lit) {
				std::string init_val = get_tok(ss).str;
				if (is_type_i(*var.type)) {
					error("Initializer type mismatch", ss,
					      true);
				} else if (is_type_f(*var.type)) {
					error("Initializer type mismatch", ss,
					      true);
				} else {
					auto tmp =
						emit_global_decl(var, init_val);
					*out_ss << tmp.code;
					var = tmp.var;
				}
			}
			ctx.vars[var.name] = var;
		}
	}
}

/*
struct_declaration_list
	{struct_declaration}+
*/
void struct_declaration_list(stream &ss, context_t &ctx, type_t &struct_type)
{
	debug();

	while (nxt_tok(ss).type != '}') {
		struct_declaration(ss, ctx, struct_type);
	}

	size_t align_to = 1;
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
	debug();

	type_t decl_type;
	specifier_qualifier_list(ss, ctx, decl_type);
	if (decl_type.type == type_t::type_basic) {
		try_regulate_basic(ss, decl_type);
	}
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
void specifier_qualifier_list(stream &ss, context_t &ctx, type_t &type)
{
	debug();

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
	debug();

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

/*
enum_specifier
	ENUM '{' enumerator_list '}'
	| ENUM IDENTIFIER '{' enumerator_list '}'
	| ENUM IDENTIFIER
*/
void enum_specifier(stream &ss, context_t &ctx, type_t &type)
{
	debug();

	if (nxt_tok(ss).type != tok_enum) {
		error("enum expected", ss, true);
	}
	match(tok_enum, ss);
	if (nxt_tok(ss).type == '{') {
		match('{', ss);
		enumerator_list(ss, ctx);
		match('}', ss);
	} else if (nxt_tok(ss).type == tok_ident) {
		tok_t tok = get_tok(ss);
		type.name = tok.str;
		if (nxt_tok(ss).type == '{') {
			match('{', ss);
			enumerator_list(ss, ctx);
			match('}', ss);
			ctx.types[type.name] = type;
		} else {
			type_t prev_type = ctx.get_type(type.name);
			if (prev_type.type == type_t::type_unknown) {
				error("Unknown enum type", ss, true);
			}
			if (prev_type.type != type_t::type_enum &&
			    !is_type_i(prev_type)) {
				error("Duplicate type specifier", ss, true);
			}
			prev_type.is_extern = type.is_extern;
			prev_type.is_static = type.is_static;
			prev_type.is_register = type.is_register;
			prev_type.is_const = type.is_const;
			prev_type.is_volatile = type.is_volatile;
			type = prev_type;
		}
	} else {
		error("Identifier or '{' expected", ss, true);
	}
	type_t i32_type;
	i32_type.name = "i32";
	i32_type.type = type_t::type_basic;
	i32_type.size = 4;
	i32_type.is_int = 1;
	i32_type.is_extern = type.is_extern;
	i32_type.is_static = type.is_static;
	i32_type.is_register = type.is_register;
	i32_type.is_const = type.is_const;
	i32_type.is_volatile = type.is_volatile;
	type = i32_type;
}

/*
enumerator_list
	enumerator {',' enumerator}*
*/
void enumerator_list(stream &ss, context_t &ctx)
{
	debug();

	int val = 0;
	enumerator(ss, ctx, val);
	while (nxt_tok(ss).type == ',') {
		match(',', ss);
		enumerator(ss, ctx, val);
	}
}

/*
enumerator
	IDENTIFIER {'=' constant_expression}
*/
void enumerator(stream &ss, context_t &ctx, int &val)
{
	debug();

	if (nxt_tok(ss).type != tok_ident) {
		error("Identifier expected", ss, true);
	}
	tok_t tok = get_tok(ss);
	if (nxt_tok(ss).type == '=') {
		match('=', ss);
		val = constant_expression(ss, ctx);
	}
	ctx.enums[tok.str] = val;
	val++;
}

/*
compound_statement
	'{' {declaration_list | statement_list}* '}'
*/
void compound_statement(stream &ss, context_t &ctx)
{
	debug();

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
	debug();

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
	debug();

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
	debug();

	if (nxt_tok(ss).type == '{') {
		context_t nctx(&ctx);
		nctx.beg_label = ctx.beg_label;
		nctx.end_label = ctx.end_label;
		compound_statement(ss, nctx);
	} else if (is_selection_statement(nxt_tok(ss))) {
		selection_statement(ss, ctx);
	} else if (is_iteration_statement(nxt_tok(ss))) {
		iteration_statement(ss, ctx);
	} else if (is_jump_statement(nxt_tok(ss))) {
		jump_statement(ss, ctx);
	} else {
		expression_statement(ss, ctx);
	}
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
var_t unary_expression(stream &ss, context_t &ctx)
{
	debug();

	if (nxt_tok(ss).type == tok_inc) {
		match(tok_inc, ss);
		var_t rs = unary_expression(ss, ctx);
		if (!rs.is_alloced) {
			error("Cannot increment rvalue", ss, true);
		}
		auto emit_tmp = emit_load(rs);
		*out_ss << emit_tmp.code;
		var_t tmp = emit_tmp.var;
		if (is_type_i(*tmp.type)) {
			var_t inc_v;
			inc_v.type = tmp.type;
			inc_v.name = '1';
			emit_tmp = emit_add(tmp, inc_v);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
		} else if (is_type_p(*tmp.type)) {
			type_t ori_type = *tmp.type;
			type_t i64_type;
			i64_type.name = "i64";
			i64_type.type = type_t::type_basic;
			i64_type.size = 8;
			i64_type.is_long = 2;
			emit_tmp = emit_ptrtoint(tmp, i64_type);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
			var_t inc_v;
			inc_v.type = make_shared<type_t>(i64_type);
			inc_v.name = '8';
			emit_tmp = emit_add(tmp, inc_v);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
			emit_tmp = emit_inttoptr(tmp, ori_type);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
		} else if (is_type_f(*tmp.type)) {
			var_t inc_v;
			inc_v.type = tmp.type;
			inc_v.name = "1.0";
			emit_tmp = emit_fadd(tmp, inc_v);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
		} else {
			error("Cannot increment this type", ss, true);
		}
		emit_tmp = emit_store(rs, tmp);
		*out_ss << emit_tmp.code;
		return rs;
	}
	if (nxt_tok(ss).type == tok_dec) {
		match(tok_dec, ss);
		var_t rs = unary_expression(ss, ctx);
		if (!rs.is_alloced) {
			error("Cannot decrement rvalue", ss, true);
		}
		auto emit_tmp = emit_load(rs);
		*out_ss << emit_tmp.code;
		var_t tmp = emit_tmp.var;
		if (is_type_i(*tmp.type)) {
			var_t inc_v;
			inc_v.type = tmp.type;
			inc_v.name = '1';
			emit_tmp = emit_sub(tmp, inc_v);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
		} else if (is_type_p(*tmp.type)) {
			type_t ori_type = *tmp.type;
			type_t i64_type;
			i64_type.name = "i64";
			i64_type.type = type_t::type_basic;
			i64_type.size = 8;
			i64_type.is_long = 2;
			auto emit_tmp = emit_ptrtoint(tmp, i64_type);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
			var_t inc_v;
			inc_v.type = make_shared<type_t>(i64_type);
			inc_v.name = '8';
			emit_tmp = emit_sub(tmp, inc_v);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
			emit_tmp = emit_inttoptr(tmp, ori_type);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
		} else if (is_type_f(*tmp.type)) {
			var_t inc_v;
			inc_v.type = tmp.type;
			inc_v.name = "1.0";
			emit_tmp = emit_fsub(tmp, inc_v);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
		} else {
			error("Cannot decrement this type", ss, true);
		}
		emit_tmp = emit_store(rs, tmp);
		*out_ss << emit_tmp.code;
		return rs;
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
			// if (tmp.is_alloced) {
			// 	emit_tmp =  emit_load(tmp);*out_ss<<emit_tmp.code;tmp=emit_tmp.var;
			// }
			if (!is_type_p(*tmp.type)) {
				error("Cannot dereference non-pointer type", ss,
				      true);
			}
			if (tmp.type->ptr_to->type == type_t::type_func) {
				return tmp;
			}
			auto emit_tmp = emit_load(tmp);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
			tmp.is_alloced = true;
			return tmp;
		}
		if (nxt_tok(ss).type == '+') {
			match('+', ss);
			var_t tmp = cast_expression(ss, ctx);
			if (tmp.is_alloced) {
				auto emit_tmp = emit_load(tmp);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
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
				auto emit_tmp = emit_load(tmp);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			}
			if (tmp.type->type != type_t::type_basic) {
				error("Cannot use unary '-' on non-basic type",
				      ss, true);
			}
			if (is_type_i(*tmp.type)) {
				var_t zero;
				zero.type = tmp.type;
				zero.name = '0';
				auto emit_tmp = emit_sub(zero, tmp);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			} else if (is_type_f(*tmp.type)) {
				var_t zero;
				zero.type = tmp.type;
				zero.name = "0.0";
				auto emit_tmp = emit_fsub(zero, tmp);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
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
				auto emit_tmp = emit_load(tmp);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			}
			if (!is_type_i(*tmp.type)) {
				error("Cannot use unary '~' on non-basic type",
				      ss, true);
			}

			var_t minus_one;
			minus_one.type = tmp.type;
			minus_one.name = "-1";
			auto emit_tmp = emit_xor(tmp, minus_one);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
			return tmp;
		}
		if (nxt_tok(ss).type == '!') {
			match('!', ss);
			var_t tmp = cast_expression(ss, ctx);
			if (tmp.is_alloced) {
				auto emit_tmp = emit_load(tmp);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			}
			if (!is_type_i(*tmp.type)) {
				error("Cannot use unary '!' on non-basic type",
				      ss, true);
			}
			var_t zero;
			zero.type = tmp.type;
			zero.name = '0';
			auto emit_tmp = emit_eq(tmp, zero);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
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
			auto emit_tmp = emit_load(tmp);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
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
bool is_cast_expression(stream &ss, context_t &ctx)
{
	if (nxt_tok(ss).type != '(') {
		return false;
	}
	std::stack<tok_t> store;
	store.push(get_tok(ss));
	if (!is_specifier_qualifier_list(nxt_tok(ss), ctx)) {
		while (!store.empty()) {
			unget_tok(store.top());
			store.pop();
		}
		return false;
	}
	while (!store.empty()) {
		unget_tok(store.top());
		store.pop();
	}
	return true;
}
var_t cast_expression(stream &ss, context_t &ctx)
{
	debug();

	if (is_cast_expression(ss, ctx)) {
		match('(', ss);
		type_t type = type_name(ss, ctx);
		match(')', ss);
		var_t tmp = cast_expression(ss, ctx);
		if (tmp.is_alloced) {
			auto emit_tmp = emit_load(tmp);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
		}
		if (type.type != type_t::type_basic &&
		    type.type != type_t::type_pointer) {
			error("Cannot cast to non-basic type", ss, true);
		}
		auto emit_tmp = emit_conv_to(tmp, type);
		*out_ss << emit_tmp.code;
		var_t ret = emit_tmp.var;
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
	debug();

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
	debug();

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
			var.type->is_int = 1;
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
		var.type->is_int = 1;
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
		var.type->is_double = 1;
		var.name = tok.str;
		return var;
	}
	if (nxt_tok(ss).type == tok_null) {
		match(tok_null, ss);
		var_t var;
		var.type = make_shared<type_t>();
		var.type->type = type_t::type_pointer;
		var.type->name = "null";
		var.type->size = 8;
		var.name = "null";
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
		auto tmp = emit_global_const_decl(var, init_str);
		post_decl += tmp.code;
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
var_t postfix_expression(stream &ss, context_t &ctx)
{
	var_t tmp = primary_expression(ss, ctx);
	while (is_postfix_expression_op(nxt_tok(ss))) {
		if (nxt_tok(ss).type == '[') {
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
			auto emit_tmp = get_item_from_arrptr(tmp, idx);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
		} else if (nxt_tok(ss).type == '(') {
			match('(', ss);
			std::vector<var_t> args;
			if (nxt_tok(ss).type != ')') {
				args = argument_expression_list(ss, ctx);
			}
			if (!(tmp.type->type == type_t::type_func ||
			      (tmp.type->type == type_t::type_pointer &&
			       tmp.type->ptr_to->type == type_t::type_func))) {
				error("Cannot call non-function type", ss,
				      true);
			}
			match(')', ss);
			auto emit_tmp = emit_call(tmp, args);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
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
				     i <
				     (int)tmp.type->ptr_to->inner_vars.size();
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
				auto emit_tmp =
					get_item_from_structptr(tmp, offset);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
				tmp.is_alloced = true;
			} else {
				if (tmp.type->type != type_t::type_struct) {
					error("Cannot access member of non-struct type",
					      ss, true);
				}
				int offset = -1;
				for (int i = 0;
				     i < (int)tmp.type->inner_vars.size();
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
				auto emit_tmp =
					get_item_from_structobj(tmp, offset);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			}
		} else if (nxt_tok(ss).type == tok_pointer) {
			match(tok_pointer, ss);
			tok_t tok = get_tok(ss);
			if (tok.type != tok_ident) {
				error("Identifier expected", ss, true);
			}
			if (tmp.is_alloced) {
				auto emit_tmp = emit_load(tmp);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			}
			if (tmp.type->type != type_t::type_pointer ||
			    tmp.type->ptr_to->type != type_t::type_struct) {
				error("Cannot access member of non-struct type",
				      ss, true);
			}
			int offset = -1;
			for (int i = 0;
			     i < (int)tmp.type->ptr_to->inner_vars.size();
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
			auto emit_tmp = get_item_from_structptr(tmp, offset);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
			tmp.is_alloced = true;
		} else if (nxt_tok(ss).type == tok_inc) {
			if (!tmp.is_alloced) {
				error("Cannot increment rvalue", ss, true);
			}
			match(tok_inc, ss);
			var_t ori_var = tmp;
			auto emit_tmp = emit_load(tmp);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
			var_t ret_var = tmp;
			if (is_type_i(*tmp.type)) {
				var_t inc_v;
				inc_v.type = tmp.type;
				inc_v.name = '1';
				emit_tmp = emit_add(tmp, inc_v);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			} else if (is_type_p(*tmp.type)) {
				type_t ori_type = *tmp.type;
				type_t i64_type;
				i64_type.name = "i64";
				i64_type.type = type_t::type_basic;
				i64_type.size = 8;
				i64_type.is_long = 2;
				emit_tmp = emit_ptrtoint(tmp, i64_type);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
				var_t inc_v;
				inc_v.type = make_shared<type_t>(i64_type);
				inc_v.name = '8';
				emit_tmp = emit_add(tmp, inc_v);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
				emit_tmp = emit_inttoptr(tmp, ori_type);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			} else if (is_type_f(*tmp.type)) {
				var_t inc_v;
				inc_v.type = tmp.type;
				inc_v.name = "1.0";
				emit_tmp = emit_fadd(tmp, inc_v);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			} else {
				error("Cannot increment this type", ss, true);
			}
			emit_tmp = emit_store(ori_var, tmp);
			*out_ss << emit_tmp.code;
			tmp = ret_var;
		} else if (nxt_tok(ss).type == tok_dec) {
			if (!tmp.is_alloced) {
				error("Cannot increment rvalue", ss, true);
			}
			match(tok_dec, ss);
			var_t ori_var = tmp;
			auto emit_tmp = emit_load(tmp);
			*out_ss << emit_tmp.code;
			tmp = emit_tmp.var;
			var_t ret_var = tmp;
			if (is_type_i(*tmp.type)) {
				var_t inc_v;
				inc_v.type = tmp.type;
				inc_v.name = '1';
				emit_tmp = emit_sub(tmp, inc_v);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			} else if (is_type_p(*tmp.type)) {
				type_t ori_type = *tmp.type;
				type_t i64_type;
				i64_type.name = "i64";
				i64_type.type = type_t::type_basic;
				i64_type.size = 8;
				i64_type.is_long = 2;
				auto emit_tmp = emit_ptrtoint(tmp, i64_type);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
				var_t inc_v;
				inc_v.type = make_shared<type_t>(i64_type);
				inc_v.name = '8';
				emit_tmp = emit_sub(tmp, inc_v);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
				emit_tmp = emit_inttoptr(tmp, ori_type);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			} else if (is_type_f(*tmp.type)) {
				var_t inc_v;
				inc_v.type = tmp.type;
				inc_v.name = "1.0";
				emit_tmp = emit_fsub(tmp, inc_v);
				*out_ss << emit_tmp.code;
				tmp = emit_tmp.var;
			} else {
				error("Cannot decrement this type", ss, true);
			}
			emit_tmp = emit_store(ori_var, tmp);
			*out_ss << emit_tmp.code;
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
	debug();

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
var_t multiplicative_expression(stream &ss, context_t &ctx)
{
	debug();

	var_t rs = cast_expression(ss, ctx);
	while (is_mul_op(nxt_tok(ss))) {
		if (nxt_tok(ss).type == '*') {
			match('*', ss);
			var_t rt = cast_expression(ss, ctx);
			if (rs.is_alloced) {
				auto emit_tmp = emit_load(rs);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			}
			if (rt.is_alloced) {
				auto emit_tmp = emit_load(rt);
				*out_ss << emit_tmp.code;
				rt = emit_tmp.var;
			}
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
				error("Cannot multiply non-basic type", ss,
				      true);
			}
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
				error("Cannot multiply non-basic type", ss,
				      true);
			}
			auto emit_tmp = emit_match_type(rs, rt);
			*out_ss << emit_tmp.code;
			if (is_type_i(*rs.type)) {
				emit_tmp = emit_mul(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_f(*rs.type)) {
				emit_tmp = emit_fmul(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else {
				error("Cannot multiply this type", ss, true);
			}
		} else if (nxt_tok(ss).type == '/') {
			match('/', ss);
			var_t rt = cast_expression(ss, ctx);
			if (rs.is_alloced) {
				auto emit_tmp = emit_load(rs);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			}
			if (rt.is_alloced) {
				auto emit_tmp = emit_load(rt);
				*out_ss << emit_tmp.code;
				rt = emit_tmp.var;
			}
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
				error("Cannot divide non-basic type", ss, true);
			}
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
				error("Cannot divide non-basic type", ss, true);
			}
			auto emit_tmp = emit_match_type(rs, rt);
			*out_ss << emit_tmp.code;
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				emit_tmp = emit_udiv(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				emit_tmp = emit_sdiv(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_f(*rs.type)) {
				emit_tmp = emit_fdiv(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else {
				error("Cannot divide this type", ss, true);
			}
		} else if (nxt_tok(ss).type == '%') {
			match('/', ss);
			var_t rt = cast_expression(ss, ctx);
			if (rs.is_alloced) {
				auto emit_tmp = emit_load(rs);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			}
			if (rt.is_alloced) {
				auto emit_tmp = emit_load(rt);
				*out_ss << emit_tmp.code;
				rt = emit_tmp.var;
			}
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
				error("Cannot mod non-basic type", ss, true);
			}
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
				error("Cannot mod non-basic type", ss, true);
			}
			auto emit_tmp = emit_match_type(rs, rt);
			*out_ss << emit_tmp.code;
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				emit_tmp = emit_urem(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				emit_tmp = emit_srem(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_f(*rs.type)) {
				emit_tmp = emit_frem(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else {
				error("Cannot mod this type", ss, true);
			}
		}
	}
	return rs;
}

/*
additive_expression
	multiplicative_expression {('+' | '-') multiplicative_expression}*
*/
var_t additive_expression(stream &ss, context_t &ctx)
{
	debug();

	var_t rs = multiplicative_expression(ss, ctx);
	while (is_add_op(nxt_tok(ss))) {
		if (nxt_tok(ss).type == '+') {
			match('+', ss);
			var_t rt = multiplicative_expression(ss, ctx);
			if (rs.is_alloced) {
				auto emit_tmp = emit_load(rs);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			}
			if (rt.is_alloced) {
				auto emit_tmp = emit_load(rt);
				*out_ss << emit_tmp.code;
				rt = emit_tmp.var;
			}
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
				error("Cannot add non-basic type", ss, true);
			}
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
				error("Cannot add non-basic type", ss, true);
			}
			auto emit_tmp = emit_match_type(rs, rt);
			*out_ss << emit_tmp.code;
			if (is_type_i(*rs.type)) {
				emit_tmp = emit_add(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_f(*rs.type)) {
				emit_tmp = emit_fadd(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else {
				error("Cannot add this type", ss, true);
			}
		} else if (nxt_tok(ss).type == '-') {
			match('-', ss);
			var_t rt = multiplicative_expression(ss, ctx);
			if (rs.is_alloced) {
				auto emit_tmp = emit_load(rs);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			}
			if (rt.is_alloced) {
				auto emit_tmp = emit_load(rt);
				*out_ss << emit_tmp.code;
				rt = emit_tmp.var;
			}
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
				error("Cannot subtract non-basic type", ss,
				      true);
			}
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
				error("Cannot subtract non-basic type", ss,
				      true);
			}
			auto emit_tmp = emit_match_type(rs, rt);
			*out_ss << emit_tmp.code;
			if (is_type_i(*rs.type)) {
				emit_tmp = emit_sub(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_f(*rs.type)) {
				emit_tmp = emit_fsub(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
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
var_t shift_expression(stream &ss, context_t &ctx)
{
	debug();

	var_t rs = additive_expression(ss, ctx);
	while (is_shift_op(nxt_tok(ss))) {
		if (nxt_tok(ss).type == tok_lshift) {
			match(tok_lshift, ss);
			var_t rt = additive_expression(ss, ctx);
			if (rs.is_alloced) {
				auto emit_tmp = emit_load(rs);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			}
			if (rt.is_alloced) {
				auto emit_tmp = emit_load(rt);
				*out_ss << emit_tmp.code;
				rt = emit_tmp.var;
			}
			if (!is_type_i(*rs.type)) {
				error("Cannot left shift non-basic type", ss,
				      true);
			}
			if (!is_type_i(*rt.type)) {
				error("Cannot left shift non-basic type", ss,
				      true);
			}
			auto emit_tmp = emit_match_type(rs, rt);
			*out_ss << emit_tmp.code;
			emit_tmp = emit_shl(rs, rt);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		} else if (nxt_tok(ss).type == tok_rshift) {
			match(tok_rshift, ss);
			var_t rt = additive_expression(ss, ctx);
			if (rs.is_alloced) {
				auto emit_tmp = emit_load(rs);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			}
			if (rt.is_alloced) {
				auto emit_tmp = emit_load(rt);
				*out_ss << emit_tmp.code;
				rt = emit_tmp.var;
			}
			if (!is_type_i(*rs.type)) {
				error("Cannot right shift non-basic type", ss,
				      true);
			}
			if (!is_type_i(*rt.type)) {
				error("Cannot right shift non-basic type", ss,
				      true);
			}
			auto emit_tmp = emit_match_type(rs, rt);
			*out_ss << emit_tmp.code;
			if (rs.type->is_unsigned) {
				emit_tmp = emit_lshr(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else {
				emit_tmp = emit_ashr(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			}
		}
	}
	return rs;
}

/*
relational_expression
	shift_expression {('<' | '>' | LE_OP | GE_OP) shift_expression}*
*/
var_t relational_expression(stream &ss, context_t &ctx)
{
	debug();

	var_t rs = shift_expression(ss, ctx);
	while (is_rel_op(nxt_tok(ss))) {
		if (rs.is_alloced) {
			auto emit_tmp = emit_load(rs);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		if (!is_type_i(*rs.type) && !is_type_f(*rs.type)) {
			error("Cannot compare non-basic type", ss, true);
		}
		tok_t ntok = get_tok(ss);
		var_t rt = shift_expression(ss, ctx);
		if (rt.is_alloced) {
			auto emit_tmp = emit_load(rt);
			*out_ss << emit_tmp.code;
			rt = emit_tmp.var;
		}
		if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
			error("Cannot compare non-basic type", ss, true);
		}
		auto emit_tmp = emit_match_type(rs, rt);
		*out_ss << emit_tmp.code;
		if (ntok.type == '<') {
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				emit_tmp = emit_ult(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				emit_tmp = emit_slt(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_f(*rs.type)) {
				emit_tmp = emit_flt(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else {
				error("Cannot compare this type", ss, true);
			}
		} else if (ntok.type == '>') {
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				emit_tmp = emit_ugt(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				emit_tmp = emit_sgt(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_f(*rs.type)) {
				emit_tmp = emit_fgt(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else {
				error("Cannot compare this type", ss, true);
			}
		} else if (ntok.type == tok_ge) {
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				emit_tmp = emit_uge(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				emit_tmp = emit_sge(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_f(*rs.type)) {
				emit_tmp = emit_fge(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else {
				error("Cannot compare this type", ss, true);
			}
		} else if (ntok.type == tok_le) {
			if (is_type_i(*rs.type) && rs.type->is_unsigned) {
				emit_tmp = emit_ule(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_i(*rs.type) &&
				   !rs.type->is_unsigned) {
				emit_tmp = emit_sle(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_f(*rs.type)) {
				emit_tmp = emit_fle(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
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
var_t equality_expression(stream &ss, context_t &ctx)
{
	debug();

	var_t rs = relational_expression(ss, ctx);
	while (is_eq_op(nxt_tok(ss))) {
		if (rs.is_alloced) {
			auto emit_tmp = emit_load(rs);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
		    !is_type_p(*rs.type)) {
			error("Cannot compare non-basic type", ss, true);
		}
		tok_t ntok = get_tok(ss);
		var_t rt = relational_expression(ss, ctx);
		if (rt.is_alloced) {
			auto emit_tmp = emit_load(rt);
			*out_ss << emit_tmp.code;
			rt = emit_tmp.var;
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
			auto emit_tmp = emit_ptrtoint(rs, i64_type);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
			emit_tmp = emit_ptrtoint(rt, i64_type);
			*out_ss << emit_tmp.code;
			rt = emit_tmp.var;
		} else if (!is_type_p(*rs.type) && !is_type_p(*rt.type)) {
			auto emit_tmp = emit_match_type(rs, rt);
			*out_ss << emit_tmp.code;
		} else {
			error("Cannot compare pointer with non-pointer", ss,
			      true);
		}
		if (ntok.type == tok_eq) {
			if (is_type_i(*rs.type)) {
				auto emit_tmp = emit_eq(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_f(*rs.type)) {
				auto emit_tmp = emit_feq(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else {
				error("Cannot compare this type", ss, true);
			}
		} else if (ntok.type == tok_ne) {
			if (is_type_i(*rs.type)) {
				auto emit_tmp = emit_ne(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			} else if (is_type_f(*rs.type)) {
				auto emit_tmp = emit_fne(rs, rt);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
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
	debug();

	var_t rs = equality_expression(ss, ctx);
	while (nxt_tok(ss).type == '&') {
		match('&', ss);
		var_t rt = equality_expression(ss, ctx);
		if (rs.is_alloced) {
			auto emit_tmp = emit_load(rs);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		if (rt.is_alloced) {
			auto emit_tmp = emit_load(rt);
			*out_ss << emit_tmp.code;
			rt = emit_tmp.var;
		}
		if (!is_type_i(*rs.type)) {
			error("Cannot and non-int type", ss, true);
		}
		if (!is_type_i(*rt.type)) {
			error("Cannot and non-int type", ss, true);
		}
		auto emit_tmp = emit_match_type(rs, rt);
		*out_ss << emit_tmp.code;
		emit_tmp = emit_and(rs, rt);
		*out_ss << emit_tmp.code;
		rs = emit_tmp.var;
	}
	return rs;
}

/*
exclusive_or_expression
	and_expression { '^' and_expression }*
*/
var_t exclusive_or_expression(stream &ss, context_t &ctx)
{
	debug();

	var_t rs = and_expression(ss, ctx);
	while (nxt_tok(ss).type == '^') {
		match('^', ss);
		var_t rt = and_expression(ss, ctx);
		if (rs.is_alloced) {
			auto emit_tmp = emit_load(rs);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		if (rt.is_alloced) {
			auto emit_tmp = emit_load(rt);
			*out_ss << emit_tmp.code;
			rt = emit_tmp.var;
		}
		if (!is_type_i(*rs.type)) {
			error("Cannot xor non-int type", ss, true);
		}
		if (!is_type_i(*rt.type)) {
			error("Cannot xor non-int type", ss, true);
		}
		auto emit_tmp = emit_match_type(rs, rt);
		*out_ss << emit_tmp.code;
		emit_tmp = emit_xor(rs, rt);
		*out_ss << emit_tmp.code;
		rs = emit_tmp.var;
	}
	return rs;
}

/*
inclusive_or_expression
	exclusive_or_expression { '|' exclusive_or_expression }*
*/
var_t inclusive_or_expression(stream &ss, context_t &ctx)
{
	debug();

	var_t rs = exclusive_or_expression(ss, ctx);
	while (nxt_tok(ss).type == '|') {
		match('|', ss);
		var_t rt = exclusive_or_expression(ss, ctx);
		if (rs.is_alloced) {
			auto emit_tmp = emit_load(rs);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		if (rt.is_alloced) {
			auto emit_tmp = emit_load(rt);
			*out_ss << emit_tmp.code;
			rt = emit_tmp.var;
		}
		if (!is_type_i(*rs.type)) {
			error("Cannot or non-int type", ss, true);
		}
		if (!is_type_i(*rt.type)) {
			error("Cannot or non-int type", ss, true);
		}
		auto emit_tmp = emit_match_type(rs, rt);
		*out_ss << emit_tmp.code;
		emit_tmp = emit_or(rs, rt);
		*out_ss << emit_tmp.code;
		rs = emit_tmp.var;
	}
	return rs;
}

/*
logical_and_expression
	inclusive_or_expression {AND_OP inclusive_or_expression}*
*/
var_t logical_and_expression(stream &ss, context_t &ctx)
{
	debug();

	var_t rs = inclusive_or_expression(ss, ctx);
	while (nxt_tok(ss).type == tok_land) {
		match(tok_land, ss);
		var_t rt = inclusive_or_expression(ss, ctx);
		if (rs.is_alloced) {
			auto emit_tmp = emit_load(rs);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		if (rt.is_alloced) {
			auto emit_tmp = emit_load(rt);
			*out_ss << emit_tmp.code;
			rt = emit_tmp.var;
		}
		if (!rs.type->is_bool) {
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
			    !is_type_p(*rs.type)) {
				error("Cannot and non-basic type", ss, true);
			}
			var_t zero;
			zero.type = rs.type;
			zero.name = "zeroinitializer";
			auto emit_tmp = emit_ne(rs, zero);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		if (!rt.type->is_bool) {
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type) &&
			    !is_type_p(*rt.type)) {
				error("Cannot and non-basic type", ss, true);
			}
			var_t zero;
			zero.type = rt.type;
			zero.name = "zeroinitializer";
			auto emit_tmp = emit_ne(rt, zero);
			*out_ss << emit_tmp.code;
			rt = emit_tmp.var;
		}
		auto emit_tmp = emit_and(rs, rt);
		*out_ss << emit_tmp.code;
		rs = emit_tmp.var;
	}
	return rs;
}

/*
logical_or_expression
	logical_and_expression {OR_OP logical_and_expression}*
*/
var_t logical_or_expression(stream &ss, context_t &ctx)
{
	debug();

	var_t rs = logical_and_expression(ss, ctx);
	while (nxt_tok(ss).type == tok_lor) {
		match(tok_lor, ss);
		var_t rt = logical_and_expression(ss, ctx);
		if (rs.is_alloced) {
			auto emit_tmp = emit_load(rs);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		if (rt.is_alloced) {
			auto emit_tmp = emit_load(rt);
			*out_ss << emit_tmp.code;
			rt = emit_tmp.var;
		}
		if (!rs.type->is_bool) {
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
			    !is_type_p(*rs.type)) {
				error("Cannot or non-basic type", ss, true);
			}
			var_t zero;
			zero.type = rs.type;
			zero.name = "zeroinitializer";
			auto emit_tmp = emit_ne(rs, zero);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		if (!rt.type->is_bool) {
			if (!is_type_i(*rt.type) && !is_type_f(*rt.type) &&
			    !is_type_p(*rt.type)) {
				error("Cannot or non-basic type", ss, true);
			}
			var_t zero;
			zero.type = rt.type;
			zero.name = "zeroinitializer";
			auto emit_tmp = emit_ne(rt, zero);
			*out_ss << emit_tmp.code;
			rt = emit_tmp.var;
		}
		auto emit_tmp = emit_or(rs, rt);
		*out_ss << emit_tmp.code;
		rs = emit_tmp.var;
	}
	return rs;
}

/*
conditional_expression
	logical_or_expression {'?' expression ':' conditional_expression}?
*/
var_t conditional_expression(stream &ss, context_t &ctx)
{
	debug();

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
			auto emit_tmp = emit_ne(rs, zero);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		match('?', ss);
		string label_true = get_label();
		string label_false = get_label();
		string label_true_conv = get_label();
		string label_false_conv = get_label();
		string label_end = get_label();
		auto emit_tmp = emit_br(rs, label_true, label_false);
		*out_ss << emit_tmp.code;

		emit_tmp = emit_label(label_true);
		*out_ss << emit_tmp.code;

		var_t rt = expression(ss, ctx);
		if (rt.is_alloced) {
			emit_tmp = emit_load(rt);
			*out_ss << emit_tmp.code;
			rt = emit_tmp.var;
		}
		emit_tmp = emit_br(label_true_conv);
		*out_ss << emit_tmp.code;

		match(':', ss);
		emit_tmp = emit_label(label_false);
		*out_ss << emit_tmp.code;

		var_t rr = conditional_expression(ss, ctx);
		if (rr.is_alloced) {
			emit_tmp = emit_load(rr);
			*out_ss << emit_tmp.code;
			rr = emit_tmp.var;
		}
		emit_tmp = emit_br(label_false_conv);
		*out_ss << emit_tmp.code;

		bool conv_to_rt = should_conv_to_first(rt, rr);
		bool conv_to_rr = should_conv_to_first(rr, rt);

		emit_tmp = emit_label(label_true_conv);
		*out_ss << emit_tmp.code;
		if (conv_to_rt) {
			emit_tmp = emit_conv_to(rt, *rr.type);
			*out_ss << emit_tmp.code;
			rr = emit_tmp.var;
		}
		emit_tmp = emit_br(label_end);
		*out_ss << emit_tmp.code;

		emit_tmp = emit_label(label_false_conv);
		*out_ss << emit_tmp.code;
		if (conv_to_rr) {
			emit_tmp = emit_conv_to(rr, *rt.type);
			*out_ss << emit_tmp.code;
			rr = emit_tmp.var;
		}
		emit_tmp = emit_br(label_end);
		*out_ss << emit_tmp.code;

		emit_tmp = emit_label(label_end);
		*out_ss << emit_tmp.code;
		emit_tmp = emit_phi(rt, label_true_conv, rr, label_false_conv);
		*out_ss << emit_tmp.code;
		rs = emit_tmp.var;
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
void assignment_operator(stream &ss, var_t rd, var_t rs, tok_t op)
{
	debug();

	if (!rd.is_alloced) {
		error("Cannot assign to lvalue", ss, true);
	}
	if (rs.is_alloced) {
		auto emit_tmp = emit_load(rs);
		*out_ss << emit_tmp.code;
		rs = emit_tmp.var;
	}
	if (op.type == '=') {
		auto emit_tmp = emit_conv_to(rs, *rd.type->ptr_to);
		*out_ss << emit_tmp.code;
		rs = emit_tmp.var;
		emit_tmp = emit_store(rd, rs);
		*out_ss << emit_tmp.code;
		return;
	}
	auto emit_tmp = emit_load(rd);
	*out_ss << emit_tmp.code;
	var_t rt = emit_tmp.var;
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
		emit_tmp = emit_ptrtoint(rs, i64_type);
		*out_ss << emit_tmp.code;
		rs = emit_tmp.var;
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
		emit_tmp = emit_ptrtoint(rt, i64_type);
		*out_ss << emit_tmp.code;
		var_t rt = emit_tmp.var;
	}
	if (!is_type_i(*rt.type) && !is_type_f(*rt.type)) {
		error("Cannot assign non-basic type", ss, true);
	}
	emit_tmp = emit_match_type(rs, rt);
	*out_ss << emit_tmp.code;
	if (op.type == tok_add_assign) {
		if (is_type_i(*rs.type)) {
			emit_tmp = emit_add(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		} else if (is_type_f(*rs.type)) {
			emit_tmp = emit_fadd(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		}
	} else if (op.type == tok_sub_assign) {
		if (is_type_i(*rs.type)) {
			emit_tmp = emit_sub(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		} else if (is_type_f(*rs.type)) {
			emit_tmp = emit_fsub(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		}
	} else if (op.type == tok_mul_assign) {
		if (is_type_i(*rs.type)) {
			emit_tmp = emit_mul(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		} else if (is_type_f(*rs.type)) {
			emit_tmp = emit_fmul(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		}
	} else if (op.type == tok_div_assign) {
		if (is_type_i(*rs.type) && rs.type->is_unsigned) {
			emit_tmp = emit_udiv(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		} else if (is_type_i(*rs.type) && !rs.type->is_unsigned) {
			emit_tmp = emit_sdiv(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		} else if (is_type_f(*rs.type)) {
			emit_tmp = emit_fdiv(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		}
	} else if (op.type == tok_mod_assign) {
		if (is_type_i(*rs.type) && rs.type->is_unsigned) {
			emit_tmp = emit_urem(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		} else if (is_type_i(*rs.type) && !rs.type->is_unsigned) {
			emit_tmp = emit_srem(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		} else if (is_type_f(*rs.type)) {
			emit_tmp = emit_frem(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		}
	} else if (op.type == tok_lshift_assign) {
		if (!is_type_i(*rs.type)) {
			error("Cannot left shift non-int type", ss, true);
		}
		emit_tmp = emit_shl(rt, rs);
		*out_ss << emit_tmp.code;
		var_t rt = emit_tmp.var;
	} else if (op.type == tok_rshift_assign) {
		if (!is_type_i(*rs.type)) {
			error("Cannot right shift non-int type", ss, true);
		}
		if (rt.type->is_unsigned) {
			emit_tmp = emit_lshr(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		} else {
			emit_tmp = emit_ashr(rt, rs);
			*out_ss << emit_tmp.code;
			var_t rt = emit_tmp.var;
		}
	} else if (op.type == tok_and_assign) {
		if (!is_type_i(*rs.type)) {
			error("Cannot and non-int type", ss, true);
		}
		emit_tmp = emit_and(rt, rs);
		*out_ss << emit_tmp.code;
		var_t rt = emit_tmp.var;
	} else if (op.type == tok_xor_assign) {
		if (!is_type_i(*rs.type)) {
			error("Cannot xor non-int type", ss, true);
		}
		emit_tmp = emit_xor(rt, rs);
		*out_ss << emit_tmp.code;
		var_t rt = emit_tmp.var;
	} else if (op.type == tok_or_assign) {
		if (!is_type_i(*rs.type)) {
			error("Cannot or non-int type", ss, true);
		}
		emit_tmp = emit_or(rt, rs);
		*out_ss << emit_tmp.code;
		var_t rt = emit_tmp.var;
	}
	emit_tmp = emit_conv_to(rt, *rd.type->ptr_to);
	*out_ss << emit_tmp.code;
	rt = emit_tmp.var;
	emit_tmp = emit_store(rd, rt);
	*out_ss << emit_tmp.code;
}

/*
assignment_expression
	{ conditional_expression
	| unary_expression assignment_operator assignment_expression
	}
*/
var_t assignment_expression(stream &ss, context_t &ctx)
{
	debug();

	// though if here has an assignment operator, we need to call the
	// unary expression; but the r_value can only come directily from
	// the unary expression, so we cal call the condition expression
	// to read the first part, then check if it is an rvalue
	var_t rd = conditional_expression(ss, ctx);
	if (is_assignment_operatior(nxt_tok(ss))) {
		if (!rd.is_alloced) {
			error("Cannot assign to rvalue", ss, true);
		}
		tok_t op = get_tok(ss);
		var_t rs = assignment_expression(ss, ctx);
		assignment_operator(ss, rd, rs, op);
	}

	return rd;
}

/*
expression
	assignment_expression {',' assignment_expression}*
*/
var_t expression(stream &ss, context_t &ctx)
{
	debug();

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
	debug();

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
void jump_statement(stream &ss, context_t &ctx)
{
	debug();

	if (!is_jump_statement(nxt_tok(ss))) {
		error("Not a jump statement", ss, true);
	}

	if (nxt_tok(ss).type == tok_continue) {
		match(tok_continue, ss);
		match(';', ss);
		if (ctx.beg_label.empty()) {
			error("Cannot continue outside of loop", ss, true);
		}
		auto emit_tmp = emit_br(ctx.beg_label);
		*out_ss << emit_tmp.code;

	} else if (nxt_tok(ss).type == tok_break) {
		match(tok_break, ss);
		match(';', ss);
		if (ctx.end_label.empty()) {
			error("Cannot break outside of loop or switch", ss,
			      true);
		}
		auto emit_tmp = emit_br(ctx.end_label);
		*out_ss << emit_tmp.code;

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
				auto emit_tmp = emit_load(rs);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			}
			if (ctx.fun_env->ret_type.type == type_t::type_basic &&
			    rs.type->type == type_t::type_basic) {
				auto emit_tmp =
					emit_conv_to(rs, ctx.fun_env->ret_type);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
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
void selection_statement(stream &ss, context_t &ctx)
{
	debug();

	string label_true = get_label();
	string label_false = get_label();
	string label_end = get_label();
	match(tok_if, ss);
	match('(', ss);
	var_t rs = expression(ss, ctx);
	if (rs.is_alloced) {
		auto emit_tmp = emit_load(rs);
		*out_ss << emit_tmp.code;
		rs = emit_tmp.var;
	}
	if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
	    !is_type_p(*rs.type)) {
		error("Cannot use non-basic type as condition", ss, true);
	}
	if (!rs.type->is_bool) {
		var_t zero;
		zero.type = rs.type;
		zero.name = "zeroinitializer";
		auto emit_tmp = emit_ne(rs, zero);
		*out_ss << emit_tmp.code;
		rs = emit_tmp.var;
	}
	match(')', ss);
	auto emit_tmp = emit_br(rs, label_true, label_false);
	*out_ss << emit_tmp.code;

	emit_tmp = emit_label(label_true);
	*out_ss << emit_tmp.code;

	statement(ss, ctx);
	if (nxt_tok(ss).type != tok_else) {
		emit_tmp = emit_label(label_false);
		*out_ss << emit_tmp.code;

		return;
	}
	emit_tmp = emit_br(label_end);
	*out_ss << emit_tmp.code;

	match(tok_else, ss);
	emit_tmp = emit_label(label_false);
	*out_ss << emit_tmp.code;

	statement(ss, ctx);
	emit_tmp = emit_br(label_end);
	*out_ss << emit_tmp.code;

	emit_tmp = emit_label(label_end);
	*out_ss << emit_tmp.code;
}

/*
iteration_statement
	{ WHILE '(' expression ')' statement
	| DO statement WHILE '(' expression ')' ';'
	| FOR '(' expression_statement expression_statement expression ')' statement
	}
*/
void iteration_statement(stream &ss, context_t &ctx)
{
	debug();

	if (nxt_tok(ss).type == tok_while) {
		match(tok_while, ss);
		string label_beg = get_label();
		string label_rbeg = get_label();
		string label_end = get_label();
		ctx.beg_label = label_beg;
		ctx.end_label = label_end;

		auto emit_tmp = emit_br(label_beg);
		*out_ss << emit_tmp.code;

		emit_tmp = emit_label(label_beg);
		*out_ss << emit_tmp.code;

		match('(', ss);
		var_t rs = expression(ss, ctx);
		if (rs.is_alloced) {
			emit_tmp = emit_load(rs);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
		    !is_type_p(*rs.type)) {
			error("Cannot use non-basic type as condition", ss,
			      true);
		}
		if (!rs.type->is_bool) {
			var_t zero;
			zero.type = rs.type;
			zero.name = "zeroinitializer";
			emit_tmp = emit_ne(rs, zero);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		match(')', ss);
		emit_tmp = emit_br(rs, label_rbeg, label_end);
		*out_ss << emit_tmp.code;

		emit_tmp = emit_label(label_rbeg);
		*out_ss << emit_tmp.code;

		statement(ss, ctx);
		emit_tmp = emit_br(label_beg);
		*out_ss << emit_tmp.code;

		emit_tmp = emit_label(label_end);
		*out_ss << emit_tmp.code;

		ctx.beg_label = "";
		ctx.end_label = "";
		return;
	}

	if (nxt_tok(ss).type == tok_do) {
		match(tok_do, ss);
		string label_beg = get_label();
		string label_end = get_label();
		ctx.beg_label = label_beg;
		ctx.end_label = label_end;

		auto emit_tmp = emit_br(label_beg);
		*out_ss << emit_tmp.code;

		emit_tmp = emit_label(label_beg);
		*out_ss << emit_tmp.code;

		statement(ss, ctx);
		match(tok_while, ss);
		match('(', ss);
		var_t rs = expression(ss, ctx);
		if (rs.is_alloced) {
			emit_tmp = emit_load(rs);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
		    !is_type_p(*rs.type)) {
			error("Cannot use non-basic type as condition", ss,
			      true);
		}
		if (!rs.type->is_bool) {
			var_t zero;
			zero.type = rs.type;
			zero.name = "zeroinitializer";
			emit_tmp = emit_ne(rs, zero);
			*out_ss << emit_tmp.code;
			rs = emit_tmp.var;
		}
		match(')', ss);
		match(';', ss);
		emit_tmp = emit_br(rs, label_beg, label_end);
		*out_ss << emit_tmp.code;

		emit_tmp = emit_label(label_end);
		*out_ss << emit_tmp.code;

		ctx.beg_label = "";
		ctx.end_label = "";
		return;
	}

	if (nxt_tok(ss).type == tok_for) {
		match(tok_for, ss);
		string label_beg = get_label();
		string label_rbeg = get_label();
		string label_cond = get_label();
		string label_end = get_label();

		context_t inner_ctx(&ctx);
		inner_ctx.beg_label = label_beg;
		inner_ctx.end_label = label_end;

		match('(', ss);
		// init expr
		expression_statement(ss, inner_ctx);
		auto emit_tmp = emit_br(label_cond);
		*out_ss << emit_tmp.code;

		// cond expr
		emit_tmp = emit_label(label_cond);
		*out_ss << emit_tmp.code;

		if (nxt_tok(ss).type == ';') {
			match(';', ss);
			emit_tmp = emit_br(label_rbeg);
			*out_ss << emit_tmp.code;

		} else {
			var_t rs = expression(ss, inner_ctx);
			if (rs.is_alloced) {
				emit_tmp = emit_load(rs);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			}
			if (!is_type_i(*rs.type) && !is_type_f(*rs.type) &&
			    !is_type_p(*rs.type)) {
				error("Cannot use non-basic type as condition",
				      ss, true);
			}
			if (!rs.type->is_bool) {
				var_t zero;
				zero.type = rs.type;
				zero.name = "zeroinitializer";
				emit_tmp = emit_ne(rs, zero);
				*out_ss << emit_tmp.code;
				rs = emit_tmp.var;
			}
			match(';', ss);
			emit_tmp = emit_br(rs, label_rbeg, label_end);
			*out_ss << emit_tmp.code;
		}

		// begin expr
		emit_tmp = emit_label(label_beg);
		*out_ss << emit_tmp.code;

		expression(ss, inner_ctx);
		match(')', ss);
		emit_tmp = emit_br(label_cond);
		*out_ss << emit_tmp.code;

		// repeat expr
		emit_tmp = emit_label(label_rbeg);
		*out_ss << emit_tmp.code;

		statement(ss, inner_ctx);
		emit_tmp = emit_br(label_beg);
		*out_ss << emit_tmp.code;

		emit_tmp = emit_label(label_end);
		*out_ss << emit_tmp.code;
	}
}
}