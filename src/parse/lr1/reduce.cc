#include "parse/lr1/reduce.hh"
#include "util.hh"

namespace neko_cc
{
env_t null_reduce_fn(deque<env_t> &unused(env))
{
	return env_t();
}

reg_sym(null_reduce_fn);

}