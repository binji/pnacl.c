/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_TIMESPEC_H_
#define PN_TIMESPEC_H_

void pn_timespec_check(struct timespec* a) {
  assert(a->tv_sec >= 0);
  assert(a->tv_nsec >= 0 && a->tv_nsec < PN_NANOSECONDS_IN_A_SECOND);
}

void pn_timespec_subtract(struct timespec* result,
                          struct timespec* a,
                          struct timespec* b) {
  pn_timespec_check(a);
  pn_timespec_check(b);
  result->tv_sec = a->tv_sec - b->tv_sec;
  result->tv_nsec = a->tv_nsec - b->tv_nsec;
  if (result->tv_nsec < 0) {
    result->tv_sec--;
    result->tv_nsec += PN_NANOSECONDS_IN_A_SECOND;
  }
  pn_timespec_check(result);
}

void pn_timespec_add(struct timespec* result,
                     struct timespec* a,
                     struct timespec* b) {
  pn_timespec_check(a);
  pn_timespec_check(b);
  result->tv_sec = a->tv_sec + b->tv_sec;
  result->tv_nsec = a->tv_nsec + b->tv_nsec;
  if (result->tv_nsec >= PN_NANOSECONDS_IN_A_SECOND) {
    result->tv_sec++;
    result->tv_nsec -= PN_NANOSECONDS_IN_A_SECOND;
  }
  pn_timespec_check(result);
}

double pn_timespec_to_double(struct timespec* t) {
  return (double)t->tv_sec + (double)t->tv_nsec / PN_NANOSECONDS_IN_A_SECOND;
}

#endif /* PN_TIMESPEC_H_ */
