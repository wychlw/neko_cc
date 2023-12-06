#pragma once

#include "parse/parse_base.hh"
#include "parse/lr1/base.hh"
#include "scan.hh"
#include "util.hh"
#include "reduce.hh"
#include <cstddef>
#include <memory>
#include <string>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/yaml.h>

namespace neko_cc
{

using std::deque;
using std::string;

struct lex_t {
	struct gramma_t {
		string name;
		size_t idx;

		struct sentence_t {
			reduce_fn *reduce;
			deque<size_t> g;
		};

		deque<sentence_t> g;
	};

	std::unordered_map<size_t, int> comp_tok;
	std::unordered_map<int, size_t> tok_idx_map;
	std::unordered_map<size_t, gramma_t> comp_gramma;
	std::unordered_map<size_t, std::unordered_set<size_t> > first_set;
	std::unordered_map<size_t, std::unordered_set<size_t> > follow_set;
	std::unordered_set<size_t> terminal;
	std::unordered_set<size_t> non_terminal;

	size_t start_idx;

	lex_t() = default;
	lex_t(stream &ss);
	lex_t(stream &ss, string start);

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
	void fill_gramma(gramma_t &group, YAML::Node &node);

    public:
	/**
	 * @brief Parse the lex file
	 * 
	 * @param root 
	 */
	void parse_lex(YAML::Node &root);

	void get_terminal();

	void get_non_terminal();

	void get_first();

	void get_follow();

    private:
	bool get_first_change();
	bool get_first_gram(const size_t &gram_idx, const deque<size_t> &g);

	bool get_follow_change();
	bool get_follow_gram(const size_t &gram_idx, const deque<size_t> &g);

    public:
	struct item_t {
		size_t left;
		size_t pos;
		size_t look_ahead;
		deque<size_t> right;
		reduce_fn *reduce;

		struct item_hash {
			size_t operator()(const item_t &item) const;
		};
		bool operator==(const item_t &rhs) const;
	};

	using item_set_t = std::unordered_set<item_t, item_t::item_hash>;

	using I_type = std::unordered_map<size_t, item_set_t>;

	size_t closure(const item_t &item);
	size_t closure(const item_set_t &item_set);

	I_type I_map;
	size_t I_start, I_empty;

    private:
	bool try_insert_item(item_set_t &item_set);

    public:
	size_t GOTO(const size_t &item_set_idx, const size_t &gram_idx);

    public:
	string print_closure(const size_t &item_set_idx);

    public:
	struct action_t {
		enum class type_t {
			shift,
			reduce,
			go,
			error,
		} type;
		union action_union_t {
			size_t idx; // shift
			struct reduce_action_t { //reduce
				size_t reduce_num;
				size_t reduce_to;
				reduce_fn *reduce;
			}reduce;
			size_t go; // go
		} action;
	};

	std::unordered_map<size_t, std::unordered_map<size_t, action_t> >
		action_table;

	void get_items_and_action();

    private:
	bool try_new_items_action();
	void try_new_items_action_one(const size_t &item_set_idx,
				      const item_set_t &item_set);
};
}
