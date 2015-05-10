.PHONY: all
all: bitcode bitcode-notrace bitcode-notimers bitcode-notrace-notimers bitcode-opt

CFLAGS = -Wall -Wno-unused-function -Werror -std=gnu89 -g

bitcode: bitcode.c
	gcc $(CFLAGS) -o $@ $^

bitcode-notrace: bitcode.c
	gcc -DPN_TRACING=0 $(CFLAGS) -o $@ $^

bitcode-notimers: bitcode.c
	gcc -DPN_TIMERS=0 $(CFLAGS) -o $@ $^

bitcode-notrace-notimers: bitcode.c
	gcc -DPN_TRACING=0 -DPN_TIMERS=0 $(CFLAGS) -o $@ $^

bitcode-opt: bitcode.c
	gcc -O3 $(CFLAGS) -o $@ $^

.PHONY: run
run: bitcode
	./bitcode -tp simple.pexe

.PHONY: debug
debug: bitcode
	gdb ./bitcode

.PHONY: stress
stress: bitcode
	./bitcode -p nacl_io_test.pexe
