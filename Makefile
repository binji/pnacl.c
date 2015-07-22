SHELL = bash

.SUFFIXES:

CC = clang

.PHONY: all
all: out/pnacl out/pnacl-opt-assert

.PHONY: everything
everything: out/pnacl out/pnacl-liveness out/pnacl-notrace out/pnacl-notimers \
	out/pnacl-notrace-notimers out/pnacl-opt out/pnacl-opt-assert out/pnacl-msan \
	out/pnacl-asan out/pnacl-32

CFLAGS = -Wall -Wno-unused-function -Werror -std=gnu89 -g -lm

out/:
	mkdir $@

out/test/:
	mkdir $@

out/pnacl: pnacl.c | out
	$(CC) $(CFLAGS) -o $@ $^

out/pnacl-liveness: pnacl.c | out
	$(CC) -DPN_CALCULATE_LIVENESS=1 $(CFLAGS) -o $@ $^

out/pnacl-notrace: pnacl.c | out
	$(CC) -DPN_TRACING=0 $(CFLAGS) -o $@ $^

out/pnacl-notimers: pnacl.c | out
	$(CC) -DPN_TIMERS=0 $(CFLAGS) -o $@ $^

out/pnacl-notrace-notimers: pnacl.c | out
	$(CC) -DPN_TRACING=0 -DPN_TIMERS=0 $(CFLAGS) -o $@ $^

out/pnacl-opt: pnacl.c | out
	$(CC) -O3 $(CFLAGS) -DNDEBUG -o $@ $^

out/pnacl-opt-assert: pnacl.c | out
	$(CC) -O3 $(CFLAGS) -o $@ $^

out/pnacl-msan: pnacl.c | out
	clang $(CFLAGS) -fsanitize=memory -fno-omit-frame-pointer -o $@ $^

out/pnacl-asan: pnacl.c | out
	clang $(CFLAGS) -fsanitize=address -fno-omit-frame-pointer -o $@ $^

out/pnacl-32: pnacl.c | out
	$(CC) $(CFLAGS) -m32 -o $@ $^

#### TESTS ####

.PHONY: test
test: out/pnacl out/pnacl-asan
	@make -C test
	@python test/run-tests.py

#### FUZZ ####

AFL_DIR ?= ~/dev/afl/afl-1.83b
FUZZ_IN = fuzz-in
FUZZ_OUT = fuzz-out

.PHONY: fuzz-master fuzz-slave1 fuzz-slave2 fuzz-slave3
fuzz-master: out/pnacl-afl
	$(AFL_DIR)/afl-fuzz -i $(FUZZ_IN) -o $(FUZZ_OUT) -m 800 -M fuzz01 -- $^ @@

fuzz-slave1: out/pnacl-afl
	$(AFL_DIR)/afl-fuzz -i $(FUZZ_IN) -o $(FUZZ_OUT) -m 800 -S fuzz02 -- $^ @@

fuzz-slave2: out/pnacl-afl
	$(AFL_DIR)/afl-fuzz -i $(FUZZ_IN) -o $(FUZZ_OUT) -m 800 -S fuzz03 -- $^ @@

fuzz-slave3: out/pnacl-afl
	$(AFL_DIR)/afl-fuzz -i $(FUZZ_IN) -o $(FUZZ_OUT) -m 800 -S fuzz04 -- $^ @@

out/pnacl-afl: pnacl.c | out
	$(AFL_DIR)/afl-gcc -O3 $(CFLAGS) -o $@ $^

#### CLEAN ####

.PHONY: clean
clean:
	rm -rf out/*
