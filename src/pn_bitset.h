/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_BITSET_H_
#define PN_BITSET_H_

static void pn_bitset_init(PNAllocator* allocator,
                           PNBitSet* bitset,
                           int32_t size) {
  bitset->num_words = (size + 31) >> 5;
  bitset->words = pn_allocator_allocz(
      allocator, sizeof(uint32_t) * bitset->num_words, sizeof(uint32_t));
}

static void pn_bitset_set(PNBitSet* bitset, uint32_t bit, PNBool set) {
  uint32_t word = bit >> 5;
  uint32_t mask = 1 << (bit & 31);
  assert(word < bitset->num_words);

  if (set) {
    bitset->words[word] |= mask;
  } else {
    bitset->words[word] &= ~mask;
  }
}

static PNBool pn_bitset_is_set(PNBitSet* bitset, uint32_t bit) {
  uint32_t word = bit >> 5;
  uint32_t mask = 1 << (bit & 31);
  assert(word < bitset->num_words);

  return (bitset->words[word] & mask) != 0;
}

static uint32_t pn_bitset_num_bits_set(PNBitSet* bitset) {
  uint32_t i;
  uint32_t result = 0;
  for (i = 0; i < bitset->num_words; ++i) {
    result += pn_popcount(bitset->words[i]);
  }
  return result;
}

#endif /* PN_BITSET_H_ */
