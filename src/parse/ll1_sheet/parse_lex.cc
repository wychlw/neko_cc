#include "parse/ll1_sheet/base.hh"
#include "parse/parse_base.hh"
#include "scan.hh"
#include "tok.hh"
#include <cstddef>
#include <istream>
#include <string>
#include <utility>
#include <unordered_set>
#include <yaml-cpp/node/parse.h>
#include <yaml-cpp/yaml.h>

#include "out.hh"

namespace neko_cc
{

std::hash<string> get_hash;

lex_t::lex_t(stream &ss)
{
	YAML::Node root = YAML::Load(dynamic_cast<std::istream &>(ss));
	parse_lex(root);
	remove_left_recursion();
	remove_left_traceback();
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
		deque<comp_t> g_group;
		for (auto lex : lex_group["lex"]) {
			string lex_name = lex.as<string>();
			size_t lex_idx = get_hash(lex_name);
			if (comp_gramma.find(lex_idx) == comp_gramma.end()) {
				err_msg("Unknown gramma morpheme: " + lex_name);
			}
			g_group.push_back({lex_idx, comp_t::GRAMMA});
		}
		group.g.push_back(g_group);
	}
}

string lex_t::get_nxt_name(const string &o_name)
{
	return o_name + "_n";
}

void lex_t::parse_lex(YAML::Node &root)
{
	// add all terminal tokens
	for (auto [k, v] : comp_predefine_tok) {
		size_t idx = get_hash(k);
		comp_tok[idx] = v;
		comp_gramma[idx] = {
			string(k), idx, {}, nullptr, nullptr, false
		};
	}

	// first find all the exist grammas
	for (auto gramma : root) {
		size_t idx = get_hash(gramma.first.as<string>());
		comp_gramma[idx] = { gramma.first.as<string>(),
				     idx,
				     {},
				     nullptr,
				     nullptr,
				     false };
	}

	// actions should be defined in other procedure

	// fill all the grammars
	for (auto gramma : root) {
		size_t idx = get_hash(gramma.first.as<string>());
		auto &g = comp_gramma[idx];
		fill_gramma(g, gramma.second);
	}
}

void lex_t::remove_left_recursion()
{
	// step 1: process indirect left recursion
	deque<size_t> unfin_idx;
	for (auto [k, v] : comp_gramma) {
		if (comp_tok.find(k) != comp_tok.end()) {
			continue;
		}
		unfin_idx.push_back(k);
	}
	for (auto it = unfin_idx.begin(); it != unfin_idx.end(); it++) {
		auto &g = comp_gramma[*it];
		for (auto jt = unfin_idx.begin(); jt != it; jt++) {
			auto &rep = comp_gramma[*jt];
			for (auto gram_it = g.g.begin(); gram_it != g.g.end();
			     gram_it++) {
				if (gram_it->empty() ||
				    gram_it->at(0).idx != rep.idx) {
					continue;
				}
				auto ins = replace_left_gramma(*gram_it, rep);
				gram_it = g.g.erase(gram_it);
				gram_it--;
				g.g.insert(g.g.end(), ins.begin(), ins.end());
			}
		}
	}

	// step 2: process direct left recursion
	deque<std::pair<size_t, gramma_t> > comp_gramma_v = {
		comp_gramma.begin(), comp_gramma.end()
	};
	for (auto it = comp_gramma_v.begin(); it != comp_gramma_v.end(); it++) {
		if (!chk_has_direct_left_recursion(it->second)) {
			continue;
		}
		auto alpha = get_alpha(it->second);
		auto beta = get_beta(it->second);
		auto nxt_name = get_nxt_name(it->second.name);
		auto nxt_idx = get_hash(nxt_name);
		comp_t nxt_comp = { nxt_idx, comp_t::GRAMMA };
		for (auto &g : beta) {
			g.push_back(nxt_comp);
		}
		it->second.g = beta;
		for (auto &g : alpha) {
			g.push_back(nxt_comp);
		}
		comp_gramma_v.push_back({ nxt_idx,
					  { nxt_name, nxt_idx, alpha, nullptr,
					    nullptr, true } });
	}
	comp_gramma.clear();
	comp_gramma.insert(comp_gramma_v.begin(), comp_gramma_v.end());
}

deque<deque<comp_t> > lex_t::replace_left_gramma(const deque<comp_t> &group,
						 const gramma_t &rep_g)
{
	deque<deque<comp_t> > res;
	if (group.empty() || group[0].idx != rep_g.idx) {
		return { group };
	}
	for (auto &g : rep_g.g) {
		deque<comp_t> tmp = g;
		tmp.insert(tmp.end(), group.begin() + 1, group.end());
		res.push_back(tmp);
	}
	return res;
}

bool lex_t::chk_has_direct_left_recursion(const gramma_t &g)
{
	for (auto &i : g.g) {
		if (i.empty()) {
			continue;
		}
		if (i[0].idx == g.idx) {
			return true;
		}
	}
	return false;
}

deque<deque<comp_t> > lex_t::get_beta(const gramma_t &group)
{
	deque<deque<comp_t> > res;
	for (auto &g : group.g) {
		if (g.empty()) {
			continue;
		}
		if (g[0].idx != group.idx) {
			res.push_back(g);
		}
	}
	return res;
}

deque<deque<comp_t> > lex_t::get_alpha(const gramma_t &group)
{
	deque<deque<comp_t> > res;
	for (auto &g : group.g) {
		if (g.empty()) {
			continue;
		}
		if (g[0].idx == group.idx) {
			res.push_back({ g.begin() + 1, g.end() });
		}
	}
	return res;
}

void lex_t::remove_left_traceback()
{
	deque<std::pair<size_t, gramma_t> > comp_gramma_v = {
		comp_gramma.begin(), comp_gramma.end()
	};
	for (auto it = comp_gramma_v.begin(); it != comp_gramma_v.end(); it++) {
		if (!chk_has_direct_left_traceback(it->second)) {
			continue;
		}
		auto ins = remove_group_left_traceback(it->second);
		for (auto &i : ins) {
			comp_gramma_v.push_back({ i.idx, i });
		}
	}
	comp_gramma.clear();
	comp_gramma.insert(comp_gramma_v.begin(), comp_gramma_v.end());
}

bool lex_t::chk_has_direct_left_traceback(const gramma_t &g)
{
	std::unordered_map<size_t, size_t> cnt;
	for (auto &i : g.g) {
		if (i.empty()) {
			continue;
		}
		cnt[i[0].idx]++;
	}
	for (auto &i : cnt) {
		if (i.second > 1) {
			return true;
		}
	}
	return false;
}

deque<gramma_t> lex_t::remove_group_left_traceback(gramma_t &g)
{
	deque<gramma_t> res;
	std::unordered_map<size_t, size_t> cnt;
	for (auto &i : g.g) {
		if (i.empty()) {
			continue;
		}
		cnt[i[0].idx]++;
	}
	size_t name_cnt = 0;
	for (auto &i : cnt) {
		if (i.second <= 1) {
			continue;
		}
		auto nxt_name =
			get_nxt_name(g.name) + std::to_string(name_cnt++);
		auto nxt_idx = get_hash(nxt_name);
		comp_t first_comp = { i.first, comp_t::GRAMMA };
		comp_t nxt_comp = { nxt_idx, comp_t::GRAMMA };
		gramma_t nxt_g = {
			nxt_name, nxt_idx, {}, nullptr, nullptr, true
		};
		for (auto it = g.g.begin(); it != g.g.end();) {
			if (it->empty() || it->at(0).idx != i.first) {
				it++;
				continue;
			}
			deque<comp_t> ins{ it->begin() + 1, it->end() };
			nxt_g.g.push_back(ins);
			it = g.g.erase(it);
		}
		i.second = 1;
		g.g.push_back({ first_comp, nxt_comp });
		res.push_back(nxt_g);
	}

	return res;
}

void lex_t::get_first()
{
	for (auto &[k, v] : comp_gramma) {
		first_set[k] = get_group_first(v);
	}
}

std::unordered_map<int, size_t> lex_t::get_group_first(const gramma_t &g)
{
	std::unordered_map<int, size_t> res;
	deque<std::pair<size_t, size_t> > search_queue;
	for (size_t i = 0; i < g.g.size(); i++) {
		auto &group = g.g[i];
		if (group.empty()) {
			continue;
		}
		if (comp_tok.find(group[0].idx) != comp_tok.end()) {
			res[comp_tok[group[0].idx]] = i;
		} else {
			search_queue.push_back({ group[0].idx, i });
		}
	}
	while (!search_queue.empty()) {
		auto [idx, i] = search_queue.front();
		search_queue.pop_front();
		if (comp_gramma.find(idx) == comp_gramma.end()) {
			err_msg("Unknown gramma morpheme: " +
				get_name(comp_tok[idx]));
		}
		auto &gram = comp_gramma[idx].g;
		for (size_t j = 0; j < gram.size(); j++) {
			auto &group = gram[j];
			if (group.empty()) {
				continue;
			}
			if (comp_tok.find(group[0].idx) != comp_tok.end()) {
				res[comp_tok[group[0].idx]] = i;
			} else {
				search_queue.push_back({ group[0].idx, i });
			}
		}
	}
	return res;
}

void lex_t::get_follow()
{
	for (auto &[k, v] : comp_gramma) {
		if (comp_tok.find(k) != comp_tok.end()) {
			continue;
		}
		follow_set[k] = {};
	}
	bool changed = true;
	while (changed) {
		changed = false;
		for (auto &[k, v] : comp_gramma) {
			for (auto &group : v.g) {
				if (group.empty()) {
					continue;
				}
				for (auto it = group.begin(); it != group.end();
				     it++) {
					if (comp_tok.find(it->idx) !=
					    comp_tok.end()) {
						continue;
					}
					if (it + 1 == group.end()) {
						for (auto &i : follow_set[k]) {
							if (follow_set[it->idx]
								    .find(i) ==
							    follow_set[it->idx]
								    .end()) {
								follow_set[it->idx]
									.insert(i);
								changed = true;
							}
						}
					} else if (comp_tok.find(
							   (it + 1)->idx) !=
						   comp_tok.end()) {
						int tok_i =
							comp_tok[(it + 1)->idx];
						if (follow_set[it->idx].find(
							    tok_i) ==
						    follow_set[it->idx].end()) {
							follow_set[it->idx]
								.insert(tok_i);
							changed = true;
						}
					} else {
						for (auto &[i, _] :
						     first_set[(it + 1)->idx]) {
							if (follow_set[it->idx]
								    .find(i) ==
							    follow_set[it->idx]
								    .end()) {
								follow_set[it->idx]
									.insert(i);
								changed = true;
							}
						}
					}
				}
			}
		}
	}
}

static lex_t lex_now;

void init_parse_env(const lex_t &lex)
{
	lex_now = lex;
}

struct parse_pos_t {
	size_t gram_idx;
	size_t group_idx;
	size_t pos_idx;
};

void translation_unit(stream &ss)
{
	while (nxt_tok(ss).type != tok_eof) {
		run_parse(ss);
	}
}

void run_parse(stream &ss)
{
	if (lex_now.first_set[lex_now.start_idx].count(nxt_tok(ss).type) == 0) {
		error("Unexpected token: " + nxt_tok(ss).str, ss, true);
	}
	parse_pos_t start = {
		lex_now.start_idx,
		lex_now.first_set[lex_now.start_idx][nxt_tok(ss).type], 0
	};
	deque<parse_pos_t> parse_stack;
	parse_stack.push_back(start);
	while (!parse_stack.empty()) {
		auto now = parse_stack.back();
		auto &gram = lex_now.comp_gramma[now.gram_idx];
		auto &group = gram.g[now.group_idx];
		if (now.pos_idx >= group.size()) {
			parse_stack.pop_back();
			continue;
		}
		debug_msg("gram: " + gram.name +
			       ", group: " + std::to_string(now.group_idx) +
			       ", pos: " + std::to_string(now.pos_idx) +
			       ", token: " + nxt_tok(ss).str);
		auto &pos = group[now.pos_idx];
		if (lex_now.comp_tok.find(pos.idx) == lex_now.comp_tok.end()) {
			parse_stack.back().pos_idx++;
			if (lex_now.first_set[pos.idx].count(
				    nxt_tok(ss).type) == 0 &&
			    !lex_now.comp_gramma[pos.idx].accept_empty) {
				error("Unexpected token: " + nxt_tok(ss).str,
				      ss, true);
			} else if (lex_now.first_set[pos.idx].count(
					   nxt_tok(ss).type) == 0 &&
				   lex_now.comp_gramma[pos.idx].accept_empty) {
				// nothing to dos
			} else {
				parse_stack.push_back(
					{ pos.idx,
					  lex_now.first_set[pos.idx]
							   [nxt_tok(ss).type],
					  0 });
			}

		} else {
			if (lex_now.comp_tok[pos.idx] != nxt_tok(ss).type) {
				error("Unexpected token: " + nxt_tok(ss).str,
				      ss, true);
			}
			get_tok(ss);
			parse_stack.back().pos_idx++;
		}
	}
}

}