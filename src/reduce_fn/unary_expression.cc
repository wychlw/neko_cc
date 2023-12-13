#include "gen.hh"
#include "parse/lr1/reduce.hh"
#include "parse/parse_base.hh"
#include "parse/parse_top_down.hh"
#include "scan.hh"
#include "tok.hh"
#include "out.hh"
#include "util.hh"
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <type_traits>

namespace neko_cc_reductions
{
using namespace neko_cc;
using namespace std;

env_t unary_expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(unary_expression_direct);

env_t unary_expression_inc(deque<env_t> &env)
{
	if (env[0].any->get<tok_t>().type != tok_inc) {
		err_msg("Expected ++");
	}

	var_t var = *env[1].var;
	if (!var.is_alloced) {
		err_msg("Expected lvalue");
	}

	var_t ret_var = var;
	var = emit_load(var);
	if (is_type_p(*var.type)) {
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		var = emit_ptrtoint(var, i64_type);
		var_t inc_var;
		inc_var.type = var.type;
		inc_var.is_alloced = false;
		inc_var.name = '8';
		var = emit_add(var, inc_var);
		var = emit_inttoptr(var, *ret_var.type);
	} else if (is_type_i(*var.type)) {
		var_t inc_var;
		inc_var.type = var.type;
		inc_var.is_alloced = false;
		inc_var.name = '1';
		var = emit_add(var, inc_var);
	} else if (is_type_f(*var.type)) {
		var_t inc_var;
		inc_var.type = var.type;
		inc_var.is_alloced = false;
		inc_var.name = '1';
		var = emit_fadd(var, inc_var);
	} else {
		err_msg("Invalid operator for type");
	}

	emit_store(var, ret_var);
	return { env[1].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(unary_expression_inc);

env_t unary_expression_dec(deque<env_t> &env)
{
	if (env[0].any->get<tok_t>().type != tok_dec) {
		err_msg("Expected --");
	}

	var_t var = *env[1].var;
	if (!var.is_alloced) {
		err_msg("Expected lvalue");
	}

	var_t ret_var = var;
	var = emit_load(var);
	if (is_type_p(*var.type)) {
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		var = emit_ptrtoint(var, i64_type);
		var_t inc_var;
		inc_var.type = var.type;
		inc_var.is_alloced = false;
		inc_var.name = '8';
		var = emit_sub(var, inc_var);
		var = emit_inttoptr(var, *ret_var.type);
	} else if (is_type_i(*var.type)) {
		var_t inc_var;
		inc_var.type = var.type;
		inc_var.is_alloced = false;
		inc_var.name = '1';
		var = emit_sub(var, inc_var);
	} else if (is_type_f(*var.type)) {
		var_t inc_var;
		inc_var.type = var.type;
		inc_var.is_alloced = false;
		inc_var.name = '1';
		var = emit_fsub(var, inc_var);
	} else {
		err_msg("Invalid operator for type");
	}

	emit_store(var, ret_var);
	return { env[1].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(unary_expression_dec);

env_t unary_expression_unary(deque<env_t> &env)
{
	if (is_unary_operator(env[0].any->get<tok_t>())) {
		err_msg("Expected unary operator");
	}

	var_t var = *env[1].var;
	tok_t op = env[0].any->get<tok_t>();

	if (op.type == '&') {
		if (var.type->type == type_t::type_pointer &&
		    var.type->ptr_to->type == type_t::type_func) {
			return { env[1].ctx, make_shared<var_t>(var),
				 make_any() };
		}
		if (!var.is_alloced) {
			err_msg("Expected lvalue");
		}
		var.is_alloced = false;
	} else if (op.type == '*') {
		if (var.is_alloced) {
			var = emit_load(var);
		}
		if (!is_type_p(*var.type)) {
			err_msg("Expected pointer type");
		}
		var = emit_load(var);
		var.is_alloced = true;
	} else if (op.type == '+') {
		if (var.is_alloced) {
			var = emit_load(var);
		}
		if (!is_type_i(*var.type) && !is_type_f(*var.type)) {
			err_msg("Expected integer or float type");
		}
	} else if (op.type == '-')
	{
		if (var.is_alloced) {
			var = emit_load(var);
		}
		if (!is_type_i(*var.type) && !is_type_f(*var.type)) {
			err_msg("Expected integer or float type");
		}
		var_t zero;
		zero.type = var.type;
		zero.is_alloced = false;
		zero.name = "zeroinitializer";
		if (is_type_i(*var.type)) {
			var = emit_sub(zero, var);
		} else {
			var = emit_fsub(zero, var);
		}
	} else if (op.type == '~') {
		if (var.is_alloced) {
			var = emit_load(var);
		}
		if (!is_type_i(*var.type)) {
			err_msg("Expected integer type");
		}
		var_t neg_one;
		neg_one.type = var.type;
		neg_one.is_alloced = false;
		neg_one.name = "-1";
		var = emit_xor(var, neg_one);
	} else if (op.type == '!') {
		if (var.is_alloced) {
			var = emit_load(var);
		}
		if (!is_type_i(*var.type) && !is_type_f(*var.type) && !is_type_p(*var.type)) {
			err_msg("Expected integer, float, or pointer type");
		}
		if (is_type_p(*var.type)) {
			type_t i64_type;
			i64_type.name = "i64";
			i64_type.type = type_t::type_basic;
			i64_type.size = 8;
			i64_type.is_long = 2;
			var = emit_ptrtoint(var, i64_type);
		}
		var_t zero;
		zero.type = var.type;
		zero.is_alloced = false;
		zero.name = "zeroinitializer";
		if (is_type_i(*var.type)) {
			var = emit_eq(var, zero);
		} else if (is_type_f(*var.type)) {
			var = emit_feq(var, zero);
		}
	}

	return { env[1].ctx, make_shared<var_t>(var), make_any() };
}
reg_sym(unary_expression_unary);

env_t unary_expression_sizeof_expr(deque<env_t> &env)
{
	if (env[0].any->get<tok_t>().type != tok_sizeof) {
		err_msg("Expected sizeof");
	}

	var_t var = *env[1].var;
	if (var.is_alloced) {
		var = emit_load(var);
	}
	size_t size = var.type->size;
	type_t i64_type;
	i64_type.name = "i64";
	i64_type.type = type_t::type_basic;
	i64_type.size = 8;
	i64_type.is_long = 2;
	var_t size_var;
	size_var.type = make_shared<type_t>(i64_type);
	size_var.is_alloced = false;
	size_var.name = to_string(size);
	return { env[1].ctx, make_shared<var_t>(size_var), make_any() };
}
reg_sym(unary_expression_sizeof_expr);

env_t unary_expression_sizeof_type(deque<env_t> &env)
{
	if (env[0].any->get<tok_t>().type != tok_sizeof) {
		err_msg("Expected sizeof");
	}
	if (env[1].any->get<tok_t>().type != '(') {
		err_msg("Expected (");
	}
	if (env[3].any->get<tok_t>().type != ')') {
		err_msg("Expected )");
	}

	type_t type = env[2].any->get<type_t>();
	size_t size = type.size;
	type_t i64_type;
	i64_type.name = "i64";
	i64_type.type = type_t::type_basic;
	i64_type.size = 8;
	i64_type.is_long = 2;
	var_t size_var;
	size_var.type = make_shared<type_t>(i64_type);
	size_var.is_alloced = false;
	size_var.name = to_string(size);
	return { env[1].ctx, make_shared<var_t>(size_var), make_any() };
}
reg_sym(unary_expression_sizeof_type);

env_t unary_operator(deque<env_t> &env)
{
	if (!is_unary_operator(env[0].any->get<tok_t>())) {
		err_msg("Expected unary operator");
	}

	return env[0];
}

}