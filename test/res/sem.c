#include <pthread.h>
#include <stdio.h>
#include <semaphore.h>

#define NUM_THREADS 4

static int work_count;
static sem_t work_sem;
static sem_t done_sem;

static int total;

static void* thread_func(void* arg) {
  sem_wait(&work_sem);
  while (1) {
    int work = __sync_add_and_fetch(&work_count, -1);
    if (work <= 0) {
      break;
    }
    __sync_add_and_fetch(&total, work);
  }
  sem_post(&done_sem);
  return NULL;
}

int main() {
  pthread_t threads[NUM_THREADS];
  int i;
  void* retval;

  sem_init(&work_sem, 0, 0);
  sem_init(&done_sem, 0, 0);

  for (i = 0; i < NUM_THREADS; ++i) {
    pthread_create(&threads[i], NULL, &thread_func, NULL);
  }

  work_count = 101;

  for (i = 0; i < NUM_THREADS; ++i) { sem_post(&work_sem); }
  for (i = 0; i < NUM_THREADS; ++i) { sem_wait(&done_sem); }
  for (i = 0; i < NUM_THREADS; ++i) { pthread_join(threads[i], &retval); }

  printf("total = %d\n", total);

  return 0;
}
