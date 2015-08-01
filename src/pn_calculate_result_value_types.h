/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_CALCULATE_RESULT_VALUE_TYPES_H_
#define PN_CALCULATE_RESULT_VALUE_TYPES_H_

static PNTypeId pn_type_get_implicit_cast_type(PNModule* module,
                                               PNTypeId type0_id,
                                               PNTypeId type1_id) {
  if (type0_id == type1_id) {
    return type0_id;
  }

  PNType* type0 = pn_module_get_type(module, type0_id);
  PNType* type1 = pn_module_get_type(module, type1_id);

  if (type0->code == PN_TYPE_CODE_FUNCTION &&
      type1->code == PN_TYPE_CODE_INTEGER && type1->width == 32) {
    return type1_id;
  } else if (type0->code == PN_TYPE_CODE_INTEGER && type0->width == 32 &&
             type1->code == PN_TYPE_CODE_FUNCTION) {
    return type1_id;
  } else {
    return PN_INVALID_TYPE_ID;
  }
}

static PNBool pn_function_assign_result_value_type(PNModule* module,
                                                   PNFunction* function,
                                                   PNInstruction* inst,
                                                   PNValueId result_value_id,
                                                   PNValueId value0_id,
                                                   PNValueId value1_id) {
  PNValue* result_value =
      pn_function_get_value(module, function, result_value_id);
  PNValue* value0 = pn_function_get_value(module, function, value0_id);
  PNValue* value1 = pn_function_get_value(module, function, value1_id);
  if (value0->type_id == PN_INVALID_TYPE_ID ||
      value1->type_id == PN_INVALID_TYPE_ID) {
    return PN_FALSE;
  }

  result_value->type_id =
      pn_type_get_implicit_cast_type(module, value0->type_id, value1->type_id);

  if (result_value->type_id == PN_INVALID_TYPE_ID) {
    PN_ERROR("Incompatible types:\n");
    pn_instruction_trace(module, function, inst, PN_TRUE);
    exit(1);
  }

  return PN_TRUE;
}

static PNBool pn_instruction_calculate_result_value_type(PNModule* module,
                                                         PNFunction* function,
                                                         PNInstruction* inst) {
  switch (inst->code) {
    case PN_FUNCTION_CODE_INST_BINOP: {
      PNInstructionBinop* i = (PNInstructionBinop*)inst;
      return pn_function_assign_result_value_type(module, function, inst,
                                                  i->result_value_id,
                                                  i->value0_id, i->value1_id);
    }

    case PN_FUNCTION_CODE_INST_VSELECT: {
      PNInstructionVselect* i = (PNInstructionVselect*)inst;
      return pn_function_assign_result_value_type(
          module, function, inst, i->result_value_id, i->true_value_id,
          i->false_value_id);
    }

    default:
      return PN_TRUE;
  }
}

static void pn_function_calculate_result_value_types(PNModule* module,
                                                     PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_RESULT_VALUE_TYPES);
  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);
  uint32_t num_invalid = 0;
  PNInstruction** invalid = NULL;

  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;
    for (m = 0; m < bb->num_instructions; ++m) {
      PNInstruction* inst = bb->instructions[m];
      if (!pn_instruction_calculate_result_value_type(module, function, inst)) {
        /* One of the types is invalid, try again later */
        pn_allocator_realloc_add(&module->temp_allocator, (void**)&invalid,
                                 sizeof(PNInstruction*),
                                 sizeof(PNInstruction*));
        invalid[num_invalid++] = inst;
      }
    }
  }

  if (num_invalid > 0) {
    uint32_t last_invalid = 0;
    while (last_invalid != num_invalid) {
      last_invalid = num_invalid;

      for (n = 0; n < num_invalid;) {
        if (pn_instruction_calculate_result_value_type(module, function,
                                                       invalid[n])) {
          /* Resolved, remove from the list by swapping with the last element
           * and not moving n */
          invalid[n] = invalid[--num_invalid];
        } else {
          /* Not resolved, keep it in the list */
          ++n;
        }
      }
    }

    if (num_invalid > 0 && last_invalid == num_invalid) {
      PN_ERROR("Unable to resolve types for %d values:\n", num_invalid);
      for (n = 0; n < num_invalid; ++n) {
        pn_instruction_trace(module, function, invalid[n], PN_TRUE);
      }
      exit(1);
    }
  }

  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
  PN_END_TIME(CALCULATE_RESULT_VALUE_TYPES);
}

#endif /* PN_CALCULATE_RESULT_VALUE_TYPES_H_ */
