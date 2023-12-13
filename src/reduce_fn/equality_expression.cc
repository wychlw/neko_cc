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

env_t equality_expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(equality_expression_direct);

env_t equality_expression_eq(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != tok_eq) {
		err_msg("Expected ==");
	}
	var_t var1 = *env[0].var;
	var_t var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if ((is_type_i(*var1.type) && is_type_f(*var1.type) && is_type_p(*var1.type)) ||
		(is_type_i(*var2.type) && is_type_f(*var2.type) && is_type_p(*var2.type))) {
		err_msg("Cannot compare non-basic types");
	}
	if (is_type_p(*var1.type) && is_type_p(*var2.type)) {
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		var1 = emit_ptrtoint(var1, i64_type);
		var2 = emit_ptrtoint(var2, i64_type);
	}
	else if (!is_type_p(*var1.type) && !is_type_p(*var2.type)) {
		emit_match_type(var1, var2);
	}
	else{
		err_msg("Cannot compare non-basic types");
	}
	var_t ret_var;
	if (is_type_i(*var1.type)) {
		ret_var = emit_eq(var1, var2);
	}
	else if (is_type_f(*var1.type)) {
		ret_var = emit_feq(var1, var2);
	}
	return env_t{env[0].ctx, make_shared<var_t>(ret_var), make_any()};
}
reg_sym(equality_expression_eq);


env_t equality_expression_ne(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != tok_ne) {
		err_msg("Expected !=");
	}
	var_t var1 = *env[0].var;
	var_t var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if ((is_type_i(*var1.type) && is_type_f(*var1.type) && is_type_p(*var1.type)) ||
		(is_type_i(*var2.type) && is_type_f(*var2.type) && is_type_p(*var2.type))) {
		err_msg("Cannot compare non-basic types");
	}
	if (is_type_p(*var1.type) && is_type_p(*var2.type)) {
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		var1 = emit_ptrtoint(var1, i64_type);
		var2 = emit_ptrtoint(var2, i64_type);
	}
	else if (!is_type_p(*var1.type) && !is_type_p(*var2.type)) {
		emit_match_type(var1, var2);
	}
	else{
		err_msg("Cannot compare non-basic types");
	}
	var_t ret_var;
	if (is_type_i(*var1.type)) {
		ret_var = emit_ne(var1, var2);
	}
	else if (is_type_f(*var1.type)) {
		ret_var = emit_fne(var1, var2);
	}
	return env_t{env[0].ctx, make_shared<var_t>(ret_var), make_any()};
}
reg_sym(equality_expression_ne);

}