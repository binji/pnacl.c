/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_BITSTREAM_H_
#define PN_BITSTREAM_H_

static void pn_bitstream_init(PNBitStream* bs, void* data, uint32_t data_len) {
  bs->data = data;
  bs->data_len = data_len;
  bs->curword = 0;
  bs->curword_bits = 0;
  bs->bit_offset = 0;
}

static uint32_t pn_bitstream_read_frac_bits(PNBitStream* bs, int num_bits) {
  assert(num_bits <= 32);
  assert(num_bits <= bs->curword_bits);
  uint32_t result;
  if (num_bits == 32) {
    result = bs->curword;
    bs->curword = 0;
  } else {
    result = bs->curword & ((1 << num_bits) - 1);
    bs->curword >>= num_bits;
  }
  bs->curword_bits -= num_bits;
  bs->bit_offset += num_bits;
  return result;
}

static void pn_bitstream_fill_curword(PNBitStream* bs) {
  uint32_t byte_offset = bs->bit_offset >> 3;
  if (byte_offset + sizeof(uint32_t) < bs->data_len) {
    bs->curword_bits = 32;
    bs->curword = *(uint32_t*)(bs->data + byte_offset);
  } else {
    /* Near the end of the stream */
    PN_CHECK(byte_offset <= bs->data_len);
    bs->curword_bits = (bs->data_len - byte_offset) * (sizeof(uint8_t) << 3);
    if (bs->curword_bits) {
      bs->curword = *(uint32_t*)(bs->data + byte_offset);
    }
    assert(bs->curword_bits <= 32);
  }
}

static uint32_t pn_bitstream_read(PNBitStream* bs, int num_bits) {
  assert(num_bits <= 32);
  if (num_bits <= bs->curword_bits) {
    return pn_bitstream_read_frac_bits(bs, num_bits);
  }

  uint32_t result = bs->curword;
  int bits_read = bs->curword_bits;
  int bits_left = num_bits - bs->curword_bits;
  bs->bit_offset += bits_read;
  pn_bitstream_fill_curword(bs);
  PN_CHECK(bits_left <= bs->curword_bits);
  result |= pn_bitstream_read_frac_bits(bs, bits_left) << bits_read;
  return result;
}

static uint32_t pn_bitstream_read_vbr(PNBitStream* bs, int num_bits) {
  uint64_t piece = pn_bitstream_read(bs, num_bits);
  uint64_t hi_mask = 1 << (num_bits - 1);
  if ((piece & hi_mask) == 0) {
    return piece;
  }

  uint64_t lo_mask = hi_mask - 1;
  uint64_t result = 0;
  int shift = 0;
  while (1) {
    PN_CHECK(shift < 64);
    result |= (piece & lo_mask) << shift;
    if ((piece & hi_mask) == 0) {
      /* The value should be < 2**32, or should be sign-extended so the top
       * 32-bits are all 1 */
      PN_CHECK(result <= UINT32_MAX ||
               ((result & 0x80000000) && ((result >> 32) == UINT32_MAX)));
      return result;
    }
    shift += num_bits - 1;
    piece = pn_bitstream_read(bs, num_bits);
  }
}

static uint64_t pn_bitstream_read_vbr_uint64(PNBitStream* bs, int num_bits) {
  uint32_t piece = pn_bitstream_read(bs, num_bits);
  uint32_t hi_mask = 1 << (num_bits - 1);
  if ((piece & hi_mask) == 0) {
    return piece;
  }

  uint32_t lo_mask = hi_mask - 1;
  uint64_t result = 0;
  int shift = 0;
  while (1) {
    PN_CHECK(shift < 64);
    result |= (uint64_t)(piece & lo_mask) << shift;
    if ((piece & hi_mask) == 0) {
      return result;
    }
    shift += num_bits - 1;
    piece = pn_bitstream_read(bs, num_bits);
  }
}

static void pn_bitstream_seek_bit(PNBitStream* bs, uint32_t bit_offset) {
  bs->bit_offset = pn_align_down(bit_offset, 32);
  pn_bitstream_fill_curword(bs);

  bit_offset &= 31;
  if (bit_offset) {
    /* Offset is not aligned, read the unaliged bits */
    pn_bitstream_read_frac_bits(bs, bit_offset);
  }
}

static void pn_bitstream_skip_bytes(PNBitStream* bs, int num_bytes) {
  pn_bitstream_seek_bit(bs, bs->bit_offset + num_bytes * 8);
}

static void pn_bitstream_align_32(PNBitStream* bs) {
  pn_bitstream_seek_bit(bs, pn_align_up(bs->bit_offset, 32));
}

static PNBool pn_bitstream_at_end(PNBitStream* bs) {
  uint32_t byte_offset = bs->bit_offset >> 3;
  return byte_offset == bs->data_len;
}

#endif /* PN_BITSTREAM_H_ */
