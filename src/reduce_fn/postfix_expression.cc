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

env_t postfix_expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(postfix_expression_direct);

env_t postfix_expression_arr(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != '[') {
		err_msg("Expected [");
	}
	if (env[3].any->get<tok_t>().type != ']') {
		err_msg("Expected ]");
	}
	var_t arr = *env[0].var;
	var_t idx = *env[2].var;
	if (arr.is_alloced) {
		arr = emit_load(arr);
	}
	if (idx.is_alloced) {
		idx = emit_load(idx);
	}
	if (!is_type_i(*idx.type)) {
		err_msg("Array index must be an integer");
	}
	if (!is_type_p(*arr.type)) {
		err_msg("Cannot index non-pointer type");
	}
	var_t ret_var = get_item_from_arrptr(arr, idx);
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(postfix_expression_arr);

env_t postfix_expression_call(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != '(') {
		err_msg("Expected (");
	}
	if (env[2].any->get<tok_t>().type != ')') {
		err_msg("Expected )");
	}
	var_t func = *env[0].var;
	if (!(func.type->type == type_t::type_func ||
	      (func.type->type == type_t::type_pointer &&
	       func.type->ptr_to->type == type_t::type_func))) {
		err_msg("Cannot call non-function type");
	}
	std::vector<var_t> args;
	var_t ret_var = emit_call(func, args);
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(postfix_expression_call);

env_t postfix_expression_call_args(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != '(') {
		err_msg("Expected (");
	}
	if (env[3].any->get<tok_t>().type != ')') {
		err_msg("Expected )");
	}
	var_t func = *env[0].var;
	if (!(func.type->type == type_t::type_func ||
	      (func.type->type == type_t::type_pointer &&
	       func.type->ptr_to->type == type_t::type_func))) {
		err_msg("Cannot call non-function type");
	}
	std::vector<var_t> args = env[2].any->get<std::vector<var_t> >();
	var_t ret_var = emit_call(func, args);
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(postfix_expression_call_args);

env_t postfix_expression_inc(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != tok_inc) {
		err_msg("Expected ++");
	}
	var_t store_var = *env[0].var;
	var_t ori_var = store_var;
	if (!ori_var.is_alloced) {
		err_msg("Cannot increment rvalue");
	}
	ori_var = emit_load(ori_var);
	var_t ret_var = ori_var;
	if (is_type_i(*ori_var.type)) {
		var_t inc_var;
		inc_var.type = ori_var.type;
		inc_var.name = 1;
		inc_var.is_alloced = false;
		ori_var = emit_add(ori_var, inc_var);
	} else if (is_type_p(*ori_var.type)) {
		type_t ori_type = *ori_var.type;
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		ori_var = emit_ptrtoint(ori_var, i64_type);
		var_t inc_var;
		inc_var.type = make_shared<type_t>(i64_type);
		inc_var.name = 8;
		inc_var.is_alloced = false;
		ori_var = emit_add(ori_var, inc_var);
		ori_var = emit_inttoptr(ori_var, ori_type);
	} else if (is_type_f(*ori_var.type)) {
		var_t inc_var;
		inc_var.type = ori_var.type;
		inc_var.name = 1.0;
		inc_var.is_alloced = false;
		ori_var = emit_fadd(ori_var, inc_var);
	} else {
		err_msg("Cannot increment non-integer type");
	}
	emit_store(store_var, ori_var);
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(postfix_expression_inc);

env_t postfix_expression_dec(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != tok_dec) {
		err_msg("Expected --");
	}
	var_t store_var = *env[0].var;
	var_t ori_var = store_var;
	if (!ori_var.is_alloced) {
		err_msg("Cannot decrement rvalue");
	}
	ori_var = emit_load(ori_var);
	var_t ret_var = ori_var;
	if (is_type_i(*ori_var.type)) {
		var_t inc_var;
		inc_var.type = ori_var.type;
		inc_var.name = 1;
		inc_var.is_alloced = false;
		ori_var = emit_sub(ori_var, inc_var);
	} else if (is_type_p(*ori_var.type)) {
		type_t ori_type = *ori_var.type;
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		ori_var = emit_ptrtoint(ori_var, i64_type);
		var_t inc_var;
		inc_var.type = make_shared<type_t>(i64_type);
		inc_var.name = 8;
		inc_var.is_alloced = false;
		ori_var = emit_sub(ori_var, inc_var);
		ori_var = emit_inttoptr(ori_var, ori_type);
	} else if (is_type_f(*ori_var.type)) {
		var_t inc_var;
		inc_var.type = ori_var.type;
		inc_var.name = 1.0;
		inc_var.is_alloced = false;
		ori_var = emit_fsub(ori_var, inc_var);
	} else {
		err_msg("Cannot decrement non-integer type");
	}
	emit_store(store_var, ori_var);
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(postfix_expression_dec);


}