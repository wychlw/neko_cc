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

env_t logical_or_expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(logical_or_expression_direct);

env_t logical_or_expression_and(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != tok_lor) {
		err_msg("Expected ||");
	}
	var_t var1 = *env[0].var;
	var_t var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}

	if (!var1.type->is_bool) {
		if (!is_type_i(*var1.type) && !is_type_f(*var1.type) &&
		    !is_type_p(*var1.type)) {
			err_msg("Logical operands must be integers, floats, or pointers");
		}
		var_t zero_var;
		zero_var.type = var1.type;
		zero_var.name = "zeroinitializer";
		var1 = emit_ne(var1, zero_var);
	}
	if (!var2.type->is_bool) {
		if (!is_type_i(*var2.type) && !is_type_f(*var2.type) &&
		    !is_type_p(*var2.type)) {
			err_msg("Logical operands must be integers, floats, or pointers");
		}
		var_t zero_var;
		zero_var.type = var2.type;
		zero_var.name = "zeroinitializer";
		var2 = emit_ne(var2, zero_var);
	}
	var_t ret_var = emit_or(var1, var2);
	return env_t{ env[0].ctx, make_shared<var_t>(ret_var), make_any() };
}

}