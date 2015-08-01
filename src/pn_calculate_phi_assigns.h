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
  PN_END_TIME(CALCULATE_PHI_ASSIGNS);
}

#endif /* PN_CALCULATE_PHI_ASSIGNS_H_ */
