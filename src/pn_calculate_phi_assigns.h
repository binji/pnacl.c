/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_CALCULATE_PHI_ASSIGNS_H_
#define PN_CALCULATE_PHI_ASSIGNS_H_

static void pn_function_calculate_phi_assigns(PNModule* module,
                                              PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_PHI_ASSIGNS);
  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;
    for (m = 0; m < bb->num_phi_uses; ++m) {
      PNPhiUse* use = &bb->phi_uses[m];
      assert(use->incoming.bb_id < function->num_bbs);

      PNBasicBlock* incoming_bb = &function->bbs[use->incoming.bb_id];
      incoming_bb->num_phi_assigns++;
    }
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];

    bb->phi_assigns = pn_allocator_alloc(
        &module->allocator, sizeof(PNPhiAssign) * bb->num_phi_assigns,
        PN_DEFAULT_ALIGN);
    bb->num_phi_assigns = 0;
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;
    for (m = 0; m < bb->num_phi_uses; ++m) {
      PNPhiUse* use = &bb->phi_uses[m];
      PNBasicBlock* incoming_bb = &function->bbs[use->incoming.bb_id];
      PNPhiAssign* assign =
          &incoming_bb->phi_assigns[incoming_bb->num_phi_assigns++];

      assign->bb_id = n;
      assign->dest_value_id = use->dest_value_id;
      assign->source_value_id = use->incoming.value_id;
    }
  }

  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);
  PNBitSet seen_bb;
  pn_bitset_init(&module->temp_allocator, &seen_bb, function->num_bbs);
  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    pn_bitset_clear(&seen_bb);

    bb->fast_phi_assign = PN_TRUE;

    uint32_t m;
    uint32_t first;
    PNBasicBlockId bb_id = PN_INVALID_BB_ID;
    PNBool done = PN_FALSE;
    for (m = 0; m < bb->num_phi_assigns && !done; ++m) {
      PNPhiAssign* assign = &bb->phi_assigns[m];
      if (bb_id == PN_INVALID_BB_ID &&
          !pn_bitset_is_set(&seen_bb, assign->bb_id)) {
        pn_bitset_set(&seen_bb, assign->bb_id, PN_TRUE);
        bb_id = assign->bb_id;
        first = m;
      } else if (assign->bb_id == bb_id) {
        /* Look backward to see if this assignment's source is a prior
         * destination. If so, we won't be able to do this phi assign the
         * fast way. */
        uint32_t k;
        for (k = first; k < m; ++k) {
          if (assign->source_value_id == bb->phi_assigns[k].dest_value_id) {
            bb->fast_phi_assign = PN_FALSE;
            done = PN_TRUE;
            break;
          }
        }
      }
    }
  }
  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
  PN_END_TIME(CALCULATE_PHI_ASSIGNS);
}

#endif /* PN_CALCULATE_PHI_ASSIGNS_H_ */
