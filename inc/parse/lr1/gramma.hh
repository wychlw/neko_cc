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

	/**
	 * @brief Get all used terminals
	 * 
	 */
	void get_terminal();

	/**
	 * @brief Get all used non-terminals
	 * 
	 */
	void get_non_terminal();

	/**
	 * @brief Get first set of all terminals and non-terminals
	 * 
	 */
	void get_first();

	/**
	 * @brief Get follow set of all non-terminals
	 * 
	 */
	void get_follow();

    private:
	/**
	 * @brief Process one step of getting first set closure
	 * 
	 * @return true the first set has changed
	 * @return false the first set has not changed
	 */
	bool get_first_change();

	/**
	 * @brief Process one step of getting first set closure of a given grammar
	 * 
	 * @param gram_idx  the index of the grammar
	 * @param g  the right part of the grammar
	 * @return true the first set of the grammar has changed
	 * @return false the first set of the grammar has not changed
	 */
	bool get_first_gram(const size_t &gram_idx, const deque<size_t> &g);

	/**
	 * @brief Process one step of getting follow set closure
	 * 
	 * @return true the follow set has changed
	 * @return false the follow set has not changed
	 */
	bool get_follow_change();
	/**
	 * @brief Process one step of getting follow set closure of a given grammar
	 * 
	 * @param gram_idx the index of the grammar
	 * @param g the right part of the grammar
	 * @return true the follow set of the grammar has changed
	 * @return false the follow set of the grammar has not changed
	 */
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

	/**
	 * @brief Get the closure of a given item
	 * 
	 * @param item An item in LR parser algorithm
	 * @return size_t The index of the item set
	 */
	size_t closure(const item_t &item);
	/**
	 * @brief Get the closure of a given item set (By may expanding the item set)
	 * 
	 * @param item_set An item set in LR parser algorithm
	 * @return size_t The index of the item set
	 */
	size_t closure(const item_set_t &item_set);

	I_type I_map;
	size_t I_start, I_empty;

    private:
	/**
	 * @brief Try to insert an item into an item set
	 * 
	 * @param item_set An item set in LR parser algorithm
	 * @return true The item set has changed
	 * @return false The item set has not changed
	 */
	bool try_insert_item(item_set_t &item_set);

    public:
	/**
	 * @brief Get the action of a given item set if the parser meets a given token
	 * 
	 * @param item_set_idx The index of the item set
	 * @param gram_idx The index of the grammar
	 * @return size_t The index of the item set
	 */
	size_t GOTO(const size_t &item_set_idx, const size_t &gram_idx);

    public:
	/**
	 * @brief Output the item set
	 * 
	 * @param item_set_idx The index of the item set
	 * @return string 
	 */
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

	/**
	 * @brief Get the action table and goto table of the given grammar
	 * 
	 */
	void get_items_and_action();

    private:
	/**
	 * @brief Try to insert an action into the table.
	 * 
	 * @return true The table has changed
	 * @return false The table has not changed
	 */
	bool try_new_items_action();
	/**
	 * @brief Try to insert an action into the table of a given item set.
	 * 
	 * @param item_set_idx The index of the item set
	 * @param item_set The item set
	 */
	void try_new_items_action_one(const size_t &item_set_idx,
				      const item_set_t &item_set);
};
}
