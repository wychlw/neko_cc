#include "gen.hh"
#include "parse/lr1/reduce.hh"
#include "parse/parse_base.hh"
#include "scan.hh"
#include "tok.hh"
#include "out.hh"
#include "util.hh"
#include <deque>
#include <string>

namespace neko_cc_reductions
{
using namespace neko_cc;
using namespace std;

env_t primary_expression_ident(deque<env_t> &env)
{
	auto tok = env[0].any->get<tok_t>();
	if (tok.type != tok_ident) {
		err_msg("Expected ident");
	}

	// try local var
	string real_name = '%' + tok.str;
	auto ctx = env[0].ctx;
	auto var = ctx->get_var(real_name);
	if (var.type->type != neko_cc::type_t::type_unknown) {
		return env_t{ ctx, make_shared<var_t>(var), make_any() };
	}

	// try global var
	real_name = '@' + tok.str;
	var = ctx->get_var(real_name);
	if (var.type->type != neko_cc::type_t::type_unknown) {
		return env_t{ ctx, make_shared<var_t>(var), make_any() };
	}

	// try enum constant
	real_name = tok.str;
	auto val = ctx->get_enum(real_name);
	if (val >= 0) {
		var_t var;
		var.type = make_shared<type_t>();
		var.type->type = type_t::type_basic;
		var.type->name = "i32";
		var.type->size = 4;
		var.type->is_int = 1;
		var.name = std::to_string(val);
		var.is_alloced = false;
		return env_t{ ctx, make_shared<var_t>(var), make_any() };
	}

	err_msg("Unknown ident: " + tok.str);
}
reg_sym(primary_expression_ident);

env_t primary_expression_int_lit(deque<env_t> &env)
{
	auto tok = env[0].any->get<tok_t>();
	if (tok.type != tok_int_lit) {
		err_msg("Expected int literal");
	}
	var_t var;
	var.type = make_shared<type_t>();
	var.type->type = type_t::type_basic;
	var.type->name = "i32";
	var.type->size = 4;
	var.type->is_int = 1;
	var.name = tok.str;
	var.is_alloced = false;
	return env_t{ env[0].ctx, make_shared<var_t>(var), make_any() };
}
reg_sym(primary_expression_int_lit);

env_t primary_expression_float_lit(deque<env_t> &env)
{
	auto tok = env[0].any->get<tok_t>();
	if (tok.type != tok_float_lit) {
		err_msg("Expected float literal");
	}
	var_t var;
	var.type = make_shared<type_t>();
	var.type->type = type_t::type_basic;
	var.type->name = "f64";
	var.type->size = 8;
	var.type->is_double = 1;
	var.name = tok.str;
	var.is_alloced = false;
	return env_t{ env[0].ctx, make_shared<var_t>(var), make_any() };
}
reg_sym(primary_expression_float_lit);

env_t primary_expression_string_lit(deque<env_t> &env)
{
	auto tok = env[0].any->get<tok_t>();
	if (tok.type != tok_string_lit) {
		err_msg("Expected string literal");
	}
	type_t char_type;
	char_type.type = type_t::type_basic;
	char_type.name = "i8";
	char_type.size = 1;
	char_type.is_char = 1;

	type_t init_arr_type;
	init_arr_type.type = type_t::type_array;
	init_arr_type.name = get_ptr_type_name(char_type.name);
	init_arr_type.size = char_type.size * tok.str.size();
	init_arr_type.ptr_to = make_shared<type_t>(char_type);

	var_t init_arr_var;
	init_arr_var.type = make_shared<type_t>(init_arr_type);
	init_arr_var.name = '@' + get_unnamed_var_name();

	string init_str;
	init_str += "c\"" + tok.str + "\\00\"";
	emit_post_const_decl(init_arr_var, init_str);

	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.name = get_ptr_type_name(char_type.name);
	ptr_type.size = 8;
	ptr_type.ptr_to = make_shared<type_t>(char_type);

	var_t ret_var;
	ret_var.type = make_shared<type_t>(ptr_type);
	ret_var.name = init_arr_var.name;
	ret_var.is_alloced = false;

	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(primary_expression_string_lit);

env_t primary_expression_null(deque<env_t> &env)
{
	auto tok = env[0].any->get<tok_t>();
	if (tok.type != tok_null) {
		err_msg("Expected null");
	}
	var_t var;
	var.type = make_shared<type_t>();
	var.type->type = type_t::type_pointer;
	var.type->name = "null";
	var.type->size = 8;
	var.name = "null";
	var.is_alloced = false;
	return env_t{ env[0].ctx, make_shared<var_t>(var), make_any() };
}
reg_sym(primary_expression_null);

env_t primary_expression_expr(deque<env_t> &env)
{
	auto tok = env[0].any->get<tok_t>();
	if (tok.type != '(') {
		err_msg("Expected (");
	}
	tok = env[2].any->get<tok_t>();
	if (tok.type != ')') {
		err_msg("Expected )");
	}
	return env[1];
}
reg_sym(primary_expression_expr);

}