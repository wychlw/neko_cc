#include "out.hh"
#include "parse/lr1/gramma.hh"
#include "tok.hh"
#include <bits/stdc++.h>
#include "scan.hh"
#include "parse/parse_base.hh"
#include "gen.hh"
#include <cstddef>
#include <fstream>
#include <iostream>

using namespace neko_cc;

using std::cout, std::endl;

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
				cout << "S" << action.idx << "\t";
			} else if (action.type ==
				   lex_t::action_t::type_t::reduce) {
				cout << "R"
				     << "\t";
			} else if (action.type == lex_t::action_t::type_t::go) {
				cout << "G" << action.idx << "\t";
			} else if (action.type ==
				   lex_t::action_t::type_t::accept) {
				cout << "A"
				     << "\t";
			} else {
				cout << "E"
				     << "\t";
			}
		}
		std::cout << std::endl;
	}
}

/*
int main(int argc, char *argv[])
{
	log_level = neko_cc::WARN;
	if (argc <= 1) {
		err_msg("File expected");
	}
	std::string file_name = argv[1];
	std::fstream f;
	f.open(file_name, std::ios::in);
	// tok_t tok = scan(f);
	// while (tok.type != tok_eof) {
	// 	std::cout << "(" << tok.type << ", " << back_tok_map(tok.type)
	// 		  << ", " << tok.str << ")" << std::endl;
	// 	tok = scan(f);
	// }

	std::fstream out;
	out.open("test/out.ll", std::ios::out);
	init_emit_engine(out, true);
	translation_unit(f);
	release_emit_engine();
	f.close();
	out.close();
	std::cout << "Code PASS." << std::endl;
	return 0;
}
*/