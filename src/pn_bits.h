/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_BITS_H_
#define PN_BITS_H_

static PNBool pn_is_power_of_two(size_t value) {
  return (value > 0) && ((value) & (value - 1)) == 0;
}

static size_t pn_next_power_of_two(size_t value) {
  size_t ret = 1;
  while (ret < value) {
    ret <<= 1;
  }

  return ret;
}

static inline uint32_t pn_ctz(uint32_t x) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_ctz(x);
#else
  /* See https://en.wikipedia.org/wiki/Find_first_set */
  if (x == 0) return 32;
  uint32_t n = 0;
  if ((x & 0xffff) == 0) { n += 16; x >>= 16; }
  if ((x & 0x00ff) == 0) { n += 8;  x >>= 8; }
  if ((x & 0x000f) == 0) { n += 4;  x >>= 4; }
  if ((x & 0x0003) == 0) { n += 2;  x >>= 2; }
  if ((x & 0x0001) == 0) { n += 1; }
  return n;
#endif
}

static inline uint32_t pn_clz(uint32_t x) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_clz(x);
#else
  /* See https://en.wikipedia.org/wiki/Find_first_set */
  if (x == 0) return 32;
  uint32_t n = 0;
  if ((x & 0xffff0000) == 0) { n += 16; x <<= 16; }
  if ((x & 0xff000000) == 0) { n += 8;  x <<= 8; }
  if ((x & 0xf0000000) == 0) { n += 4;  x <<= 4; }
  if ((x & 0xc0000000) == 0) { n += 2;  x <<= 2; }
  if ((x & 0x80000000) == 0) { n += 1; }
  return n;
#endif
}

static inline uint32_t pn_popcount(uint32_t x) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_popcount(x);
#else
  /* See
   * http://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
   */
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  return (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
#endif
}

static int32_t pn_decode_sign_rotated_value(uint32_t value) {
  if ((value & 1) == 0) {
    return value >> 1;
  }
  if (value != 1) {
    return -(value >> 1);
  } else {
    return INT_MIN;
  }
}

static int64_t pn_decode_sign_rotated_value_int64(uint64_t value) {
  if ((value & 1) == 0) {
    return value >> 1;
  }
  if (value != 1) {
    return -(value >> 1);
  } else {
    return INT64_MIN;
  }
}

#endif /* PN_BITS_H_ */
