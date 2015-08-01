/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_CALCULATE_OPCODES_H_
#define PN_CALCULATE_OPCODES_H_

static void pn_basic_block_calculate_opcodes(PNModule* module,
                                             PNFunction* function,
                                             PNBasicBlock* bb) {
  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);

#define BEGIN_CASE_OPCODE(name) case PN_##name:

#define IF_TYPE(name, type0)                     \
  if (basic_type0 == PN_BASIC_TYPE_##type0) {    \
    i->base.opcode = PN_OPCODE_##name##_##type0; \
  } else

#define IF_TYPE2(name, type0, type1)                       \
  if (basic_type0 == PN_BASIC_TYPE_##type0 &&              \
      basic_type1 == PN_BASIC_TYPE_##type1) {              \
    i->base.opcode = PN_OPCODE_##name##_##type0##_##type1; \
  } else

#define END_CASE_OPCODE(name)                                                \
  {                                                                          \
    PN_ERROR("PN_" #name " with basic type %d unsupported:\n", basic_type0); \
    pn_instruction_trace(module, function, inst, PN_TRUE);                   \
    exit(1);                                                                 \
    break;                                                                   \
  }                                                                          \
  break;

/* Same as END_CASE_OPCODE but with a more convenient name */
#define END_IF_TYPE(name) END_CASE_OPCODE(name)

#define END_CASE_OPCODE2(name)                                         \
  {                                                                    \
    PN_ERROR("PN_" #name " with basic types %d and %d unsupported.\n", \
             basic_type0, basic_type1);                                \
    pn_instruction_trace(module, function, inst, PN_TRUE);             \
    exit(1);                                                           \
    break;                                                             \
  }                                                                    \
  break;

  uint32_t n;
  for (n = 0; n < bb->num_instructions; ++n) {
    PNInstruction* inst = bb->instructions[n];
    switch (inst->code) {
      case PN_FUNCTION_CODE_INST_BINOP: {
        PNInstructionBinop* i = (PNInstructionBinop*)inst;
        PNValue* value0 = pn_function_get_value(module, function, i->value0_id);
        PNValue* value1 = pn_function_get_value(module, function, i->value1_id);
        PNBasicType basic_type0 =
            pn_module_get_type(module, value0->type_id)->basic_type;
        PNBasicType basic_type1 =
            pn_module_get_type(module, value1->type_id)->basic_type;
        if (basic_type0 != basic_type1) {
          PN_FATAL(
              "Expected binop to have the same basic type for each operand. "
              "Got %d and %d\n",
              basic_type0, basic_type1);
        }

        switch (i->binop_opcode) {
          BEGIN_CASE_OPCODE(BINOP_ADD)
            IF_TYPE(BINOP_ADD, DOUBLE)
            IF_TYPE(BINOP_ADD, FLOAT)
            IF_TYPE(BINOP_ADD, INT8)
            IF_TYPE(BINOP_ADD, INT16)
            IF_TYPE(BINOP_ADD, INT32)
            IF_TYPE(BINOP_ADD, INT64)
          END_CASE_OPCODE(BINOP_ADD)

          BEGIN_CASE_OPCODE(BINOP_SUB)
            IF_TYPE(BINOP_SUB, DOUBLE)
            IF_TYPE(BINOP_SUB, FLOAT)
            IF_TYPE(BINOP_SUB, INT8)
            IF_TYPE(BINOP_SUB, INT16)
            IF_TYPE(BINOP_SUB, INT32)
            IF_TYPE(BINOP_SUB, INT64)
          END_CASE_OPCODE(BINOP_SUB)

          BEGIN_CASE_OPCODE(BINOP_MUL)
            IF_TYPE(BINOP_MUL, DOUBLE)
            IF_TYPE(BINOP_MUL, FLOAT)
            IF_TYPE(BINOP_MUL, INT8)
            IF_TYPE(BINOP_MUL, INT16)
            IF_TYPE(BINOP_MUL, INT32)
            IF_TYPE(BINOP_MUL, INT64)
          END_CASE_OPCODE(BINOP_MUL)

          BEGIN_CASE_OPCODE(BINOP_UDIV)
            IF_TYPE(BINOP_UDIV, INT8)
            IF_TYPE(BINOP_UDIV, INT16)
            IF_TYPE(BINOP_UDIV, INT32)
            IF_TYPE(BINOP_UDIV, INT64)
          END_CASE_OPCODE(BINOP_UDIV)

          BEGIN_CASE_OPCODE(BINOP_SDIV)
            IF_TYPE(BINOP_SDIV, DOUBLE)
            IF_TYPE(BINOP_SDIV, FLOAT)
            IF_TYPE(BINOP_SDIV, INT32)
            IF_TYPE(BINOP_SDIV, INT64)
          END_CASE_OPCODE(BINOP_SDIV)

          BEGIN_CASE_OPCODE(BINOP_UREM)
            IF_TYPE(BINOP_UREM, INT8)
            IF_TYPE(BINOP_UREM, INT16)
            IF_TYPE(BINOP_UREM, INT32)
            IF_TYPE(BINOP_UREM, INT64)
          END_CASE_OPCODE(BINOP_UREM)

          BEGIN_CASE_OPCODE(BINOP_SREM)
            IF_TYPE(BINOP_SREM, INT32)
            IF_TYPE(BINOP_SREM, INT64)
          END_CASE_OPCODE(BINOP_SREM)

          BEGIN_CASE_OPCODE(BINOP_SHL)
            IF_TYPE(BINOP_SHL, INT8)
            IF_TYPE(BINOP_SHL, INT16)
            IF_TYPE(BINOP_SHL, INT32)
            IF_TYPE(BINOP_SHL, INT64)
          END_CASE_OPCODE(BINOP_SHL)

          BEGIN_CASE_OPCODE(BINOP_LSHR)
            IF_TYPE(BINOP_LSHR, INT8)
            IF_TYPE(BINOP_LSHR, INT16)
            IF_TYPE(BINOP_LSHR, INT32)
            IF_TYPE(BINOP_LSHR, INT64)
          END_CASE_OPCODE(BINOP_LSHR)

          BEGIN_CASE_OPCODE(BINOP_ASHR)
            IF_TYPE(BINOP_ASHR, INT8)
            IF_TYPE(BINOP_ASHR, INT16)
            IF_TYPE(BINOP_ASHR, INT32)
            IF_TYPE(BINOP_ASHR, INT64)
          END_CASE_OPCODE(BINOP_ASHR)

          BEGIN_CASE_OPCODE(BINOP_AND)
            IF_TYPE(BINOP_AND, INT1)
            IF_TYPE(BINOP_AND, INT8)
            IF_TYPE(BINOP_AND, INT16)
            IF_TYPE(BINOP_AND, INT32)
            IF_TYPE(BINOP_AND, INT64)
          END_CASE_OPCODE(BINOP_AND)

          BEGIN_CASE_OPCODE(BINOP_OR)
            IF_TYPE(BINOP_OR, INT1)
            IF_TYPE(BINOP_OR, INT8)
            IF_TYPE(BINOP_OR, INT16)
            IF_TYPE(BINOP_OR, INT32)
            IF_TYPE(BINOP_OR, INT64)
          END_CASE_OPCODE(BINOP_OR)

          BEGIN_CASE_OPCODE(BINOP_XOR)
            IF_TYPE(BINOP_XOR, INT1)
            IF_TYPE(BINOP_XOR, INT8)
            IF_TYPE(BINOP_XOR, INT16)
            IF_TYPE(BINOP_XOR, INT32)
            IF_TYPE(BINOP_XOR, INT64)
          END_CASE_OPCODE(BINOP_XOR)
        }
        break;
      }

      case PN_FUNCTION_CODE_INST_CAST: {
        PNInstructionCast* i = (PNInstructionCast*)inst;
        PNValue* value = pn_function_get_value(module, function, i->value_id);
        PNType* type0 = pn_module_get_type(module, value->type_id);
        PNType* type1 = pn_module_get_type(module, i->type_id);
        PNBasicType basic_type0 = type0->basic_type;
        PNBasicType basic_type1 = type1->basic_type;

        switch (i->cast_opcode) {
          BEGIN_CASE_OPCODE(CAST_TRUNC)
            IF_TYPE2(CAST_TRUNC, INT8, INT1)
            IF_TYPE2(CAST_TRUNC, INT16, INT1)
            IF_TYPE2(CAST_TRUNC, INT16, INT8)
            IF_TYPE2(CAST_TRUNC, INT32, INT1)
            IF_TYPE2(CAST_TRUNC, INT32, INT8)
            IF_TYPE2(CAST_TRUNC, INT32, INT16)
            IF_TYPE2(CAST_TRUNC, INT64, INT8)
            IF_TYPE2(CAST_TRUNC, INT64, INT16)
            IF_TYPE2(CAST_TRUNC, INT64, INT32)
          END_CASE_OPCODE2(CAST_TRUNC)

          BEGIN_CASE_OPCODE(CAST_ZEXT)
            IF_TYPE2(CAST_ZEXT, INT1, INT8)
            IF_TYPE2(CAST_ZEXT, INT1, INT16)
            IF_TYPE2(CAST_ZEXT, INT1, INT32)
            IF_TYPE2(CAST_ZEXT, INT1, INT64)
            IF_TYPE2(CAST_ZEXT, INT8, INT16)
            IF_TYPE2(CAST_ZEXT, INT8, INT32)
            IF_TYPE2(CAST_ZEXT, INT8, INT64)
            IF_TYPE2(CAST_ZEXT, INT16, INT32)
            IF_TYPE2(CAST_ZEXT, INT16, INT64)
            IF_TYPE2(CAST_ZEXT, INT32, INT64)
          END_CASE_OPCODE2(CAST_ZEXT)

          BEGIN_CASE_OPCODE(CAST_SEXT)
            IF_TYPE2(CAST_SEXT, INT1, INT8)
            IF_TYPE2(CAST_SEXT, INT1, INT16)
            IF_TYPE2(CAST_SEXT, INT1, INT32)
            IF_TYPE2(CAST_SEXT, INT1, INT64)
            IF_TYPE2(CAST_SEXT, INT8, INT16)
            IF_TYPE2(CAST_SEXT, INT8, INT32)
            IF_TYPE2(CAST_SEXT, INT8, INT64)
            IF_TYPE2(CAST_SEXT, INT16, INT32)
            IF_TYPE2(CAST_SEXT, INT16, INT64)
            IF_TYPE2(CAST_SEXT, INT32, INT64)
          END_CASE_OPCODE2(CAST_SEXT)

          BEGIN_CASE_OPCODE(CAST_FPTOUI)
            IF_TYPE2(CAST_FPTOUI, DOUBLE, INT8)
            IF_TYPE2(CAST_FPTOUI, DOUBLE, INT16)
            IF_TYPE2(CAST_FPTOUI, DOUBLE, INT32)
            IF_TYPE2(CAST_FPTOUI, DOUBLE, INT64)
            IF_TYPE2(CAST_FPTOUI, FLOAT, INT8)
            IF_TYPE2(CAST_FPTOUI, FLOAT, INT16)
            IF_TYPE2(CAST_FPTOUI, FLOAT, INT32)
            IF_TYPE2(CAST_FPTOUI, FLOAT, INT64)
          END_CASE_OPCODE2(CAST_FPTOUI)

          BEGIN_CASE_OPCODE(CAST_FPTOSI)
            IF_TYPE2(CAST_FPTOSI, DOUBLE, INT8)
            IF_TYPE2(CAST_FPTOSI, DOUBLE, INT16)
            IF_TYPE2(CAST_FPTOSI, DOUBLE, INT32)
            IF_TYPE2(CAST_FPTOSI, DOUBLE, INT64)
            IF_TYPE2(CAST_FPTOSI, FLOAT, INT8)
            IF_TYPE2(CAST_FPTOSI, FLOAT, INT16)
            IF_TYPE2(CAST_FPTOSI, FLOAT, INT32)
            IF_TYPE2(CAST_FPTOSI, FLOAT, INT64)
          END_CASE_OPCODE2(CAST_FPTOSI)

          BEGIN_CASE_OPCODE(CAST_UITOFP)
            IF_TYPE2(CAST_UITOFP, INT8, DOUBLE)
            IF_TYPE2(CAST_UITOFP, INT8, FLOAT)
            IF_TYPE2(CAST_UITOFP, INT16, DOUBLE)
            IF_TYPE2(CAST_UITOFP, INT16, FLOAT)
            IF_TYPE2(CAST_UITOFP, INT32, DOUBLE)
            IF_TYPE2(CAST_UITOFP, INT32, FLOAT)
            IF_TYPE2(CAST_UITOFP, INT64, DOUBLE)
            IF_TYPE2(CAST_UITOFP, INT64, FLOAT)
          END_CASE_OPCODE2(CAST_UITOFP)

          BEGIN_CASE_OPCODE(CAST_SITOFP)
            IF_TYPE2(CAST_SITOFP, INT8, DOUBLE)
            IF_TYPE2(CAST_SITOFP, INT8, FLOAT)
            IF_TYPE2(CAST_SITOFP, INT16, DOUBLE)
            IF_TYPE2(CAST_SITOFP, INT16, FLOAT)
            IF_TYPE2(CAST_SITOFP, INT32, DOUBLE)
            IF_TYPE2(CAST_SITOFP, INT32, FLOAT)
            IF_TYPE2(CAST_SITOFP, INT64, DOUBLE)
            IF_TYPE2(CAST_SITOFP, INT64, FLOAT)
          END_CASE_OPCODE2(CAST_SITOFP)

          BEGIN_CASE_OPCODE(CAST_FPTRUNC)
            IF_TYPE2(CAST_FPTRUNC, DOUBLE, FLOAT)
          END_CASE_OPCODE2(CAST_FPTRUNC)

          BEGIN_CASE_OPCODE(CAST_FPEXT)
            IF_TYPE2(CAST_FPEXT, FLOAT, DOUBLE)
          END_CASE_OPCODE2(CAST_FPEXT)

          BEGIN_CASE_OPCODE(CAST_BITCAST)
            IF_TYPE2(CAST_BITCAST, DOUBLE, INT64)
            IF_TYPE2(CAST_BITCAST, FLOAT, INT32)
            IF_TYPE2(CAST_BITCAST, INT32, FLOAT)
            IF_TYPE2(CAST_BITCAST, INT64, DOUBLE)
          END_CASE_OPCODE2(CAST_BITCAST)
        }
        break;
      }

      case PN_FUNCTION_CODE_INST_RET: {
        PNInstructionRet* i = (PNInstructionRet*)inst;
        if (i->value_id != PN_INVALID_VALUE_ID) {
          i->base.opcode = PN_OPCODE_RET_VALUE;
        } else {
          i->base.opcode = PN_OPCODE_RET;
        }
        break;
      }

      case PN_FUNCTION_CODE_INST_BR: {
        PNInstructionBr* i = (PNInstructionBr*)inst;
        if (i->value_id != PN_INVALID_VALUE_ID) {
          PNValue* value = pn_function_get_value(module, function, i->value_id);
          PNBasicType basic_type0 =
              pn_module_get_type(module, value->type_id)->basic_type;

          IF_TYPE(BR, INT1)
          END_IF_TYPE(BR)
        } else {
          i->base.opcode = PN_OPCODE_BR;
        }
        break;
      }

      case PN_FUNCTION_CODE_INST_SWITCH: {
        PNInstructionSwitch* i = (PNInstructionSwitch*)inst;
        PNValue* value = pn_function_get_value(module, function, i->value_id);
        PNBasicType basic_type0 =
            pn_module_get_type(module, value->type_id)->basic_type;

        IF_TYPE(SWITCH, INT1)
        IF_TYPE(SWITCH, INT8)
        IF_TYPE(SWITCH, INT16)
        IF_TYPE(SWITCH, INT32)
        IF_TYPE(SWITCH, INT64)
        END_IF_TYPE(SWITCH)
        break;
      }

      case PN_FUNCTION_CODE_INST_PHI: {
        PNInstructionPhi* i = (PNInstructionPhi*)inst;
        i->base.opcode = PN_OPCODE_PHI;
        break;
      }

      case PN_FUNCTION_CODE_INST_ALLOCA: {
        PNInstructionAlloca* i = (PNInstructionAlloca*)inst;
        PNValue* value = pn_function_get_value(module, function, i->size_id);
        PNBasicType basic_type0 =
            pn_module_get_type(module, value->type_id)->basic_type;

        IF_TYPE(ALLOCA, INT32)
        END_IF_TYPE(ALLOCA)
        break;
      }

      case PN_FUNCTION_CODE_INST_LOAD: {
        PNInstructionLoad* i = (PNInstructionLoad*)inst;
        PNBasicType basic_type0 =
            pn_module_get_type(module, i->type_id)->basic_type;

        PNValue* src = pn_function_get_value(module, function, i->src_id);
        PNBasicType src_basic_type =
            pn_module_get_type(module, src->type_id)->basic_type;
        if (src_basic_type != PN_BASIC_TYPE_INT32) {
          PN_FATAL("Expected load src to have the int32 basic type, not %d.\n",
                   src_basic_type);
        }

        IF_TYPE(LOAD, DOUBLE)
        IF_TYPE(LOAD, FLOAT)
        IF_TYPE(LOAD, INT8)
        IF_TYPE(LOAD, INT16)
        IF_TYPE(LOAD, INT32)
        IF_TYPE(LOAD, INT64)
        END_IF_TYPE(LOAD)
        break;
      }

      case PN_FUNCTION_CODE_INST_STORE: {
        PNInstructionStore* i = (PNInstructionStore*)inst;
        PNValue* value = pn_function_get_value(module, function, i->value_id);
        PNBasicType basic_type0 =
            pn_module_get_type(module, value->type_id)->basic_type;

        PNValue* dest = pn_function_get_value(module, function, i->dest_id);
        PNBasicType dest_basic_type =
            pn_module_get_type(module, dest->type_id)->basic_type;
        if (dest_basic_type != PN_BASIC_TYPE_INT32) {
          PN_FATAL(
              "Expected store dest to have the int32 basic type, not %d.\n",
              dest_basic_type);
        }

        IF_TYPE(STORE, DOUBLE)
        IF_TYPE(STORE, FLOAT)
        IF_TYPE(STORE, INT8)
        IF_TYPE(STORE, INT16)
        IF_TYPE(STORE, INT32)
        IF_TYPE(STORE, INT64)
        END_IF_TYPE(STORE)
        break;
      }

      case PN_FUNCTION_CODE_INST_CMP2: {
        PNInstructionCmp2* i = (PNInstructionCmp2*)inst;
        PNValue* value0 = pn_function_get_value(module, function, i->value0_id);
        PNValue* value1 = pn_function_get_value(module, function, i->value1_id);
        PNBasicType basic_type0 =
            pn_module_get_type(module, value0->type_id)->basic_type;
        PNBasicType basic_type1 =
            pn_module_get_type(module, value1->type_id)->basic_type;
        if (basic_type0 != basic_type1) {
          PN_FATAL(
              "Expected cmp2 to have the same basic type for each operand. "
              "Got %d and %d\n",
              basic_type0, basic_type1);
        }

        switch (i->cmp2_opcode) {
          BEGIN_CASE_OPCODE(FCMP_FALSE)
          END_CASE_OPCODE(FCMP_FALSE)

          BEGIN_CASE_OPCODE(FCMP_OEQ)
            IF_TYPE(FCMP_OEQ, DOUBLE)
            IF_TYPE(FCMP_OEQ, FLOAT)
          END_CASE_OPCODE(FCMP_OEQ)

          BEGIN_CASE_OPCODE(FCMP_OGT)
            IF_TYPE(FCMP_OGT, DOUBLE)
            IF_TYPE(FCMP_OGT, FLOAT)
          END_CASE_OPCODE(FCMP_OGT)

          BEGIN_CASE_OPCODE(FCMP_OGE)
            IF_TYPE(FCMP_OGE, DOUBLE)
            IF_TYPE(FCMP_OGE, FLOAT)
          END_CASE_OPCODE(FCMP_OGE)

          BEGIN_CASE_OPCODE(FCMP_OLT)
            IF_TYPE(FCMP_OLT, DOUBLE)
            IF_TYPE(FCMP_OLT, FLOAT)
          END_CASE_OPCODE(FCMP_OLT)

          BEGIN_CASE_OPCODE(FCMP_OLE)
            IF_TYPE(FCMP_OLE, DOUBLE)
            IF_TYPE(FCMP_OLE, FLOAT)
          END_CASE_OPCODE(FCMP_OLE)

          BEGIN_CASE_OPCODE(FCMP_ONE)
            IF_TYPE(FCMP_ONE, DOUBLE)
            IF_TYPE(FCMP_ONE, FLOAT)
          END_CASE_OPCODE(FCMP_ONE)

          BEGIN_CASE_OPCODE(FCMP_ORD)
            IF_TYPE(FCMP_ORD, DOUBLE)
            IF_TYPE(FCMP_ORD, FLOAT)
          END_CASE_OPCODE(FCMP_ORD)

          BEGIN_CASE_OPCODE(FCMP_UNO)
            IF_TYPE(FCMP_UNO, DOUBLE)
            IF_TYPE(FCMP_UNO, FLOAT)
          END_CASE_OPCODE(FCMP_UNO)

          BEGIN_CASE_OPCODE(FCMP_UEQ)
            IF_TYPE(FCMP_UEQ, DOUBLE)
            IF_TYPE(FCMP_UEQ, FLOAT)
          END_CASE_OPCODE(FCMP_UEQ)

          BEGIN_CASE_OPCODE(FCMP_UGT)
            IF_TYPE(FCMP_UGT, DOUBLE)
            IF_TYPE(FCMP_UGT, FLOAT)
          END_CASE_OPCODE(FCMP_UGT)

          BEGIN_CASE_OPCODE(FCMP_UGE)
            IF_TYPE(FCMP_UGE, DOUBLE)
            IF_TYPE(FCMP_UGE, FLOAT)
          END_CASE_OPCODE(FCMP_UGE)

          BEGIN_CASE_OPCODE(FCMP_ULT)
            IF_TYPE(FCMP_ULT, DOUBLE)
            IF_TYPE(FCMP_ULT, FLOAT)
          END_CASE_OPCODE(FCMP_ULT)

          BEGIN_CASE_OPCODE(FCMP_ULE)
            IF_TYPE(FCMP_ULE, DOUBLE)
            IF_TYPE(FCMP_ULE, FLOAT)
          END_CASE_OPCODE(FCMP_ULE)

          BEGIN_CASE_OPCODE(FCMP_UNE)
            IF_TYPE(FCMP_UNE, DOUBLE)
            IF_TYPE(FCMP_UNE, FLOAT)
          END_CASE_OPCODE(FCMP_UNE)

          BEGIN_CASE_OPCODE(FCMP_TRUE)
          END_CASE_OPCODE(FCMP_TRUE)

          BEGIN_CASE_OPCODE(ICMP_EQ)
            IF_TYPE(ICMP_EQ, INT8)
            IF_TYPE(ICMP_EQ, INT16)
            IF_TYPE(ICMP_EQ, INT32)
            IF_TYPE(ICMP_EQ, INT64)
          END_CASE_OPCODE(ICMP_EQ)

          BEGIN_CASE_OPCODE(ICMP_NE)
            IF_TYPE(ICMP_NE, INT8)
            IF_TYPE(ICMP_NE, INT16)
            IF_TYPE(ICMP_NE, INT32)
            IF_TYPE(ICMP_NE, INT64)
          END_CASE_OPCODE(ICMP_NE)

          BEGIN_CASE_OPCODE(ICMP_UGT)
            IF_TYPE(ICMP_UGT, INT8)
            IF_TYPE(ICMP_UGT, INT16)
            IF_TYPE(ICMP_UGT, INT32)
            IF_TYPE(ICMP_UGT, INT64)
          END_CASE_OPCODE(ICMP_UGT)

          BEGIN_CASE_OPCODE(ICMP_UGE)
            IF_TYPE(ICMP_UGE, INT8)
            IF_TYPE(ICMP_UGE, INT16)
            IF_TYPE(ICMP_UGE, INT32)
            IF_TYPE(ICMP_UGE, INT64)
          END_CASE_OPCODE(ICMP_UGE)

          BEGIN_CASE_OPCODE(ICMP_ULT)
            IF_TYPE(ICMP_ULT, INT8)
            IF_TYPE(ICMP_ULT, INT16)
            IF_TYPE(ICMP_ULT, INT32)
            IF_TYPE(ICMP_ULT, INT64)
          END_CASE_OPCODE(ICMP_ULT)

          BEGIN_CASE_OPCODE(ICMP_ULE)
            IF_TYPE(ICMP_ULE, INT8)
            IF_TYPE(ICMP_ULE, INT16)
            IF_TYPE(ICMP_ULE, INT32)
            IF_TYPE(ICMP_ULE, INT64)
          END_CASE_OPCODE(ICMP_ULE)

          BEGIN_CASE_OPCODE(ICMP_SGT)
            IF_TYPE(ICMP_SGT, INT8)
            IF_TYPE(ICMP_SGT, INT16)
            IF_TYPE(ICMP_SGT, INT32)
            IF_TYPE(ICMP_SGT, INT64)
          END_CASE_OPCODE(ICMP_SGT)

          BEGIN_CASE_OPCODE(ICMP_SGE)
            IF_TYPE(ICMP_SGE, INT8)
            IF_TYPE(ICMP_SGE, INT16)
            IF_TYPE(ICMP_SGE, INT32)
            IF_TYPE(ICMP_SGE, INT64)
          END_CASE_OPCODE(ICMP_SGE)

          BEGIN_CASE_OPCODE(ICMP_SLT)
            IF_TYPE(ICMP_SLT, INT8)
            IF_TYPE(ICMP_SLT, INT16)
            IF_TYPE(ICMP_SLT, INT32)
            IF_TYPE(ICMP_SLT, INT64)
          END_CASE_OPCODE(ICMP_SLT)

          BEGIN_CASE_OPCODE(ICMP_SLE)
            IF_TYPE(ICMP_SLE, INT8)
            IF_TYPE(ICMP_SLE, INT16)
            IF_TYPE(ICMP_SLE, INT32)
            IF_TYPE(ICMP_SLE, INT64)
          END_CASE_OPCODE(ICMP_SLE)
        }

        break;
      }

      case PN_FUNCTION_CODE_INST_VSELECT: {
        PNInstructionVselect* i = (PNInstructionVselect*)inst;
        PNValue* value0 =
            pn_function_get_value(module, function, i->true_value_id);
        PNValue* value1 =
            pn_function_get_value(module, function, i->false_value_id);
        PNValue* value2 = pn_function_get_value(module, function, i->cond_id);
        PNBasicType basic_type0 =
            pn_module_get_type(module, value0->type_id)->basic_type;
        PNBasicType basic_type1 =
            pn_module_get_type(module, value1->type_id)->basic_type;
        PNBasicType basic_type2 =
            pn_module_get_type(module, value2->type_id)->basic_type;

        if (basic_type2 != PN_BASIC_TYPE_INT1) {
          PN_FATAL("Expected vselect cond to have basic type int1, not %d.\n",
                   basic_type2);
        }

        if (basic_type0 != basic_type1) {
          PN_FATAL(
              "Expected vselect to have the same basic type for true and false "
              "branches. Got %d and %d\n",
              basic_type0, basic_type1);
        }

        i->base.opcode = PN_OPCODE_VSELECT;
        break;
      }

      case PN_FUNCTION_CODE_INST_CALL: {
        PNInstructionCall* i = (PNInstructionCall*)inst;
        PNValue* callee = pn_function_get_value(module, function, i->callee_id);
        assert(callee->code == PN_VALUE_CODE_FUNCTION);
        PNFunction* callee_function =
            pn_module_get_function(module, callee->index);
        if (callee_function->intrinsic_id != PN_INTRINSIC_NULL) {
          switch (callee_function->intrinsic_id) {
#define PN_INTRINSIC_CHECK(e, name)           \
  case PN_INTRINSIC_##e:                      \
    i->base.opcode = PN_OPCODE_INTRINSIC_##e; \
    break;
            PN_FOREACH_INTRINSIC(PN_INTRINSIC_CHECK)
#undef PN_INTRINSIC_CHECK
            default:
              i->base.opcode = PN_OPCODE_CALL;
              break;
          }
        } else {
          i->base.opcode = PN_OPCODE_CALL;
        }

        /* Specialize some intrinsics based on constant args */
        switch (i->base.opcode) {
          case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I8:
          case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I16:
          case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I32:
          case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I64: {
            uint32_t type_offset =
                i->base.opcode - PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I8;

            PN_CHECK(i->num_args == 4);
            PNValue* opcode =
                pn_function_get_value(module, function, i->arg_ids[0]);
            PN_CHECK(opcode->code == PN_VALUE_CODE_CONSTANT);
            PNConstant* op = pn_function_get_constant(function, opcode->index);
            PN_CHECK(op->basic_type == PN_BASIC_TYPE_INT32);
            switch (op->value.u32) {
              case 1:
                i->base.opcode =
                    PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I8 + type_offset;
                break;
              case 2:
                i->base.opcode =
                    PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I8 + type_offset;
                break;
              case 3:
                i->base.opcode =
                    PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I8 + type_offset;
                break;
              case 4:
                i->base.opcode =
                    PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I8 + type_offset;
                break;
              case 5:
                i->base.opcode =
                    PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I8 + type_offset;
                break;
              case 6:
                i->base.opcode =
                    PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I8 +
                    type_offset;
                break;
              default:
                PN_UNREACHABLE();
                break;
            }
            break;
          }

          default:
            break;
        }

        /* TODO(binji): check arg types against function type? */
        break;
      }

      case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
        PNInstructionCall* i = (PNInstructionCall*)inst;
        i->base.opcode = PN_OPCODE_CALL_INDIRECT;
        /* TODO(binji): check arg types against function type? */
        break;
      }

      case PN_FUNCTION_CODE_INST_UNREACHABLE: {
        PNInstructionUnreachable* i = (PNInstructionUnreachable*)inst;
        i->base.opcode = PN_OPCODE_UNREACHABLE;
      }

      case PN_FUNCTION_CODE_INST_FORWARDTYPEREF: {
        PNInstructionForwardtyperef* i = (PNInstructionForwardtyperef*)inst;
        i->base.opcode = PN_OPCODE_FORWARDTYPEREF;
        break;
      }

      default:
        PN_FATAL("Invalid instruction code: %d\n", inst->code);
        break;
    }
  }

#undef BEGIN_CASE_OPCODE
#undef IF_TYPE
#undef IF_TYPE2
#undef END_CASE_OPCODE
#undef END_CASE_OPCODE2

  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
}

static void pn_function_calculate_opcodes(PNModule* module,
                                          PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_OPCODES);
  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    pn_basic_block_calculate_opcodes(module, function, &function->bbs[n]);
  }
  PN_END_TIME(CALCULATE_OPCODES);
}

#endif /* PN_CALCULATE_OPCODES_H_ */
