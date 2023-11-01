#include "out.hh"
#include "tok.hh"
#include <bits/stdc++.h>
#include "scan.hh"
// #include "anal.hh"
// #include "gen.hh"

using namespace neko_cc;

int main(int argc, char *argv[])
{
	log_level = neko_cc::INFO;
	if (argc <= 1) {
		err_msg("File expected");
	}
	std::string file_name = argv[1];
	std::fstream f;
	f.open(file_name, std::ios::in);
	tok_t tok = scan(f);
	while (tok.type != tok_eof) {
		std::cout << "(" << tok.type << ", " << back_tok_map(tok.type)
			  << ", " << tok.str << ")" << std::endl;
		tok = scan(f);
	}

	// std::fstream out;
	// out.open("test/out.ll", std::ios::out);
	// init_emit_engine(out);
	// translation_unit(f);
	// release_emit_engine();
	// f.close();
	// out.close();
	return 0;
}