#include <bits/stdc++.h>
#include "scan.hh"
#include "anal.hh"
#include <fstream>

using namespace neko_cc;

int main()
{
	std::fstream f;
	f.open("test/tmp_test.txt", std::ios::in);
	translation_unit(f);
}