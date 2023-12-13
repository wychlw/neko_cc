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

env_t expression_direct(deque<env_t> &env)
{
	return env[0];
}
reg_sym(expression_direct);

env_t expression_list(deque<env_t> &env)
{
	if (env[1].any->get<tok_t>().type != ',') {
		err_msg("Expected ',' in expression list");
	}
	return env[2];
}
reg_sym(expression_list);

}