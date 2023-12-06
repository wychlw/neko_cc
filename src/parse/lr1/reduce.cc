#include "parse/lr1/reduce.hh"
#include "util.hh"

namespace neko_cc
{
env_t null_reduce_fn(deque<env_t> &unused(env))
{
	shared_ptr<context_t> empty_ctx = nullptr;
	shared_ptr<var_t> empty_var = nullptr;
	return env_t{ empty_ctx, empty_var, make_any() };
}

reg_sym(null_reduce_fn);

}