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

  if (pn_bitset_is_set(&state->seen_values, rel_id)) {
    return;
  }
  pn_bitset_set(&state->seen_values, rel_id, PN_TRUE);

  pn_bitset_clear(&state->livein);
  pn_bitset_clear(&state->seen_bbs);
  pn_bitset_set(&state->seen_bbs, initial_bb_id, PN_TRUE);

  /* Allocate enough space for any predecessor chain. num_bbs is always an
   * upper bound */
  state->bb_id_stack[0] = initial_bb_id;
  uint32_t stack_top = 1;
  PNLivenessRange* range = &function->value_liveness_range[rel_id];

  while (stack_top > 0) {
    PNBasicBlockId bb_id = state->bb_id_stack[--stack_top];
    PNBasicBlock* bb = &function->bbs[bb_id];
    if (value_id >= bb->first_def_id && value_id <= bb->last_def_id) {
      /* Value killed at definition. */
      continue;
    }

    if (pn_bitset_is_set(&state->livein, bb_id)) {
      /* Already processed. */
      continue;
    }

    pn_bitset_set(&state->livein, bb_id, PN_TRUE);

    if (bb_id < range->first_bb_id) {
      range->first_bb_id = bb_id;
    } else if (bb_id > range->last_bb_id) {
      range->last_bb_id = bb_id;
    }

    uint32_t n;
    for (n = 0; n < bb->num_pred_bbs; ++n) {
      PNBasicBlockId pred_bb_id = bb->pred_bb_ids[n];
      if (!pn_bitset_is_set(&state->seen_bbs, pred_bb_id)) {
        assert(stack_top < function->num_bbs);
        state->bb_id_stack[stack_top++] = pred_bb_id;
        pn_bitset_set(&state->seen_bbs, pred_bb_id, PN_TRUE);
      }
    }
  }
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
  pn_bitset_init(&module->temp_allocator, &state.seen_values,
                 function->num_values);
  pn_bitset_init(&module->temp_allocator, &state.seen_bbs, function->num_bbs);
  pn_bitset_init(&module->temp_allocator, &state.livein, function->num_bbs);
  state.bb_id_stack = pn_allocator_alloc(
      &module->temp_allocator, sizeof(PNBasicBlockId) * function->num_bbs,
      sizeof(PNBasicBlockId));

  function->value_liveness_range = pn_allocator_alloc(
      &module->allocator, sizeof(PNLivenessRange) * function->num_values,
      PN_DEFAULT_ALIGN);

  uint32_t n;
  for (n = 0; n < function->num_values; ++n) {
    function->value_liveness_range[n].first_bb_id = PN_INVALID_BB_ID;
    function->value_liveness_range[n].last_bb_id = PN_INVALID_BB_ID;
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    if (bb->first_def_id != PN_INVALID_VALUE_ID &&
        bb->last_def_id != PN_INVALID_VALUE_ID) {
      uint32_t m;
      for (m = bb->first_def_id; m <= bb->last_def_id; ++m) {
        PNValueId rel_id = m - module->num_values;
        function->value_liveness_range[rel_id].first_bb_id = n;
        function->value_liveness_range[rel_id].last_bb_id = n;
      }
    }
  }

  for (n = function->num_bbs; n > 0; --n) {
    PNBasicBlockId bb_id = n - 1;
    pn_basic_block_calculate_liveness(module, function, &state, bb_id);
  }

  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
  PN_END_TIME(CALCULATE_LIVENESS);
}
#endif /* PN_CALCULATE_LIVENESS */

#endif /* PN_CALCULATE_LIVENESS_H_ */
