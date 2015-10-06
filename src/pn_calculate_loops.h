/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_CALCULATE_LOOPS_H_
#define PN_CALCULATE_LOOPS_H_

#if PN_CALCULATE_LOOPS

/* Algorithm from http://lenx.100871.net/papers/loop-SAS.pdf */

static void pn_basic_block_tag_loop_header(PNFunction* function,
                                           PNBasicBlock* bb,
                                           PNBasicBlockId loop_header_id) {
  if (loop_header_id == PN_INVALID_BB_ID) {
    return;
  }

  PNBasicBlock* loop_header = &function->bbs[loop_header_id];
  if (loop_header == bb) {
    return;
  }

  PNBasicBlock* cur1 = bb;
  PNBasicBlock* cur2 = loop_header;
  PNBasicBlockId cur2_id = loop_header_id;
  while (cur1->loop_header_id != PN_INVALID_BB_ID) {
    PNBasicBlockId parent_loop_header_id = cur1->loop_header_id;
    PNBasicBlock* parent_loop_header = &function->bbs[parent_loop_header_id];
    if (parent_loop_header == cur2) {
      return;
    }

    if (parent_loop_header->dfsp_pos < cur2->dfsp_pos) {
      cur1->loop_header_id = cur2_id;
      cur1 = cur2;
      cur2 = parent_loop_header;
      cur2_id = parent_loop_header_id;
    } else {
      cur1 = parent_loop_header;
    }
  }
  cur1->loop_header_id = cur2_id;
}

static PNBasicBlockId pn_basic_block_calculate_loops(PNFunction* function,
                                                     PNBitSet* seen_bbs,
                                                     PNBasicBlockId bb_id,
                                                     uint32_t dfsp_pos) {
  PNBasicBlock* bb = &function->bbs[bb_id];
  pn_bitset_set(seen_bbs, bb_id, PN_TRUE);
  bb->dfsp_pos = dfsp_pos;

  uint32_t n;
  for (n = 0; n < bb->num_succ_bbs; ++n) {
    PNBasicBlockId succ_bb_id = bb->succ_bb_ids[n];
    PNBasicBlock* succ_bb = &function->bbs[succ_bb_id];
    if (!pn_bitset_is_set(seen_bbs, succ_bb_id)) {
      PNBasicBlockId new_loop_header = pn_basic_block_calculate_loops(
          function, seen_bbs, succ_bb_id, dfsp_pos + 1);
      pn_basic_block_tag_loop_header(function, bb, new_loop_header);
    } else {
      if (succ_bb->dfsp_pos > 0) {
        succ_bb->is_loop_header = PN_TRUE;
        pn_basic_block_tag_loop_header(function, bb, succ_bb_id);
      } else if (succ_bb->loop_header_id == PN_INVALID_BB_ID) {
        /* do nothing */
      } else {
        PNBasicBlockId loop_header_id = succ_bb->loop_header_id;
        PNBasicBlock* loop_header = &function->bbs[loop_header_id];
        if (loop_header->dfsp_pos > 0) {
          pn_basic_block_tag_loop_header(function, bb, loop_header_id);
        } else {
          succ_bb->is_reentry = PN_TRUE;
          loop_header->is_irreducible = PN_TRUE;
          while (loop_header->loop_header_id != PN_INVALID_BB_ID) {
            loop_header_id = loop_header->loop_header_id;
            loop_header = &function->bbs[loop_header_id];
            if (loop_header->dfsp_pos > 0) {
              pn_basic_block_tag_loop_header(function, bb, loop_header_id);
              break;
            }
            loop_header->is_irreducible = PN_TRUE;
          }
        }
      }
    }
  }
  bb->dfsp_pos = 0;
  return bb->loop_header_id;
}

static void pn_function_calculate_loops(PNModule* module,
                                        PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_LOOPS);
  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);

  PNBitSet seen_bbs;
  pn_bitset_init(&module->temp_allocator, &seen_bbs, function->num_bbs);

  if (function->num_bbs > 0) {
    pn_basic_block_calculate_loops(function, &seen_bbs, 0, 1);
  }

  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
  PN_END_TIME(CALCULATE_LOOPS);
}

#endif /* PN_CALCULATE_LOOPS */

#endif /* PN_CALCULATE_LOOPS_H_ */
