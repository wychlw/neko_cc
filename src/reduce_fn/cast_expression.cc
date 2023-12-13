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

env_t cast_expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(cast_expression_direct);

env_t cast_expression_cast(deque<env_t> &env)
{
	if (env[0].any->get<tok_t>().type != '(') {
		err_msg("Expected (");
	}
	if (env[2].any->get<tok_t>().type != ')') {
		err_msg("Expected )");
	}

	type_t type = env[1].any->get<type_t>();
	var_t var = *env[3].var;
	if (var.is_alloced) {
		var = emit_load(var);
	}
	if (!is_type_p(*var.type) && var.type->type != type_t::type_basic) {
		err_msg("Expected pointer or basic type");
	}
	if (is_type_p(*var.type) && !is_type_p(type)) {
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		var = emit_ptrtoint(var, i64_type);
		var = emit_conv_to(var, type);
	} else if (is_type_p(*var.type) && is_type_p(type)) {
		// do nothing
	} else if (!is_type_p(*var.type) && is_type_p(type) &&
		   is_type_i(*var.type)) {
		var = emit_inttoptr(var, type);
	} else if (!is_type_p(*var.type) && is_type_p(type) &&
		   is_type_f(*var.type)) {
		type_t i64_type;
		i64_type.name = "i64";
		i64_type.type = type_t::type_basic;
		i64_type.size = 8;
		i64_type.is_long = 2;
		var = emit_conv_to(var, i64_type);
		var = emit_inttoptr(var, type);
	} else {
		var = emit_conv_to(var, type);
	}
	return { env[3].ctx, make_shared<var_t>(var), make_any() };
}
reg_sym(cast_expression_cast);

}