#include "gen.hh"
#include "parse/lr1/reduce.hh"
#include "parse/parse_base.hh"
#include "parse/parse_top_down.hh"
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

env_t conditional_expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(conditional_expression_direct);

/*
 * !Important note!
 * This version of the conditional expression
 * doesn't have short circuiting.
*/
env_t conditional_expression_cond(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != '?') {
		err_msg("Expected ?");
	}
	if (env[3].any->get<tok_t>().type != ':') {
		err_msg("Expected :");
	}
	var_t var_cond = *env[0].var;
	var_t var_true = *env[2].var;
	var_t var_false = *env[4].var;
	type_t conv_type;
	auto label_true = get_label();
	auto label_false = get_label();
	auto label_end = get_label();

	var_t true_type, false_type;
	if (var_true.is_alloced) {
		true_type.type = var_true.type->ptr_to;
	} else {
		true_type.type = var_true.type;
	}
	if (var_false.is_alloced) {
		false_type.type = var_false.type->ptr_to;
	} else {
		false_type.type = var_false.type;
	}
	bool conv_true = should_conv_to_first(false_type, true_type);
	bool conv_false = should_conv_to_first(true_type, false_type);

	if (var_cond.is_alloced) {
		var_cond = emit_load(var_cond);
	}
	emit_br(var_cond, label_true, label_false);

	emit_label(label_true);
	if (var_true.is_alloced) {
		var_true = emit_load(var_true);
	}
	if (conv_true && !conv_false) {
		var_true = emit_conv_to(var_true, *true_type.type);
	}
	emit_br(label_end);

	emit_label(label_false);
	if (var_false.is_alloced) {
		var_false = emit_load(var_false);
	}
	if (conv_false) {
		var_false = emit_conv_to(var_false, *false_type.type);
	}
	emit_br(label_end);

	emit_label(label_end);
	var_t var_ret = emit_phi(var_true, label_true, var_false, label_false);
	return env_t{ env[0].ctx, make_shared<var_t>(var_ret), make_any() };
}
reg_sym(conditional_expression_cond);
}