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

env_t multiplicative_expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(multiplicative_expression_direct);

env_t multiplicative_expression_mul(deque<env_t> &env)
{
	auto tok = env[1].any->get<tok_t>();
	if (tok.type != '*') {
		err_msg("Expected *");
	}
	auto var1 = *env[0].var;
	auto var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if (!is_type_i(*var1.type) && !is_type_f(*var1.type)) {
		err_msg("Cannot multiply non-basic type");
	}
	if (!is_type_i(*var2.type) && !is_type_f(*var2.type)) {
		err_msg("Cannot multiply non-basic type");
	}
	emit_match_type(var1, var2);
	var_t ret_var;
	if (is_type_i(*var1.type)) {
		ret_var = emit_mul(var1, var2);
	} else if (is_type_f(*var1.type)) {
		ret_var = emit_fmul(var1, var2);
	} else {
		err_msg("Cannot multiply this type");
	}
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(multiplicative_expression_mul);

env_t multiplicative_expression_div(deque<env_t> &env)
{
	auto tok = env[1].any->get<tok_t>();
	if (tok.type != '/') {
		err_msg("Expected /");
	}
	auto var1 = *env[0].var;
	auto var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if (!is_type_i(*var1.type) && !is_type_f(*var1.type)) {
		err_msg("Cannot divide non-basic type");
	}
	if (!is_type_i(*var2.type) && !is_type_f(*var2.type)) {
		err_msg("Cannot divide non-basic type");
	}
	emit_match_type(var1, var2);
	var_t ret_var;
	if (is_type_i(*var1.type) && var1.type->is_unsigned) {
		ret_var = emit_udiv(var1, var2);
	} else if (is_type_i(*var1.type) && !var1.type->is_unsigned) {
		ret_var = emit_sdiv(var1, var2);
	} else if (is_type_f(*var1.type)) {
		ret_var = emit_fdiv(var1, var2);
	} else {
		err_msg("Cannot divide this type");
	}
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(multiplicative_expression_div);

env_t multiplicative_expression_mod(deque<env_t> &env)
{
	auto tok = env[1].any->get<tok_t>();
	if (tok.type != '%') {
		err_msg("Expected %");
	}
	auto var1 = *env[0].var;
	auto var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if (!is_type_i(*var1.type) && !is_type_f(*var1.type)) {
		err_msg("Cannot mod non-basic type");
	}
	if (!is_type_i(*var2.type) && !is_type_f(*var2.type)) {
		err_msg("Cannot mod non-basic type");
	}
	emit_match_type(var1, var2);
	var_t ret_var;
	if (is_type_i(*var1.type) && var1.type->is_unsigned) {
		ret_var = emit_urem(var1, var2);
	} else if (is_type_i(*var1.type) && !var1.type->is_unsigned) {
		ret_var = emit_srem(var1, var2);
	} else if (is_type_f(*var1.type)) {
		ret_var = emit_frem(var1, var2);
	} else {
		err_msg("Cannot mod this type");
	}
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(multiplicative_expression_mod);

env_t additive_expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(additive_expression_direct);

env_t additive_expression_add(deque<env_t> &env)
{
	auto tok = env[1].any->get<tok_t>();
	if (tok.type != '+') {
		err_msg("Expected +");
	}
	auto var1 = *env[0].var;
	auto var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if (!is_type_i(*var1.type) && !is_type_f(*var1.type)) {
		err_msg("Cannot add non-basic type");
	}
	if (!is_type_i(*var2.type) && !is_type_f(*var2.type)) {
		err_msg("Cannot add non-basic type");
	}
	emit_match_type(var1, var2);
	var_t ret_var;
	if (is_type_i(*var1.type)) {
		ret_var = emit_add(var1, var2);
	} else if (is_type_f(*var1.type)) {
		ret_var = emit_fadd(var1, var2);
	} else {
		err_msg("Cannot add this type");
	}
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(additive_expression_add);

env_t additive_expression_sub(deque<env_t> &env)
{
	auto tok = env[1].any->get<tok_t>();
	if (tok.type != '-') {
		err_msg("Expected -");
	}
	auto var1 = *env[0].var;
	auto var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if (!is_type_i(*var1.type) && !is_type_f(*var1.type)) {
		err_msg("Cannot sub non-basic type");
	}
	if (!is_type_i(*var2.type) && !is_type_f(*var2.type)) {
		err_msg("Cannot sub non-basic type");
	}
	emit_match_type(var1, var2);
	var_t ret_var;
	if (is_type_i(*var1.type)) {
		ret_var = emit_sub(var1, var2);
	} else if (is_type_f(*var1.type)) {
		ret_var = emit_fsub(var1, var2);
	} else {
		err_msg("Cannot sub this type");
	}
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(additive_expression_sub);

}