TESTS = \
	res/start \
	res/main \
	res/puts \
	res/setjmp \
	res/printf \
	benchmark/res/nbody \
	benchmark/res/fannkuchredux \
	benchmark/res/meteor \
	benchmark/res/fasta \
	benchmark/res/spectralnorm \
	benchmark/res/mandelbrot \
	benchmark/res/binarytrees \

TEST_PEXES = $(TESTS:%=%.pexe)

.PHONY: all
all: $(TEST_PEXES)

%.bc: %.c
	`$(NACL_SDK_ROOT)/tools/nacl_config.py --tool cc -t pnacl` -std=c99 -O2 -o $@ $^

%.pexe: %.bc
	`$(NACL_SDK_ROOT)/tools/nacl_config.py --tool finalize -t pnacl` -o $@ $^
