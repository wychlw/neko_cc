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

	/**
	 * @brief Get the name of an grammar or token
	 * 
	 * @param idx 
	 * @return string 
	 */
	string get_name(size_t idx);

    private:
	/**
	 * @brief Parse the lex file of a given grammar
	 * 
	 * @param group 
	 * @param node 
	 */
	void fill_gramma(gramma_t &group, YAML::detail::iterator_value &node);

	/**
	 * @brief Get the replace name of a grammar
	 * 
	 * @param o_name 
	 * @return string 
	 */
	string get_nxt_name(const string &o_name);

    public:
	/**
	 * @brief Parse the lex file
	 * 
	 * @param root 
	 */
	void parse_lex(YAML::Node &root);

	/**
	 * @brief Remove the left recursion of the grammar
	 * 
	 */
	void remove_left_recursion();

    private:
	/**
	 * @brief Replace the left recursion of a grammar
	 * a -> a b | b
	 * --------------
	 * a -> b a'
	 * a' -> b a' | ε
	 * 
	 * @param group 
	 * @param rep_g 
	 * @return deque<deque<comp_t> > 
	 */
	deque<deque<comp_t> > replace_left_gramma(const deque<comp_t> &group,
						  const gramma_t &rep_g);
						
	/**
	 * @brief Detect whether the grammar has direct left recursion
	 * 
	 * @param g 
	 * @return true 
	 * @return false 
	 */
	bool chk_has_direct_left_recursion(const gramma_t &g);

	/**
	 * @brief Get the beta part of a grammar
	 * 
	 * @param group 
	 * @return deque<deque<comp_t> > 
	 */
	deque<deque<comp_t> > get_beta(const gramma_t &group);

	/**
	 * @brief Get the alpha part of a grammar
	 * 
	 * @param group 
	 * @return deque<deque<comp_t> > 
	 */
	deque<deque<comp_t> > get_alpha(const gramma_t &group);

	public:
	/**
	 * @brief Remove the left traceback of the grammar
	 * 
	 */
	void remove_left_traceback();

	private:
	/**
	 * @brief Check whether the grammar has direct left traceback
	 * 
	 * @param g 
	 * @return true 
	 * @return false 
	 */
	bool chk_has_direct_left_traceback(const gramma_t &g);

	/**
	 * @brief Remove the left traceback of a grammar
	 * 
	 * @param g 
	 * @return deque<gramma_t> 
	 */
	deque<gramma_t> remove_group_left_traceback(gramma_t &g);

	public:
	/**
	 * @brief Get the first set of the grammar
	 * 
	 */
	void get_first();
	private:
	/**
	 * @brief Get the first set of a grammar group
	 * 
	 * @param g 
	 * @return std::unordered_map<int, size_t> 
	 */
	std::unordered_map<int, size_t> get_group_first(const gramma_t &g);

	public:
	/**
	 * @brief Get the follow set of the grammar
	 * 
	 */
	void get_follow();
};


void init_parse_env(const lex_t &lex);

void run_parse(stream &ss);

}