TESTS = start main puts setjmp printf
TEST_PEXES = $(TESTS:%=%.pexe)

.PHONY: all
all: $(TEST_PEXES)

%.bc: %.c
	`$(NACL_SDK_ROOT)/tools/nacl_config.py --tool cc -t pnacl` -std=gnu89 -O2 -o $@ $^

%.pexe: %.bc
	`$(NACL_SDK_ROOT)/tools/nacl_config.py --tool finalize -t pnacl` -o $@ $^
