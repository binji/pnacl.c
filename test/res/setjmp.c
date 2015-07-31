#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

jmp_buf buf;

int fact(int n) {
  printf("fact(%d)\n", n);
  if (n > 0) {
    int res = n * fact(n - 1);
    if (res > 1000) {
      longjmp(buf, res);
    }

    printf("~fact(%d) => %d\n", n, res);
    return res;
  } else {
    return 1;
  }
}

int main(int argc, char** argv) {
  --argc, ++argv;
  int n = atoi(argv[0]);
  int val = setjmp(buf);
  if (!val) {
    printf("fact(%d) = %d\n", n, fact(n));
  } else {
    printf("longjmp'd. val = %d\n", val);
  }
  return 0;
}
