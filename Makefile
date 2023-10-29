all:
	g++ main.cc src/*.cc -Iinc  -o build/t0 -Og -g

run: all
	./build/t0