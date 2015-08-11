/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_CALCULATE_PRED_BBS_H_
#define PN_CALCULATE_PRED_BBS_H_

#if PN_CALCULATE_LIVENESS
static void pn_function_calculate_pred_bbs(PNModule* module,
                                           PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_PRED_BBS);
  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;
    for (m = 0; m < bb->num_succ_bbs; ++m) {
      PNBasicBlockId succ_bb_id = bb->succ_bb_ids[m];
      assert(succ_bb_id < function->num_bbs);
      PNBasicBlock* succ_bb = &function->bbs[succ_bb_id];
      succ_bb->num_pred_bbs++;
    }
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    bb->pred_bb_ids = pn_allocator_alloc(
        &module->allocator, sizeof(PNBasicBlockId) * bb->num_pred_bbs,
        sizeof(PNBasicBlockId));
    bb->num_pred_bbs = 0;
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;
    for (m = 0; m < bb->num_succ_bbs; ++m) {
      PNBasicBlockId succ_bb_id = bb->succ_bb_ids[m];
      PNBasicBlock* succ_bb = &function->bbs[succ_bb_id];
      succ_bb->pred_bb_ids[succ_bb->num_pred_bbs++] = n;
    }
  }
  PN_END_TIME(CALCULATE_PRED_BBS);
}
#endif /* PN_CALCULATE_LIVENESS */

#endif /* PN_CALCULATE_PRED_BBS_H_ */
