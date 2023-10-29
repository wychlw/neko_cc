#include "tok.hh"
#include <bits/stdc++.h>
#include "scan.hh"
#include "anal.hh"
#include "gen.hh"
#include <fstream>

using namespace neko_cc;

int main()
{
	// std::fstream f;
	// f.open("test/tok_test0.txt", std::ios::in);
	// tok_t tok = scan(f);
	// while (tok.type != tok_eof) {
	// 	std::cout << "(" << tok.type << ", " << back_tok_map(tok.type)
	// 		  << ", " << tok.str << ")" << std::endl;
	// 	tok = scan(f);
	// }

	std::fstream f;
	f.open("test/tmp_test.txt", std::ios::in);
	std::fstream out;
	out.open("test/out.ll", std::ios::out);
	init_emit_engine(out);
	translation_unit(f);
	release_emit_engine();
	f.close();
	out.close();
	return 0;
}