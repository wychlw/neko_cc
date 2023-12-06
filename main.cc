#include "out.hh"
#include "parse/lr1/gramma.hh"
#include "parse/lr1/parse.hh"
#include "tok.hh"
#include "util.hh"
#include <bits/stdc++.h>
#include "scan.hh"
#include "parse/parse_base.hh"
#include "gen.hh"
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace neko_cc;

using std::cout, std::endl;

void parser_hook_print(deque<size_t> &state, deque<env_t> &env, tok_t &tok,
		       lex_t &lex, lex_t::action_t &action);

int main(int argc, char *argv[])
{
	log_level = neko_cc::DEBUG;
	if (argc <= 1) {
		err_msg("File expected");
	}
	std::string file_name = argv[1];
	std::fstream f;
	f.open(file_name, std::ios::in);

	std::string path = "test/lex.yml";
	std::fstream l(path);

	lex_t res(l, "S");
	for (auto i : res.comp_gramma) {
		if (res.comp_tok.find(i.first) != res.comp_tok.end()) {
			continue;
		}
		std::cout << res.get_name(i.first) << std::endl;
		for (auto j : i.second.g) {
			std::cout << "\t";
			for (auto k : j.g) {
				std::cout << res.get_name(k) << " ";
			}
			std::cout << std::endl;
		}
	}
	cout << endl;

	cout << "Terminal" << endl;
	for (auto i : res.terminal) {
		std::cout << res.get_name(i) << std::endl;
	}
	cout << endl;

	cout << "Non-Terminal" << endl;
	for (auto i : res.non_terminal) {
		std::cout << res.get_name(i) << std::endl;
	}
	cout << endl;

	cout << "\nFirst" << endl;
	for (auto i : res.comp_gramma) {
		if (!res.non_terminal.count(i.first)) {
			continue;
		}
		std::cout << res.get_name(i.first) << std::endl;
		for (auto v : res.first_set[i.first]) {
			std::cout << "\t" << res.get_name(v) << endl;
		}
	}
	cout << endl;

	cout << "\nFollow" << endl;
	for (auto i : res.comp_gramma) {
		if (!res.terminal.count(i.first) &&
		    !res.non_terminal.count(i.first)) {
			continue;
		}
		std::cout << res.get_name(i.first) << std::endl;
		for (auto v : res.follow_set[i.first]) {
			std::cout << "\t" << res.get_name(v) << endl;
		}
	}
	cout << endl;

	res.get_items_and_action();
	cout << "\nItems" << endl;
	for (auto &[idx, item] : res.I_map) {
		cout << "------------------------------" << endl;
		std::cout << "I " << idx << endl;
		cout << res.print_closure(idx) << endl;
	}

	cout << "\nAction" << endl;
	std::unordered_set<size_t> all;
	all.insert(res.terminal.begin(), res.terminal.end());
	all.insert(res.non_terminal.begin(), res.non_terminal.end());
	cout << "\t";
	for (auto &t : all) {
		std::cout << res.get_name(t) << "\t";
	}
	std::cout << std::endl;
	for (auto &[idx, item] : res.I_map) {
		if (idx == res.I_empty) {
			continue;
		}
		std::cout << "I " << idx << "\t";
		for (auto &t : all) {
			if (!res.action_table[idx].count(t)) {
				std::cout << " \t";
				continue;
			}
			auto &action = res.action_table[idx][t];
			if (action.type == lex_t::action_t::type_t::shift) {
				cout << "S" << action.action.idx << "\t";
			} else if (action.type ==
				   lex_t::action_t::type_t::reduce) {
				cout << "R"
				     << res.get_name(
						action.action.reduce.reduce_to)
				     << "\t";
			} else if (action.type == lex_t::action_t::type_t::go) {
				cout << "G" << action.action.go << "\t";
			} else {
				cout << "E"
				     << "\t";
			}
		}
		std::cout << std::endl;
	}

	cout << "\nParse" << endl;

	init_parse_env(res, &parser_hook_print);
	translation_unit(f);

	cout << "Code Parse Fin." << endl;
}

static deque<size_t> tok_stack;

void parser_hook_print(deque<size_t> &state, deque<env_t> &unused(env),
		       tok_t &tok, lex_t &lex, lex_t::action_t &action)
{
	string state_str;
	for (auto &i : state) {
		state_str += std::to_string(i) + " ";
	}
	cout << std::setw(15) << std::left << state_str;
	string tok_str;
	for (auto &i : tok_stack) {
		tok_str += lex.get_name(i) + " ";
	}
	cout << std::setw(25) << std::left << tok_str;
	string action_str;
	if (action.type == lex_t::action_t::type_t::shift) {
		action_str = "S" + std::to_string(action.action.idx);
	} else if (action.type == lex_t::action_t::type_t::reduce) {
		action_str = "R" + lex.get_name(action.action.reduce.reduce_to);
	} else if (action.type == lex_t::action_t::type_t::go) {
		action_str = "G" + std::to_string(action.action.go);
	} else {
		action_str = "E";
	}
	cout << std::setw(15) << std::left << action_str;
	cout << std::setw(15) << std::left << tok.str << ", "
	     << back_tok_map(tok.type);

	if (action.type == lex_t::action_t::type_t::shift) {
		tok_stack.push_back(lex.tok_idx_map[tok.type]);
	} else if (action.type == lex_t::action_t::type_t::reduce) {
		auto num = action.action.reduce.reduce_num;
		for (size_t i = 0; i < num; i++) {
			tok_stack.pop_back();
		}
		tok_stack.push_back(action.action.reduce.reduce_to);
	}
	cout << endl;
}