#include "out.hh"
#include "parse/ll1_sheet/base.hh"
#include "tok.hh"
#include <bits/stdc++.h>
#include "scan.hh"
#include "parse/parse_base.hh"
#include "parse/ll1_sheet/parse_lex.hh"
#include "gen.hh"
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

	lex_t res(l);
	for (auto i : res.comp_gramma) {
		if (res.comp_tok.find(i.first) != res.comp_tok.end()) {
			continue;
		}
		std::cout << res.get_name(i.first) << std::endl;
		if (res.get_name(i.first)=="S")
		{
			res.start_idx = i.first;
		}
		for (auto j : i.second.g) {
			std::cout << "\t";
			for (auto k : j) {
				std::cout << res.get_name(k.idx) << " ";
			}
			std::cout << std::endl;
		}
		if (i.second.accept_empty) {
			std::cout << "\tempty" << std::endl;
		}
	}

	cout << "\nFirst" << endl;
	for (auto i : res.comp_gramma) {
		if (res.comp_tok.find(i.first) != res.comp_tok.end()) {
			continue;
		}
		std::cout << res.get_name(i.first) << std::endl;
		for (auto [k, v] : res.first_set[i.first]) {
			std::cout << "\t" << back_tok_map(k) << endl;
		}
	}

	cout << "\nFollow" << endl;
	for (auto i : res.comp_gramma) {
		if (res.comp_tok.find(i.first) != res.comp_tok.end()) {
			continue;
		}
		std::cout << res.get_name(i.first) << std::endl;
		for (auto k : res.follow_set[i.first]) {
			std::cout << "\t" << back_tok_map(k) << endl;
		}
	}

	init_parse_env(res);
	translation_unit(f);
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