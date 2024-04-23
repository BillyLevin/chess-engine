all: engine magics

engine:
	cc -std=c99 -Wall engine.c -o engine.out

debug:
	cc -std=c99 -Wall -g -O0 engine.c -o engine.out

release:
	cc -std=c99 -Wall -O3 engine.c -o engine.out

magics:
	cc -std=c99 -Wall magics.c -o magics.out
