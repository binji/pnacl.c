/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_CALCULATE_USES_H_
#define PN_CALCULATE_USES_H_

static void pn_basic_block_calculate_uses(PNModule* module,
                                          PNFunction* function,
                                          PNBasicBlock* bb) {
  PNValueId first_function_value_id =
      module->num_values + function->num_args + function->num_constants;
  PNBitSet uses;
  pn_bitset_init(&module->temp_allocator, &uses, function->num_values);

#define PN_SET_VALUE_USE(value_id)                                \
  if (value_id >= first_function_value_id) {                      \
    pn_bitset_set(&uses, value_id - module->num_values, PN_TRUE); \
  }

  uint32_t n;
  for (n = 0; n < bb->num_instructions; ++n) {
    PNInstruction* inst = bb->instructions[n];
    switch (inst->code) {
      case PN_FUNCTION_CODE_INST_BINOP: {
        PNInstructionBinop* i = (PNInstructionBinop*)inst;
        PN_SET_VALUE_USE(i->value0_id);
        PN_SET_VALUE_USE(i->value1_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_CAST: {
        PNInstructionCast* i = (PNInstructionCast*)inst;
        PN_SET_VALUE_USE(i->value_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_RET: {
        PNInstructionRet* i = (PNInstructionRet*)inst;
        if (i->value_id != PN_INVALID_VALUE_ID) {
          PN_SET_VALUE_USE(i->value_id);
        }
        break;
      }

      case PN_FUNCTION_CODE_INST_BR: {
        PNInstructionBr* i = (PNInstructionBr*)inst;
        if (i->value_id != PN_INVALID_VALUE_ID) {
          PN_SET_VALUE_USE(i->value_id);
        }
        break;
      }

      case PN_FUNCTION_CODE_INST_SWITCH: {
        PNInstructionSwitch* i = (PNInstructionSwitch*)inst;
        PN_SET_VALUE_USE(i->value_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_PHI: {
        PNInstructionPhi* i = (PNInstructionPhi*)inst;
        int32_t n;
        for (n = 0; n < i->num_incoming; ++n) {
          pn_allocator_realloc_add(&module->allocator, (void**)&bb->phi_uses,
                                   sizeof(PNPhiUse), PN_DEFAULT_ALIGN);
          PNPhiUse* use = &bb->phi_uses[bb->num_phi_uses++];
          use->dest_value_id = i->result_value_id;
          use->incoming = i->incoming[n];
        }
      }

      case PN_FUNCTION_CODE_INST_ALLOCA: {
        PNInstructionAlloca* i = (PNInstructionAlloca*)inst;
        PN_SET_VALUE_USE(i->size_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_LOAD: {
        PNInstructionLoad* i = (PNInstructionLoad*)inst;
        PN_SET_VALUE_USE(i->src_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_STORE: {
        PNInstructionStore* i = (PNInstructionStore*)inst;
        PN_SET_VALUE_USE(i->dest_id);
        PN_SET_VALUE_USE(i->value_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_CMP2: {
        PNInstructionCmp2* i = (PNInstructionCmp2*)inst;
        PN_SET_VALUE_USE(i->value0_id);
        PN_SET_VALUE_USE(i->value1_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_VSELECT: {
        PNInstructionVselect* i = (PNInstructionVselect*)inst;
        PN_SET_VALUE_USE(i->cond_id);
        PN_SET_VALUE_USE(i->true_value_id);
        PN_SET_VALUE_USE(i->false_value_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_CALL:
      case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
        PNInstructionCall* i = (PNInstructionCall*)inst;
        if (i->is_indirect) {
          PN_SET_VALUE_USE(i->callee_id);
        }

        uint32_t m;
        for (m = 0; m < i->num_args; ++m) {
          PN_SET_VALUE_USE(i->arg_ids[m]);
        }
        break;
      }

      case PN_FUNCTION_CODE_INST_UNREACHABLE:
      case PN_FUNCTION_CODE_INST_FORWARDTYPEREF:
        break;

      default:
        PN_FATAL("Invalid instruction code: %d\n", inst->code);
        break;
    }
  }

  bb->uses = pn_allocator_alloc(
      &module->allocator, sizeof(PNValueId) * pn_bitset_num_bits_set(&uses),
      sizeof(PNValueId));

  uint32_t w;
  for (w = 0; w < uses.num_words; ++w) {
    uint32_t word = uses.words[w];
    if (!word) {
      continue;
    }

    uint32_t b;
    uint32_t first_bit = pn_ctz(word);
    uint32_t last_bit = 32 - pn_clz(word);
    if (last_bit == 32) last_bit = 31;
    for (b = first_bit; b <= last_bit; ++b) {
      if (word & (1U << b)) {
        bb->uses[bb->num_uses++] = module->num_values + (w << 5) + b;
      }
    }
  }

#undef PN_SET_VALUE_USE
}

static void pn_function_calculate_uses(PNModule* module, PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_USES);
  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);
  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    pn_basic_block_calculate_uses(module, function, &function->bbs[n]);
  }
  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
  PN_END_TIME(CALCULATE_USES);
}

#endif /* PN_CALCULATE_USES_H_ */
