all:
	g++ main.cc src/*.cc -Iinc  -o build/neko_cc -Og -g

run: all
	./build/neko_cc