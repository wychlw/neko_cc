#include "parse/lr1/gramma.hh"
#include "parse/parse_base.hh"
#include "scan.hh"
#include "out.hh"
#include "tok.hh"
#include "util.hh"
#include <cassert>
#include <cstddef>
#include <string>
#include <yaml-cpp/node/node.h>

namespace neko_cc
{
std::hash<string> get_hash;

static const size_t empty_idx = get_hash("tok_empty");

lex_t::lex_t(stream &ss)
{
	YAML::Node root = YAML::Load(dynamic_cast<std::istream &>(ss));
	parse_lex(root);
	get_terminal();
	get_non_terminal();
	get_first();
	get_follow();
}

lex_t::lex_t(stream &ss, string start)
{
	YAML::Node root = YAML::Load(dynamic_cast<std::istream &>(ss));
	parse_lex(root);
	start_idx = get_hash("TRANS_UNIT");
	comp_gramma[start_idx] = {
		"TRANS_UNIT",
		start_idx,
		{ { *get_sym_type("null_reduce_fn", reduce_fn),
		    { get_hash(start) } } }
	};
	get_terminal();
	get_non_terminal();
	get_first();
	get_follow();
}

string lex_t::get_name(size_t idx)
{
	if (comp_tok.find(idx) != comp_tok.end()) {
		return back_tok_map(comp_tok[idx]);
	} else if (comp_gramma.find(idx) != comp_gramma.end()) {
		return comp_gramma[idx].name;
	} else {
		return "Unknown";
	}
}

void lex_t::fill_gramma(gramma_t &group, YAML::Node &node)
{
	for (auto lex_group : node) {
		gramma_t::sentence_t g_group;
		for (auto lex : lex_group["lex"]) {
			string lex_name = lex.as<string>();
			size_t lex_idx = get_hash(lex_name);
			if (comp_gramma.find(lex_idx) == comp_gramma.end()) {
				err_msg("Unknown gramma morpheme: " + lex_name);
			}
			g_group.g.push_back(lex_idx);
		}
		if (lex_group["reduce"] &&
		    lex_group["reduce"].as<string>() != "null") {
			string reduce_name = lex_group["reduce"].as<string>();
			g_group.reduce = *get_sym_type(reduce_name, reduce_fn);
		} else {
			g_group.reduce =
				*get_sym_type("null_reduce_fn", reduce_fn);
		}
		group.g.push_back(g_group);
	}
}

void lex_t::parse_lex(YAML::Node &root)
{
	// add all terminal tokens
	for (auto [k, v] : comp_predefine_tok) {
		size_t idx = get_hash(k);
		comp_tok[idx] = v;
		comp_gramma[idx] = { string(k), idx, {} };
		tok_idx_map[v] = idx;
	}

	// first find all the exist grammas
	for (auto gramma : root) {
		size_t idx = get_hash(gramma.first.as<string>());
		comp_gramma[idx] = { gramma.first.as<string>(), idx, {} };
	}

	// fill all the grammars
	for (auto gramma : root) {
		size_t idx = get_hash(gramma.first.as<string>());
		auto &g = comp_gramma[idx];
		fill_gramma(g, gramma.second);
	}
}

void lex_t::get_terminal()
{
	for (auto &[_, gram] : comp_gramma) {
		for (auto &g : gram.g) {
			for (auto &i : g.g) {
				if (comp_tok.count(i)) {
					terminal.insert(i);
				}
			}
		}
	}
	terminal.insert(tok_idx_map[tok_eof]);
}

void lex_t::get_non_terminal()
{
	for (auto &[_, gram] : comp_gramma) {
		for (auto &g : gram.g) {
			for (auto &i : g.g) {
				if (!comp_tok.count(i)) {
					non_terminal.insert(i);
				}
			}
		}
	}
}

void lex_t::get_first()
{
	while (get_first_change())
		;
}

bool lex_t::get_first_change()
{
	bool changed = false;
	for (auto &t : terminal) {
		if (t == empty_idx) {
			continue;
		}
		changed |= first_set[t].insert(t).second;
	}

	for (auto &[gram_idx, gram] : comp_gramma) {
		for (auto &g : gram.g) {
			changed |= get_first_gram(gram_idx, g.g);
		}
	}
	return changed;
}

template <typename S> static size_t union_set(S &a, const S &b)
{
	size_t changed = 0;
	for (auto &i : b) {
		if (!a.count(i)) {
			a.insert(i);
			changed++;
		}
	}
	return changed;
}

bool lex_t::get_first_gram(const size_t &gram_idx, const deque<size_t> &g)
{
	bool changed = false;
	for (auto &i : g) {
		auto &first = first_set[i];
		if (first.size() == 0) {
			continue;
		}
		changed |= union_set(first_set[gram_idx], first);
		break;
	}
	return changed;
}

void lex_t::get_follow()
{
	while (get_follow_change())
		;
}

bool lex_t::get_follow_change()
{
	bool changed = false;
	changed |= follow_set[start_idx].insert(tok_idx_map[tok_eof]).second;

	for (auto &[gram_idx, gram] : comp_gramma) {
		for (auto &g : gram.g) {
			changed |= get_follow_gram(gram_idx, g.g);
		}
	}
	return changed;
}

bool lex_t::get_follow_gram(const size_t &gram_idx, const deque<size_t> &g)
{
	bool changed = false;

	for (auto it = g.begin(); it != g.end(); it++) {
		if (terminal.count(*it)) {
			continue;
		}

		auto next = it + 1;
		if (next == g.end() && non_terminal.count(*it)) {
			changed |= union_set(follow_set[*it],
					     follow_set[gram_idx]);
		} else {
			auto &first = first_set[*next];
			changed |= union_set(follow_set[*it], first);
		}
	}

	return changed;
}

size_t lex_t::item_t::item_hash::operator()(const item_t &item) const
{
	size_t hash = 0;
	hash_combine(hash, item.left);
	hash_combine(hash, item.pos);
	hash_combine(hash, item.look_ahead);
	for (auto &i : item.right) {
		hash_combine(hash, i);
	}
	return hash;
}

bool lex_t::item_t::operator==(const item_t &rhs) const
{
	return left == rhs.left && pos == rhs.pos &&
	       look_ahead == rhs.look_ahead && right == rhs.right;
}

size_t lex_t::closure(const item_t &item)
{
	item_set_t item_set = { item };
	return closure(item_set);
}

size_t lex_t::closure(const item_set_t &item_set)
{
	item_set_t new_set = item_set;
	while (try_insert_item(new_set))
		;
	for (auto &[k, v] : I_map) {
		if (v == new_set) {
			return k;
		}
	}
	size_t new_idx = I_map.size();
	I_map.insert({ new_idx, new_set });
	return new_idx;
}

bool lex_t::try_insert_item(item_set_t &item_set)
{
	bool changed = false;
	auto set_now = deque<item_t>(item_set.begin(), item_set.end());

	for (auto &item : set_now) {
		if (item.pos == item.right.size()) {
			continue;
		}
		if (terminal.count(item.right[item.pos])) {
			continue;
		}
		auto &gram = comp_gramma[item.right[item.pos]];
		auto &beta = item.pos + 1 < item.right.size() ?
				     item.right[item.pos + 1] :
				     item.look_ahead;
		auto &first = first_set[beta];

		for (auto &g : gram.g) {
			for (auto &i : first) {
				item_t new_item = { item.right[item.pos], 0, i,
						    g.g, g.reduce };
				changed |= item_set.insert(new_item).second;
			}
		}
	}
	return changed;
}

size_t lex_t::GOTO(const size_t &item_set_idx, const size_t &gram_idx)
{
	item_set_t new_set;
	auto &item_set = I_map[item_set_idx];
	for (auto &item : item_set) {
		if (item.pos == item.right.size()) {
			continue;
		}
		if (item.right[item.pos] != gram_idx) {
			continue;
		}
		item_t new_item = { item.left, item.pos + 1, item.look_ahead,
				    item.right, item.reduce };
		new_set.insert(new_item);
	}
	return closure(new_set);
}

string lex_t::print_closure(const size_t &item_set_idx)
{
	string res;
	auto &item_set = I_map[item_set_idx];
	for (const auto &item : item_set) {
		res += get_name(item.left) + " -> ";
		for (size_t i = 0; i < item.right.size(); i++) {
			if (i == item.pos) {
				res += " . ";
			}
			res += get_name(item.right[i]) + " ";
		}
		if (item.pos == item.right.size()) {
			res += " . ";
		}
		res += ", " + get_name(item.look_ahead) + "\n";
	}
	return res;
}

void lex_t::get_items_and_action()
{
	// init I_map
	item_set_t empty_set;
	I_empty = closure(empty_set);
	item_t start_item = { start_idx, 0, tok_idx_map[tok_eof],
			      comp_gramma[start_idx].g[0].g,
			      comp_gramma[start_idx].g[0].reduce };
	I_start = closure(start_item);

	while (try_new_items_action())
		;
}

bool lex_t::try_new_items_action()
{
	size_t last_size = I_map.size();
	for (auto &[item_set_idx, item_set] : I_map) {
		try_new_items_action_one(item_set_idx, item_set);
	}
	return last_size != I_map.size();
}

void lex_t::try_new_items_action_one(const size_t &item_set_idx,
				     const item_set_t &item_set)
{
	for (auto &t : terminal) {
		action_t action;
		size_t goto_idx = GOTO(item_set_idx, t);
		if (goto_idx == I_empty) {
			continue;
		}
		action.type = action_t::type_t::shift;
		action.idx = goto_idx;
		action_table[item_set_idx][t] = action;
	}
	for (auto &nt : non_terminal) {
		action_t action;
		size_t goto_idx = GOTO(item_set_idx, nt);
		if (goto_idx == I_empty) {
			continue;
		}
		action.type = action_t::type_t::go;
		action.go = goto_idx;
		action_table[item_set_idx][nt] = action;
	}
	for (auto &item : item_set) {
		if (item.pos != item.right.size()) {
			continue;
		}
		if (item.left == start_idx) {
			// special treatment for accept
			// though I'm considering to treat accept as a
			// special reduce?
			action_t action;
			action.type = action_t::type_t::accept;
			action_table[item_set_idx][tok_idx_map[tok_eof]] =
				action;
			continue;
		}
		action_t action;
		action.type = action_t::type_t::reduce;
		action.reduce = item.reduce;
		action_table[item_set_idx][item.look_ahead] = action;
	}
}

}