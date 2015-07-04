.PHONY: all
all: out/pnacl out/pnacl-notrace out/pnacl-notimers out/pnacl-notrace-notimers \
  out/pnacl-opt out/pnacl-msan out/pnacl-asan

CFLAGS = -Wall -Wno-unused-function -Werror -std=gnu89 -g

out/pnacl: pnacl.c
	gcc $(CFLAGS) -o $@ $^

out/pnacl-notrace: pnacl.c
	gcc -DPN_TRACING=0 $(CFLAGS) -o $@ $^

out/pnacl-notimers: pnacl.c
	gcc -DPN_TIMERS=0 $(CFLAGS) -o $@ $^

out/pnacl-notrace-notimers: pnacl.c
	gcc -DPN_TRACING=0 -DPN_TIMERS=0 $(CFLAGS) -o $@ $^

out/pnacl-opt: pnacl.c
	gcc -O3 $(CFLAGS) -DNDEBUG -o $@ $^

out/pnacl-msan: pnacl.c
	clang $(CFLAGS) -fsanitize=memory -fno-omit-frame-pointer -o $@ $^

out/pnacl-asan: pnacl.c
	clang $(CFLAGS) -fsanitize=address -fno-omit-frame-pointer -o $@ $^

.PHONY: clean
clean:
	rm -f out/*

.PHONY: run
run: out/pnacl
	$< -tp test/simple.pexe

.PHONY: debug
debug: out/pnacl
	gdb $<

.PHONY: stress
stress: out/pnacl
	$< -p test/nacl_io_test.pexe
