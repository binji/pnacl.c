TESTS = \
	res/start \
	res/main \
	res/puts \
	res/setjmp \
	res/printf \
	res/dedupe_incoming \

THREAD_TESTS = \
	res/thread \
	res/sem \

BENCHMARKS = \
	benchmark/res/nbody \
	benchmark/res/fannkuchredux \
	benchmark/res/meteor \
	benchmark/res/fasta \
	benchmark/res/spectralnorm \
	benchmark/res/mandelbrot \
	benchmark/res/binarytrees \

TEST_PEXES = $(TESTS:%=%.pexe)

THREAD_TEST_BCS = $(THREAD_TESTS:%=%.bc)
THREAD_TEST_PEXES = $(THREAD_TESTS:%=%.pexe)

BENCHMARK_PEXES = $(BENCHMARKS:%=%.pexe)
BENCHMARK_O0_X86_64_NEXES = $(BENCHMARKS:%=%.O0.x86-64.nexe)
BENCHMARK_O2_X86_64_NEXES = $(BENCHMARKS:%=%.O2.x86-64.nexe)

.PHONY: all
all: $(TEST_PEXES) $(THREAD_TEST_PEXES) $(BENCHMARK_PEXES) \
	$(BENCHMARK_O0_X86_64_NEXES) $(BENCHMARK_O2_X86_64_NEXES)

NACL_CONFIG = $(NACL_SDK_ROOT)/tools/nacl_config.py
PNACL_CC = $(shell $(NACL_CONFIG) --tool cc -t pnacl)
PNACL_FINALIZE = $(shell $(NACL_CONFIG) --tool finalize -t pnacl)
PNACL_TRANSLATE = $(shell $(NACL_CONFIG) --tool translate -t pnacl)

%.bc: %.c Makefile
	$(PNACL_CC) -std=c99 -O2 -o $@ $<

$(THREAD_TEST_BCS): %.bc: %.c Makefile
	$(PNACL_CC) -std=c99 -O2 -pthread -o $@ $<

%.pexe: %.bc
	$(PNACL_FINALIZE) --no-strip-syms -o $@ $<

%.O0.x86-64.nexe: %.pexe
	$(PNACL_TRANSLATE) -O0 -arch x86-64 -o $@ $<

%.O2.x86-64.nexe: %.pexe
	$(PNACL_TRANSLATE) -O2 -arch x86-64 -o $@ $<
