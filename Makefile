all: engine magics

engine:
	cc -std=c99 -Wall -O3 engine.c -o engine.out

magics:
	cc -std=c99 -Wall magics.c -o magics.out
