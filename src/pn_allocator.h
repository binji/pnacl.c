/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_ALLOCATOR_H_
#define PN_ALLOCATOR_H_

static void pn_allocator_init(PNAllocator* allocator,
                              size_t min_chunk_size,
                              const char* name) {
  assert(pn_is_power_of_two(min_chunk_size));
  assert(min_chunk_size > sizeof(PNAllocatorChunk));

  allocator->name = name;
  allocator->chunk_head = NULL;
  allocator->last_alloc = NULL;
  allocator->min_chunk_size = min_chunk_size;
  allocator->total_used = 0;
  allocator->internal_fragmentation = 0;
}

static size_t pn_max(size_t x, size_t y) {
  return x > y ? x : y;
}

static PNAllocatorChunk* pn_allocator_new_chunk(PNAllocator* allocator,
                                                size_t initial_alloc_size,
                                                uint32_t align) {
  PN_CHECK(initial_alloc_size < 0x80000000);
  if (allocator->chunk_head) {
    allocator->internal_fragmentation +=
        (allocator->chunk_head->end - allocator->chunk_head->current);
  }

  size_t chunk_size = pn_next_power_of_two(
      pn_max(initial_alloc_size + sizeof(PNAllocatorChunk) + align - 1,
             allocator->min_chunk_size));
  PNAllocatorChunk* chunk = pn_malloc(chunk_size);
  assert(pn_is_aligned_pointer(chunk, sizeof(void*)));

  chunk->current =
      pn_align_up_pointer((void*)chunk + sizeof(PNAllocatorChunk), align);
  chunk->end = (void*)chunk + chunk_size;
  chunk->next = allocator->chunk_head;
  allocator->chunk_head = chunk;
  return chunk;
}

static void* pn_allocator_alloc(PNAllocator* allocator,
                                size_t size,
                                uint32_t align) {
  PNAllocatorChunk* chunk = allocator->chunk_head;
  void* ret;
  void* new_current;

  if (chunk) {
    ret = pn_align_up_pointer(chunk->current, align);
    new_current = ret + size;
    PN_CHECK(new_current >= ret);
  }

  if (!chunk || new_current > chunk->end) {
    chunk = pn_allocator_new_chunk(allocator, size, align);
    ret = pn_align_up_pointer(chunk->current, align);
    new_current = ret + size;
    PN_CHECK(new_current >= ret);
    assert(new_current <= chunk->end);
  }

  chunk->current = new_current;
  allocator->last_alloc = ret;
  allocator->total_used += size;
  return ret;
}

static void* pn_allocator_allocz(PNAllocator* allocator,
                                 size_t size,
                                 uint32_t align) {
  void* p = pn_allocator_alloc(allocator, size, align);
  memset(p, 0, size);
  return p;
}

static void* pn_allocator_realloc_add(PNAllocator* allocator,
                                      void** p,
                                      size_t add_size,
                                      uint32_t align) {
  if (!*p) {
    *p = pn_allocator_alloc(allocator, add_size, align);
    return *p;
  }

  if (*p != allocator->last_alloc) {
    PN_FATAL(
        "Attempting to realloc, but it was not the last allocation:\n"
        "p = %p, last_alloc = %p\n",
        *p, allocator->last_alloc);
  }

  PNAllocatorChunk* chunk = allocator->chunk_head;
  assert(chunk);
  void* ret = chunk->current;
  void* new_current = chunk->current + add_size;
  PN_CHECK(new_current > chunk->current);

  if (new_current > chunk->end) {
    /* Doesn't fit, alloc a new chunk */
    size_t old_size = chunk->current - *p;
    size_t new_size = old_size + add_size;
    chunk = pn_allocator_new_chunk(allocator, new_size, align);

    assert(chunk->current + new_size <= chunk->end);
    memcpy(chunk->current, *p, old_size);
    *p = chunk->current;
    ret = chunk->current + old_size;
    new_current = ret + add_size;
  }

  chunk->current = new_current;
  allocator->last_alloc = *p;
  allocator->total_used += add_size;
  return ret;
}

static size_t pn_allocator_last_alloc_size(PNAllocator* allocator) {
  PNAllocatorChunk* chunk = allocator->chunk_head;
  assert(chunk);
  return chunk->current - allocator->last_alloc;
}

static PNAllocatorMark pn_allocator_mark(PNAllocator* allocator) {
  PNAllocatorMark mark;
  mark.current = allocator->chunk_head ? allocator->chunk_head->current : 0;
  mark.last_alloc = allocator->last_alloc;
  mark.total_used = allocator->total_used;
  mark.internal_fragmentation = allocator->internal_fragmentation;
  return mark;
}

static void pn_allocator_reset_to_mark(PNAllocator* allocator,
                                       PNAllocatorMark mark) {
  /* Free chunks until last_alloc is found */
  PNAllocatorChunk* chunk = allocator->chunk_head;
  while (chunk) {
    if (mark.last_alloc >= (void*)chunk &&
        mark.last_alloc < (void*)chunk->end) {
      break;
    }

    PNAllocatorChunk* next = chunk->next;
    pn_free(chunk);
    chunk = next;
  }

  if (chunk) {
    chunk->current = mark.current;
  }
  allocator->chunk_head = chunk;
  allocator->last_alloc = mark.last_alloc;
  allocator->total_used = mark.total_used;
  allocator->internal_fragmentation = mark.internal_fragmentation;
}

#endif /* PN_ALLOCATOR_H_ */
