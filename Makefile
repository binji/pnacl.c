.PHONY: all
all: pnacl pnacl-notrace pnacl-notimers pnacl-notrace-notimers pnacl-opt

CFLAGS = -Wall -Wno-unused-function -Werror -std=gnu89 -g

pnacl: pnacl.c
	gcc $(CFLAGS) -o $@ $^

pnacl-notrace: pnacl.c
	gcc -DPN_TRACING=0 $(CFLAGS) -o $@ $^

pnacl-notimers: pnacl.c
	gcc -DPN_TIMERS=0 $(CFLAGS) -o $@ $^

pnacl-notrace-notimers: pnacl.c
	gcc -DPN_TRACING=0 -DPN_TIMERS=0 $(CFLAGS) -o $@ $^

pnacl-opt: pnacl.c
	gcc -O3 $(CFLAGS) -o $@ $^

.PHONY: run
run: pnacl
	./pnacl -tp simple.pexe

.PHONY: debug
debug: pnacl
	gdb ./pnacl

.PHONY: stress
stress: pnacl
	./pnacl -p nacl_io_test.pexe
