#pragma once

#include "scan.hh"
#include <cstddef>
#include <deque>
#include <string>
#include "parse/parse_base.hh"
#include <unordered_map>
#include <unordered_set>
#include <yaml-cpp/node/node.h>

namespace neko_cc
{

using std::deque;
using std::string;

struct env_t {
	context_t &ctx;
	tok_t &tok;
	var_t &rs;
	void *addon;
};

using action_fn = env_t (*)(deque<env_t &> env);

struct comp_t {
	size_t idx;
	enum { UNKNOWN, GRAMMA, TOK } type;
};

struct gramma_t {
	string name;
	size_t idx;
	deque<deque<comp_t> > g;
	action_fn prev_action;
	action_fn post_action;
	bool accept_empty;
};

extern std::unordered_map<string, int> comp_predefine_tok;

struct lex_t {
	std::unordered_map<size_t, int> comp_tok;
	std::unordered_map<size_t, gramma_t> comp_gramma;
	std::unordered_map<size_t, std::unordered_map<int, size_t>> first_set;
	std::unordered_map<size_t, std::unordered_set<int>> follow_set;
	size_t start_idx;

	lex_t() = default;
	lex_t(stream &ss);

	string get_name(size_t idx);

    private:
	void fill_gramma(gramma_t &group, YAML::detail::iterator_value &node);
	string get_nxt_name(const string &o_name);

    public:
	void parse_lex(YAML::Node &root);
	void remove_left_recursion();

    private:
	deque<deque<comp_t> > replace_left_gramma(const deque<comp_t> &group,
						  const gramma_t &rep_g);
	bool chk_has_direct_left_recursion(const gramma_t &g);
	deque<deque<comp_t> > get_beta(const gramma_t &group);
	deque<deque<comp_t> > get_alpha(const gramma_t &group);

	public:
	void remove_left_traceback();

	private:
	bool chk_has_direct_left_traceback(const gramma_t &g);
	deque<gramma_t> remove_group_left_traceback(gramma_t &g);

	public:
	void get_first();
	private:
	std::unordered_map<int, size_t> get_group_first(const gramma_t &g);

	public:
	void get_follow();
};


void init_parse_env(const lex_t &lex);

void run_parse(stream &ss);

}