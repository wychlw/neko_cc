#pragma once

#include "parse/lr1/gramma.hh"
#include "parse/lr1/base.hh"
#include "parse/lr1/reduce.hh"
#include "parse/parse_base.hh"
#include "scan.hh"
#include <cstddef>
#include <deque>

namespace neko_cc
{

using std::deque;

typedef void parser_hook(deque<size_t> &state, deque<env_t> &env, tok_t &tok,
			 lex_t &lex, lex_t::action_t &action);

void init_parse_env(const lex_t &lex);

void init_parse_env(const lex_t &lex, parser_hook *hook);

void run_parse(stream &ss);

env_t accept_reduce_fn(deque<env_t> &env);

}