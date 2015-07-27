#include <stdio.h>

int main(int argc, char** argv) {
  int i = 0;
  switch (argv[0][0]) {
    case 'a':
    case 'e':
    case 'g':
    case '0':
      i = 1;
      break;

    case 'c':
    case 'f':
    case 'z':
    case '9':
      i = 2;
      break;

    default:
      printf("something else\n");
      break;
  }
  printf("i = %d\n", i);
  return 0;
}
