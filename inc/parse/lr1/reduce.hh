#pragma once

#include "util.hh"
#include "parse/parse_base.hh"
#include <deque>
#include <memory>

namespace neko_cc
{

using std::deque;
using std::shared_ptr;
using std::make_shared;

struct env_t {
	shared_ptr<context_t> ctx;
	shared_ptr<var_t> var;
	shared_ptr<any_unique> any;
};

typedef env_t reduce_fn(deque<env_t> &env);

env_t null_reduce_fn(deque<env_t> &env);

}