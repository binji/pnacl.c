SHELL = bash

.SUFFIXES:

.PHONY: all
all: out/pnacl out/pnacl-liveness out/pnacl-notrace out/pnacl-notimers \
	out/pnacl-notrace-notimers out/pnacl-opt out/pnacl-opt-assert out/pnacl-msan \
	out/pnacl-asan

CFLAGS = -Wall -Wno-unused-function -Werror -std=gnu89 -g

out/:
	mkdir $@

out/test/:
	mkdir $@

out/pnacl: pnacl.c | out
	gcc $(CFLAGS) -o $@ $^

out/pnacl-liveness: pnacl.c | out
	gcc -DPN_CALCULATE_LIVENESS=1 $(CFLAGS) -o $@ $^

out/pnacl-notrace: pnacl.c | out
	gcc -DPN_TRACING=0 $(CFLAGS) -o $@ $^

out/pnacl-notimers: pnacl.c | out
	gcc -DPN_TIMERS=0 $(CFLAGS) -o $@ $^

out/pnacl-notrace-notimers: pnacl.c | out
	gcc -DPN_TRACING=0 -DPN_TIMERS=0 $(CFLAGS) -o $@ $^

out/pnacl-opt: pnacl.c | out
	gcc -O3 $(CFLAGS) -DNDEBUG -o $@ $^

out/pnacl-opt-assert: pnacl.c | out
	gcc -O3 $(CFLAGS) -o $@ $^

out/pnacl-msan: pnacl.c | out
	clang $(CFLAGS) -fsanitize=memory -fno-omit-frame-pointer -o $@ $^

out/pnacl-asan: pnacl.c | out
	clang $(CFLAGS) -fsanitize=address -fno-omit-frame-pointer -o $@ $^

#### TESTS ####

TESTS = start main
TEST_PEXES = $(TESTS:%=out/test/%.pexe)

.PHONY: test
test: out/pnacl out/pnacl-asan $(TEST_PEXES)
	@set -e; for exe in out/pnacl out/pnacl-asan; do \
		for test in $(TESTS); do \
			PEXE=out/test/$$test.pexe; \
			echo "Testing $$exe -t $$PEXE"; \
			diff -u --label "'output from $$PEXE'" <($$exe -t $$PEXE) test/$$test.c.golden; \
		done; \
	done

.PHONY: reset-golden
reset-golden: out/pnacl $(TEST_PEXES)
	@set -e; for test in $(TESTS); do \
		out/pnacl -t out/test/$$test.pexe > test/$$test.c.golden; \
	done

out/test/%.bc: test/%.c | out/test
	`$(NACL_SDK_ROOT)/tools/nacl_config.py --tool cc -t pnacl` -std=gnu89 -O2 -o $@ $^

out/test/%.pexe: out/test/%.bc | out/test
	`$(NACL_SDK_ROOT)/tools/nacl_config.py --tool finalize -t pnacl` -o $@ $^

#### CLEAN ####

.PHONY: clean
clean:
	rm -rf out/*
