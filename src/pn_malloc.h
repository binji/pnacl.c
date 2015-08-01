/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_MALLOC_H_
#define PN_MALLOC_H_

static void* pn_malloc(size_t size) {
  void* ret = malloc(size);
  if (!ret) {
    PN_FATAL("Out of memory.\n");
  }
  return ret;
}

static void* pn_realloc(void* p, size_t size) {
  void* ret = realloc(p, size);
  if (!ret) {
    PN_FATAL("Out of memory.\n");
  }
  return ret;
}

static void* pn_calloc(size_t nmemb, size_t size) {
  void* ret = calloc(nmemb, size);
  if (!ret) {
    PN_FATAL("Out of memory.\n");
  }
  return ret;
}

static char* pn_strdup(char* s) {
  char* ret = strdup(s);
  if (!ret) {
    PN_FATAL("Out of memory.\n");
  }
  return ret;
}

static void pn_free(void* p) {
  free(p);
}

#undef malloc
#define malloc DONT_CALL_THIS_FUNCTION
#undef realloc
#define realloc DONT_CALL_THIS_FUNCTION
#undef calloc
#define calloc DONT_CALL_THIS_FUNCTION
#undef strdup
#define strdup DONT_CALL_THIS_FUNCTION
#undef free
#define free DONT_CALL_THIS_FUNCTION
#undef printf
#define printf DONT_CALL_THIS_FUNCTION

static inline size_t pn_align_down(size_t size, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return size & ~(align - 1);
}

static inline void* pn_align_down_pointer(void* p, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return (void*)((intptr_t)p & ~((intptr_t)align - 1));
}

static inline size_t pn_align_up(size_t size, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return (size + align - 1) & ~(align - 1);
}

static inline void* pn_align_up_pointer(void* p, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return (void*)(((intptr_t)p + align - 1) & ~((intptr_t)align - 1));
}

static inline PNBool pn_is_aligned(size_t size, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return (size & (align - 1)) == 0;
}

static inline PNBool pn_is_aligned_pointer(void* p, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return ((intptr_t)p & (intptr_t)(align - 1)) == 0;
}

#endif /* PN_MALLOC_H_ */
