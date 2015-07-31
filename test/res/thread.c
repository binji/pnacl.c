#include <pthread.h>
#include <stdio.h>

static int data;

static void* thread_func(void* arg) {
  data = *(int*)arg;
  return (void*)42;
}

int main() {
  pthread_t thread;
  int arg = 10;
  pthread_create(&thread, NULL, &thread_func, &arg);
  void* retval = 0;
  pthread_join(thread, &retval);
  printf("data = %d\n", data);
  printf("retval = %p\n", retval);
  return 0;
}
