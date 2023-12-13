#include "gen.hh"
#include "parse/lr1/reduce.hh"
#include "parse/parse_base.hh"
#include "parse/parse_top_down.hh"
#include "scan.hh"
#include "tok.hh"
#include "out.hh"
#include "util.hh"
#include <deque>
#include <memory>
#include <string>

namespace neko_cc_reductions
{
using namespace neko_cc;
using namespace std;

env_t assignment_expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(assignment_expression_direct);

env_t assignment_expression_assign(deque<env_t> &env)
{
	tok_t op = env[1].any->get<tok_t>();
	var_t var1 = *env[0].var;
	var_t var2 = *env[2].var;

	if (!var1.is_alloced) {
		err_msg("Expected lvalue");
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}

	if (op.type == '=') {
		var2 = emit_conv_to(var2, *var1.type->ptr_to);
		emit_store(var1, var2);
		return { env[0].ctx, make_shared<var_t>(var1), make_any() };
	}

	var_t ret_var = var1;

	var1 = emit_load(var1);
	if (is_type_p(*var1.type)) {
		if (op.type != tok_add_assign && op.type != tok_sub_assign) {
			err_msg("Invalid operator for pointer");
		}
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		var1 = emit_ptrtoint(var1, i64_type);
	}
	if (!is_type_i(*var1.type) && !is_type_f(*var2.type)) {
		err_msg("Invalid operator for type");
	}
	emit_match_type(var1, var2);
	if (op.type == tok_add_assign) {
		if (is_type_i(*var1.type)) {
			var1 = emit_add(var1, var2);
		} else {
			var1 = emit_fadd(var1, var2);
		}
	} else if (op.type == tok_sub_assign) {
		if (is_type_i(*var1.type)) {
			var1 = emit_sub(var1, var2);
		} else {
			var1 = emit_fsub(var1, var2);
		}
	} else if (op.type == tok_mul_assign) {
		if (is_type_i(*var1.type)) {
			var1 = emit_mul(var1, var2);
		} else {
			var1 = emit_fmul(var1, var2);
		}
	} else if (op.type == tok_div_assign) {
		if (is_type_i(*var1.type) && var1.type->is_unsigned) {
			var1 = emit_udiv(var1, var2);
		} else if (is_type_i(*var1.type)) {
			var1 = emit_sdiv(var1, var2);
		} else {
			var1 = emit_fdiv(var1, var2);
		}
	} else if (op.type == tok_mod_assign) {
		if (is_type_i(*var1.type) && var1.type->is_unsigned) {
			var1 = emit_urem(var1, var2);
		} else if (is_type_i(*var1.type)) {
			var1 = emit_srem(var1, var2);
		} else {
			err_msg("Invalid operator for type");
		}
	} else if (op.type == tok_lshift_assign) {
		if (is_type_i(*var1.type)) {
			var1 = emit_shl(var1, var2);
		} else {
			err_msg("Invalid operator for type");
		}
	} else if (op.type == tok_rshift_assign) {
		if (is_type_i(*var1.type) && var1.type->is_unsigned) {
			var1 = emit_lshr(var1, var2);
		} else if (is_type_i(*var1.type)) {
			var1 = emit_ashr(var1, var2);
		} else {
			err_msg("Invalid operator for type");
		}
	} else if (op.type == tok_and_assign) {
		if (is_type_i(*var1.type)) {
			var1 = emit_and(var1, var2);
		} else {
			err_msg("Invalid operator for type");
		}
	} else if (op.type == tok_xor_assign) {
		if (is_type_i(*var1.type)) {
			var1 = emit_xor(var1, var2);
		} else {
			err_msg("Invalid operator for type");
		}
	} else if (op.type == tok_or_assign) {
		if (is_type_i(*var1.type)) {
			var1 = emit_or(var1, var2);
		} else {
			err_msg("Invalid operator for type");
		}
	} else {
		err_msg("Invalid operator for type");
	}

	var1 = emit_conv_to(var1, *ret_var.type->ptr_to);
	emit_store(ret_var, var1);
	return { env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(assignment_expression_assign);

env_t assignment_operator(deque<env_t> &env)
{
	return env[0];
}
reg_sym(assignment_operator);

}