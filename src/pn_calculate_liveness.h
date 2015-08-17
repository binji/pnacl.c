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

static void pn_function_make_slot_map(PNModule* module, PNFunction* function) {
  PNBasicBlockId bb_id = 0;

  uint32_t num_avail_slots = function->num_values;
  PNValueId* avail_slots = pn_allocator_alloc(
      &module->temp_allocator, sizeof(PNValueId) * function->num_values,
      sizeof(PNValueId));
  uint32_t n;
  for (n = 0; n < function->num_values; ++n) {
    avail_slots[n] = n;
  }

  uint32_t num_active = 0;
  PNLivenessRange* active = pn_allocator_alloc(
      &module->temp_allocator, sizeof(PNLivenessRange) * function->num_values,
      PN_DEFAULT_ALIGN);

  for (n = function->num_args + function->num_constants;
       n < function->num_values; ++n) {
    PNLivenessRange* range = &function->value_liveness_range[n];
    if (range->first_bb_id > bb_id) {
      /* Evict active intervals that are now finished */
      bb_id = range->first_bb_id;
      uint32_t m = 0;
      while (m < num_active) {
        if (active[m].last_bb_id < bb_id) {
          PNValueId slot_id = active[m].slot_id;
          /* Reheap avail */
          assert(num_avail_slots < function->num_values);
          uint32_t insert_at = num_avail_slots;
          uint32_t parent = (insert_at - 1) / 2;
          while (insert_at != 0 && avail_slots[parent] > slot_id) {
            avail_slots[insert_at] = avail_slots[parent];
            insert_at = parent;
            parent = (parent - 1) / 2;
          }
          avail_slots[insert_at] = slot_id;
          num_avail_slots++;

          /* Remove from active list */
          active[m] = active[--num_active];
        } else {
          ++m;
        }
      }
    }

    /* Pop minimum slot id from heap */
    uint32_t slot_index = 0;
    PNValueId slot_id = avail_slots[slot_index];
    while (slot_index * 2 + 2 < num_avail_slots) {
      PNValueId left_slot_id = avail_slots[slot_index * 2 + 1];
      PNValueId right_slot_id = avail_slots[slot_index * 2 + 2];
      if (left_slot_id < right_slot_id) {
        avail_slots[slot_index] = left_slot_id;
        slot_index = slot_index * 2 + 1;
      } else {
        avail_slots[slot_index] = right_slot_id;
        slot_index = slot_index * 2 + 2;
      }
    }

    if (slot_index * 2 + 1 < num_avail_slots) {
      avail_slots[slot_index] = avail_slots[slot_index * 2 + 1];
    }

    num_avail_slots--;

    /* Append the new active range */
    assert(num_active < function->num_values);
    range->slot_id = slot_id;
    active[num_active++] = *range;
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
    PNLivenessRange* range = &function->value_liveness_range[n];
    range->first_bb_id = PN_INVALID_BB_ID;
    range->last_bb_id = PN_INVALID_BB_ID;
    range->slot_id = PN_INVALID_VALUE_ID;
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

  pn_function_make_slot_map(module, function);

  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
  PN_END_TIME(CALCULATE_LIVENESS);
}
#endif /* PN_CALCULATE_LIVENESS */

#endif /* PN_CALCULATE_LIVENESS_H_ */
