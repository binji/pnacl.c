SHELL = bash

.SUFFIXES:

CC = clang
ALL = pnacl pnacl-opt-assert
EVERYTHING = pnacl pnacl-liveness pnacl-notrace pnacl-notimers \
	pnacl-notrace-notimers pnacl-opt pnacl-opt-assert pnacl-msan pnacl-asan \
	pnacl-ubsan pnacl-32


.PHONY: all
all: $(addprefix out/,$(ALL))

.PHONY: everything
everything: $(addprefix out/,$(EVERYTHING))

CFLAGS = -Wall -Wno-unused-function -Werror -std=gnu89 -g -lm -MMD -MP -MF $@.d

out/:
	mkdir $@

out/test/:
	mkdir $@

out/pnacl: src/pnacl.c | out
	$(CC) $(CFLAGS) -o $@ $<

out/pnacl-liveness: src/pnacl.c | out
	$(CC) -DPN_CALCULATE_LIVENESS=1 $(CFLAGS) -o $@ $<

out/pnacl-notrace: src/pnacl.c | out
	$(CC) -DPN_TRACING=0 $(CFLAGS) -o $@ $<

out/pnacl-notimers: src/pnacl.c | out
	$(CC) -DPN_TIMERS=0 $(CFLAGS) -o $@ $<

out/pnacl-notrace-notimers: src/pnacl.c | out
	$(CC) -DPN_TRACING=0 -DPN_TIMERS=0 $(CFLAGS) -o $@ $<

out/pnacl-opt: src/pnacl.c | out
	$(CC) -O3 $(CFLAGS) -DNDEBUG -o $@ $<

out/pnacl-opt-assert: src/pnacl.c | out
	$(CC) -O3 $(CFLAGS) -o $@ $<

out/pnacl-msan: src/pnacl.c | out
	clang $(CFLAGS) -fsanitize=memory -fno-omit-frame-pointer -o $@ $<

out/pnacl-asan: src/pnacl.c | out
	clang $(CFLAGS) -fsanitize=address -fno-omit-frame-pointer -o $@ $<

out/pnacl-ubsan: src/pnacl.c | out
	clang $(CFLAGS) -fsanitize=undefined -fno-omit-frame-pointer -o $@ $<

out/pnacl-32: src/pnacl.c | out
	$(CC) $(CFLAGS) -m32 -o $@ $<

#### TESTS ####

.PHONY: test
test: out/pnacl-opt-assert
	@make -C test
	@python test/run-tests.py

.PHONY: test-all
test-all: out/pnacl-opt-assert
	@make -C test
	@python test/run-tests.py -s

.PHONY: benchmark
benchmark: out/pnacl-opt-assert
	@make -C test
	@python test/run-benchmarks.py

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

out/pnacl-afl: src/pnacl.c | out
	$(AFL_DIR)/afl-gcc -O3 $(CFLAGS) -o $@ $^

#### CLEAN ####

.PHONY: clean
clean:
	rm -rf out/*

#### DEPS #####

define INCLUDE
-include out/$(1).d
endef

$(foreach target,$(EVERYTHING),$(eval $(call INCLUDE,$(target))))
