all:
	g++ main.cc src/*.cc -Iinc  -o build/t0 

run: all
	./build/t0