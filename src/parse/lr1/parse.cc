#include "parse/lr1/parse.hh"
#include "out.hh"
#include "parse/lr1/gramma.hh"
#include "parse/lr1/reduce.hh"
#include "parse/parse_base.hh"
#include "tok.hh"
#include "util.hh"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

namespace neko_cc
{

static lex_t lex_now;
static bool parse_fin;
static parser_hook *hook_fn;

void init_parse_env(const lex_t &lex)
{
	lex_now = lex;
	hook_fn = nullptr;
}

void init_parse_env(const lex_t &lex, parser_hook *hook)
{
	lex_now = lex;
	hook_fn = hook;
}

void translation_unit(stream &ss)
{
	parse_fin = false;
	while (!parse_fin) {
		run_parse(ss);
	}
}

void run_parse(stream &ss)
{
	deque<size_t> state;
	deque<env_t> env;
	state.push_back(lex_now.I_start);
	shared_ptr<context_t> empty_ctx = nullptr;
	{
		type_t i32;
		i32.type = type_t::type_basic;
		i32.name = "i32";
		i32.size = 4;
		i32.is_int = 1;
		type_t i32_ptr;
		i32_ptr.type = type_t::type_pointer;
		i32_ptr.name = "i32_ptr";
		i32_ptr.size = 8;
		i32_ptr.ptr_to = make_shared<type_t>(i32);
		var_t init_var;
		init_var.type = make_shared<type_t>(i32_ptr);
		init_var.name = "@a";
		init_var.is_alloced = true;
		empty_ctx = make_shared<context_t>();
		empty_ctx->vars["@a"] = init_var;
	}
	shared_ptr<var_t> empty_var = nullptr;
	env.push_back({ empty_ctx, empty_var, make_any() });
	size_t look;
	tok_t tok;
	while (!parse_fin) {
		tok = nxt_tok(ss);
		look = lex_now.tok_idx_map[tok.type];
		if (hook_fn != nullptr) {
			lex_t::action_t action_try;
			if (lex_now.action_table[state.back()].count(look) ==
			    0) {
				action_try = { lex_t::action_t::type_t::error,
					       { 0 } };
			} else {
				action_try =
					lex_now.action_table[state.back()][look];
			}
			hook_fn(state, env, tok, lex_now, action_try);
		}
		if (lex_now.action_table[state.back()].count(look) == 0) {
			error("Unexpected token " + tok.str + ", " +
				      back_tok_map(tok.type),
			      ss, true);
		}
		auto action = lex_now.action_table[state.back()][look];
		if (action.type == lex_t::action_t::type_t::shift) {
			tok = get_tok(ss);
			auto tok_arg = make_any(tok);
			env_t tmp{ env.back().ctx, empty_var, tok_arg };
			state.push_back(action.action.idx);
			env.push_back(tmp);
		} else if (action.type == lex_t::action_t::type_t::reduce) {
			auto reduce = action.action.reduce.reduce;
			auto num = action.action.reduce.reduce_num;
			auto to = action.action.reduce.reduce_to;
			deque<env_t> env_arg;
			for (size_t i = 0; i < num; i++) {
				env_arg.push_front(env[env.size() - 1 - i]);
			}
			env_t tmp_env = reduce(env_arg);
			for (size_t i = 0; i < num; i++) {
				state.pop_back();
				env.pop_back();
			}
			if (parse_fin) {
				break;
			}
			if (lex_now.action_table[state.back()].count(to) == 0) {
				error("Unexpected token " + tok.str + ", " +
					      back_tok_map(tok.type),
				      ss, true);
			}
			auto action = lex_now.action_table[state.back()][to];
			if (action.type != lex_t::action_t::type_t::go) {
				error("Unexpected token " + tok.str + ", " +
					      back_tok_map(tok.type),
				      ss, true);
			}
			state.push_back(action.action.go);
			env.push_back(tmp_env);
		} else {
			error("Unexpected token " + tok.str + ", " +
				      back_tok_map(tok.type),
			      ss, true);
		}
	}
}

env_t accept_reduce_fn(deque<env_t> &unused(env))
{
	parse_fin = true;
	shared_ptr<context_t> empty_ctx = nullptr;
	shared_ptr<var_t> empty_var = nullptr;
	return env_t{ empty_ctx, empty_var, make_any() };
}

reg_sym(accept_reduce_fn);

}