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

env_t exclusive_or_expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(exclusive_or_expression_direct);

env_t exclusive_or_expression_exclusive_or(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != '^') {
		err_msg("Expected ^");
	}
	var_t var1 = *env[0].var;
	var_t var2 = *env[2].var;
	if (var1.is_alloced) {
		var1 = emit_load(var1);
	}
	if (var2.is_alloced) {
		var2 = emit_load(var2);
	}
	if (!is_type_i(*var1.type) || !is_type_i(*var2.type)) {
		err_msg("Bitwise operands must be integers");
	}
	emit_match_type(var1, var2);
	var_t ret_var = emit_xor(var1, var2);
	return env_t{env[0].ctx, make_shared<var_t>(ret_var), make_any()};
}
reg_sym(exclusive_or_expression_exclusive_or);

}