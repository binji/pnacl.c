/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_CALCULATE_LIVENESS_H_
#define PN_CALCULATE_LIVENESS_H_

#if PN_CALCULATE_LIVENESS
static void pn_basic_block_calculate_liveness_per_value(
    PNModule* module,
    PNFunction* function,
    PNLivenessState* state,
    PNBasicBlockId initial_bb_id,
    PNValueId rel_id) {
  PNValueId value_id = module->num_values + rel_id;

  /* Allocate enough space for any predecessor chain. num_bbs is always an
   * upper bound */
  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);
  PNBasicBlockId* bb_id_stack = pn_allocator_alloc(
      &module->temp_allocator, sizeof(PNBasicBlockId) * function->num_bbs,
      sizeof(PNBasicBlockId));
  bb_id_stack[0] = initial_bb_id;
  uint32_t stack_top = 1;

  while (stack_top > 0) {
    PNBasicBlockId bb_id = bb_id_stack[--stack_top];
    PNBasicBlock* bb = &function->bbs[bb_id];
    if (value_id >= bb->first_def_id && value_id <= bb->last_def_id) {
      /* Value killed at definition. */
      continue;
    }

    if (pn_bitset_is_set(&state->livein[bb_id], rel_id)) {
      /* Already processed. */
      continue;
    }

    pn_bitset_set(&state->livein[bb_id], rel_id, PN_TRUE);

    uint32_t n;
    for (n = 0; n < bb->num_pred_bbs; ++n) {
      PNBasicBlockId pred_bb_id = bb->pred_bb_ids[n];
      pn_bitset_set(&state->liveout[pred_bb_id], rel_id, PN_TRUE);

      assert(stack_top < function->num_bbs);
      bb_id_stack[stack_top++] = pred_bb_id;
    }
  }

  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
}

static void pn_basic_block_calculate_liveness(PNModule* module,
                                              PNFunction* function,
                                              PNLivenessState* state,
                                              PNBasicBlockId bb_id) {
  PNBasicBlock* bb = &function->bbs[bb_id];
  uint32_t n;
  for (n = 0; n < bb->num_phi_uses; ++n) {
    PNPhiIncoming* incoming = &bb->phi_uses[n].incoming;
    if (incoming->value_id <
        module->num_values + function->num_args + function->num_constants) {
      break;
    }

    PNValueId rel_id = incoming->value_id - module->num_values;
    PNBasicBlockId pred_bb_id = incoming->bb_id;
    //    pn_bitset_set(&state->liveout[pred_bb_id], rel_id, PN_TRUE);
    pn_basic_block_calculate_liveness_per_value(module, function, state,
                                                pred_bb_id, rel_id);
  }

  for (n = 0; n < bb->num_uses; ++n) {
    PNValueId value_id = bb->uses[n];
    PNValueId rel_id = value_id - module->num_values;
    pn_basic_block_calculate_liveness_per_value(module, function, state, bb_id,
                                                rel_id);
  }
}

static void pn_function_calculate_liveness(PNModule* module,
                                           PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_LIVENESS);
  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);

  PNLivenessState state;
  state.livein = pn_allocator_alloc(&module->temp_allocator,
                                    sizeof(PNBitSet) * function->num_bbs,
                                    PN_DEFAULT_ALIGN);
  state.liveout = pn_allocator_alloc(&module->temp_allocator,
                                     sizeof(PNBitSet) * function->num_bbs,
                                     PN_DEFAULT_ALIGN);

  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    pn_bitset_init(&module->temp_allocator, &state.livein[n],
                   function->num_values);
    pn_bitset_init(&module->temp_allocator, &state.liveout[n],
                   function->num_values);
  }

  for (n = function->num_bbs; n > 0; --n) {
    PNBasicBlockId bb_id = n - 1;
    pn_basic_block_calculate_liveness(module, function, &state, bb_id);
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;

    uint32_t livein_bits_set = pn_bitset_num_bits_set(&state.livein[n]);
    if (livein_bits_set) {
      bb->num_livein = 0;
      bb->livein = pn_allocator_alloc(&module->allocator,
                                      sizeof(PNValueId) * livein_bits_set,
                                      sizeof(PNValueId));

      for (m = 0; m < function->num_values; ++m) {
        if (pn_bitset_is_set(&state.livein[n], m)) {
          bb->livein[bb->num_livein++] = module->num_values + m;
        }
      }
    }

    uint32_t liveout_bits_set = pn_bitset_num_bits_set(&state.liveout[n]);
    if (liveout_bits_set) {
      bb->num_liveout = 0;
      bb->liveout = pn_allocator_alloc(&module->allocator,
                                       sizeof(PNValueId) * liveout_bits_set,
                                       sizeof(PNValueId));

      for (m = 0; m < function->num_values; ++m) {
        if (pn_bitset_is_set(&state.liveout[n], m)) {
          bb->liveout[bb->num_liveout++] = module->num_values + m;
        }
      }
    }
  }

  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
  PN_END_TIME(CALCULATE_LIVENESS);
}
#endif /* PN_CALCULATE_LIVENESS */

#endif /* PN_CALCULATE_LIVENESS_H_ */
