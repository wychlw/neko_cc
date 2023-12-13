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

env_t relational_expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(relational_expression_direct);

env_t relational_expression_lt(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != '<') {
		err_msg("Expected <");
	}
	var_t var1 = *env[0].var;
	var_t var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if ((!is_type_i(*var1.type) && !is_type_f(*var1.type)) ||
	    (!is_type_i(*var2.type) && !is_type_f(*var2.type))) {
		err_msg("Relational operands must be integers or floats");
	}
	emit_match_type(var1, var2);
	var_t ret_var;
	if (is_type_i(*var1.type) && var1.type->is_unsigned) {
		ret_var = emit_ult(var1, var2);
	} else if (is_type_i(*var1.type)) {
		ret_var = emit_slt(var1, var2);
	} else if (is_type_f(*var1.type)) {
		ret_var = emit_flt(var1, var2);
	}
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(relational_expression_lt);

env_t relational_expression_gt(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != '>') {
		err_msg("Expected >");
	}
	var_t var1 = *env[0].var;
	var_t var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if ((!is_type_i(*var1.type) && !is_type_f(*var1.type)) ||
	    (!is_type_i(*var2.type) && !is_type_f(*var2.type))) {
		err_msg("Relational operands must be integers or floats");
	}
	emit_match_type(var1, var2);
	var_t ret_var;
	if (is_type_i(*var1.type) && var1.type->is_unsigned) {
		ret_var = emit_ugt(var1, var2);
	} else if (is_type_i(*var1.type)) {
		ret_var = emit_sgt(var1, var2);
	} else if (is_type_f(*var1.type)) {
		ret_var = emit_fgt(var1, var2);
	}
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(relational_expression_gt);

env_t relational_expression_le(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != tok_le) {
		err_msg("Expected <=");
	}
	var_t var1 = *env[0].var;
	var_t var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if ((!is_type_i(*var1.type) && !is_type_f(*var1.type)) ||
	    (!is_type_i(*var2.type) && !is_type_f(*var2.type))) {
		err_msg("Relational operands must be integers or floats");
	}
	emit_match_type(var1, var2);
	var_t ret_var;
	if (is_type_i(*var1.type) && var1.type->is_unsigned) {
		ret_var = emit_ule(var1, var2);
	} else if (is_type_i(*var1.type)) {
		ret_var = emit_sle(var1, var2);
	} else if (is_type_f(*var1.type)) {
		ret_var = emit_fle(var1, var2);
	}
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(relational_expression_le);

env_t relational_expression_ge(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != tok_ge) {
		err_msg("Expected >=");
	}
	var_t var1 = *env[0].var;
	var_t var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if ((!is_type_i(*var1.type) && !is_type_f(*var1.type)) ||
	    (!is_type_i(*var2.type) && !is_type_f(*var2.type))) {
		err_msg("Relational operands must be integers or floats");
	}
	emit_match_type(var1, var2);
	var_t ret_var;
	if (is_type_i(*var1.type) && var1.type->is_unsigned) {
		ret_var = emit_uge(var1, var2);
	} else if (is_type_i(*var1.type)) {
		ret_var = emit_sge(var1, var2);
	} else if (is_type_f(*var1.type)) {
		ret_var = emit_fge(var1, var2);
	}
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}
reg_sym(relational_expression_ge);

}