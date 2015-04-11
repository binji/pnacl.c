bitcode: bitcode.c
	gcc -Wall -Wno-unused-function -Werror -std=gnu89 -g -o $@ $^

bitcode-opt: bitcode.c
	gcc -O3 -Wall -Wno-unused-function -Werror -std=gnu89 -g -o $@ $^

.PHONY: run
run: bitcode
	./bitcode simple.pexe

.PHONY: debug
debug: bitcode
	gdb ./bitcode

.PHONY: stress
stress: bitcode
	./bitcode nacl_io_test.pexe
