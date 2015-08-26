/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_CALCULATE_OPCODES_H_
#define PN_CALCULATE_OPCODES_H_

static void* pn_basic_block_write_instruction_stream(PNModule* module,
                                                     PNFunction* function,
                                                     PNBasicBlock* bb,
                                                     void** bb_offsets,
                                                     void* offset,
                                                     PNBool write) {
#define PN_BEGIN_CASE_OPCODE(name) case PN_##name:

#define PN_IF_TYPE(name, type0)                  \
  if (basic_type0 == PN_BASIC_TYPE_##type0) {    \
    o->base.opcode = PN_OPCODE_##name##_##type0; \
  } else

#define PN_IF_TYPE2(name, type0, type1)                    \
  if (basic_type0 == PN_BASIC_TYPE_##type0 &&              \
      basic_type1 == PN_BASIC_TYPE_##type1) {              \
    o->base.opcode = PN_OPCODE_##name##_##type0##_##type1; \
  } else

#define PN_END_IF_TYPE(name) \
  {                                                                          \
    PN_ERROR("PN_" #name " with basic type %d unsupported:\n", basic_type0); \
    pn_instruction_trace(module, function, inst, PN_TRUE);                   \
    exit(1);                                                                 \
  }                                                                          \

#define PN_END_CASE_OPCODE(name) \
  PN_END_IF_TYPE(name)           \
  break;

#define PN_END_CASE_OPCODE2(name)                                      \
  {                                                                    \
    PN_ERROR("PN_" #name " with basic types %d and %d unsupported.\n", \
             basic_type0, basic_type1);                                \
    pn_instruction_trace(module, function, inst, PN_TRUE);             \
    exit(1);                                                           \
  }                                                                    \
  break;

  PNBool write_phi_assigns = PN_FALSE;
  PNInstruction* inst;
  for (inst = bb->instructions; inst; inst = inst->next) {
    switch (inst->code) {
      case PN_FUNCTION_CODE_INST_BINOP: {
        if (write) {
          PNInstructionBinop* i = (PNInstructionBinop*)inst;
          PNRuntimeInstructionBinop* o = (PNRuntimeInstructionBinop*)offset;

          PNValue* value0 =
              pn_function_get_value(module, function, i->value0_id);
          PNValue* value1 =
              pn_function_get_value(module, function, i->value1_id);
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
            // clang-format off
            PN_BEGIN_CASE_OPCODE(BINOP_ADD)
              PN_IF_TYPE(BINOP_ADD, DOUBLE)
              PN_IF_TYPE(BINOP_ADD, FLOAT)
              PN_IF_TYPE(BINOP_ADD, INT8)
              PN_IF_TYPE(BINOP_ADD, INT16)
              PN_IF_TYPE(BINOP_ADD, INT32)
              PN_IF_TYPE(BINOP_ADD, INT64)
            PN_END_CASE_OPCODE(BINOP_ADD)

            PN_BEGIN_CASE_OPCODE(BINOP_SUB)
              PN_IF_TYPE(BINOP_SUB, DOUBLE)
              PN_IF_TYPE(BINOP_SUB, FLOAT)
              PN_IF_TYPE(BINOP_SUB, INT8)
              PN_IF_TYPE(BINOP_SUB, INT16)
              PN_IF_TYPE(BINOP_SUB, INT32)
              PN_IF_TYPE(BINOP_SUB, INT64)
            PN_END_CASE_OPCODE(BINOP_SUB)

            PN_BEGIN_CASE_OPCODE(BINOP_MUL)
              PN_IF_TYPE(BINOP_MUL, DOUBLE)
              PN_IF_TYPE(BINOP_MUL, FLOAT)
              PN_IF_TYPE(BINOP_MUL, INT8)
              PN_IF_TYPE(BINOP_MUL, INT16)
              PN_IF_TYPE(BINOP_MUL, INT32)
              PN_IF_TYPE(BINOP_MUL, INT64)
            PN_END_CASE_OPCODE(BINOP_MUL)

            PN_BEGIN_CASE_OPCODE(BINOP_UDIV)
              PN_IF_TYPE(BINOP_UDIV, INT8)
              PN_IF_TYPE(BINOP_UDIV, INT16)
              PN_IF_TYPE(BINOP_UDIV, INT32)
              PN_IF_TYPE(BINOP_UDIV, INT64)
            PN_END_CASE_OPCODE(BINOP_UDIV)

            PN_BEGIN_CASE_OPCODE(BINOP_SDIV)
              PN_IF_TYPE(BINOP_SDIV, DOUBLE)
              PN_IF_TYPE(BINOP_SDIV, FLOAT)
              PN_IF_TYPE(BINOP_SDIV, INT32)
              PN_IF_TYPE(BINOP_SDIV, INT64)
            PN_END_CASE_OPCODE(BINOP_SDIV)

            PN_BEGIN_CASE_OPCODE(BINOP_UREM)
              PN_IF_TYPE(BINOP_UREM, INT8)
              PN_IF_TYPE(BINOP_UREM, INT16)
              PN_IF_TYPE(BINOP_UREM, INT32)
              PN_IF_TYPE(BINOP_UREM, INT64)
            PN_END_CASE_OPCODE(BINOP_UREM)

            PN_BEGIN_CASE_OPCODE(BINOP_SREM)
              PN_IF_TYPE(BINOP_SREM, INT32)
              PN_IF_TYPE(BINOP_SREM, INT64)
            PN_END_CASE_OPCODE(BINOP_SREM)

            PN_BEGIN_CASE_OPCODE(BINOP_SHL)
              PN_IF_TYPE(BINOP_SHL, INT8)
              PN_IF_TYPE(BINOP_SHL, INT16)
              PN_IF_TYPE(BINOP_SHL, INT32)
              PN_IF_TYPE(BINOP_SHL, INT64)
            PN_END_CASE_OPCODE(BINOP_SHL)

            PN_BEGIN_CASE_OPCODE(BINOP_LSHR)
              PN_IF_TYPE(BINOP_LSHR, INT8)
              PN_IF_TYPE(BINOP_LSHR, INT16)
              PN_IF_TYPE(BINOP_LSHR, INT32)
              PN_IF_TYPE(BINOP_LSHR, INT64)
            PN_END_CASE_OPCODE(BINOP_LSHR)

            PN_BEGIN_CASE_OPCODE(BINOP_ASHR)
              PN_IF_TYPE(BINOP_ASHR, INT8)
              PN_IF_TYPE(BINOP_ASHR, INT16)
              PN_IF_TYPE(BINOP_ASHR, INT32)
              PN_IF_TYPE(BINOP_ASHR, INT64)
            PN_END_CASE_OPCODE(BINOP_ASHR)

            PN_BEGIN_CASE_OPCODE(BINOP_AND)
              PN_IF_TYPE(BINOP_AND, INT1)
              PN_IF_TYPE(BINOP_AND, INT8)
              PN_IF_TYPE(BINOP_AND, INT16)
              PN_IF_TYPE(BINOP_AND, INT32)
              PN_IF_TYPE(BINOP_AND, INT64)
            PN_END_CASE_OPCODE(BINOP_AND)

            PN_BEGIN_CASE_OPCODE(BINOP_OR)
              PN_IF_TYPE(BINOP_OR, INT1)
              PN_IF_TYPE(BINOP_OR, INT8)
              PN_IF_TYPE(BINOP_OR, INT16)
              PN_IF_TYPE(BINOP_OR, INT32)
              PN_IF_TYPE(BINOP_OR, INT64)
            PN_END_CASE_OPCODE(BINOP_OR)

            PN_BEGIN_CASE_OPCODE(BINOP_XOR)
              PN_IF_TYPE(BINOP_XOR, INT1)
              PN_IF_TYPE(BINOP_XOR, INT8)
              PN_IF_TYPE(BINOP_XOR, INT16)
              PN_IF_TYPE(BINOP_XOR, INT32)
              PN_IF_TYPE(BINOP_XOR, INT64)
            PN_END_CASE_OPCODE(BINOP_XOR)
            // clang-format on
          }

          o->result_value_id = i->result_value_id;
          o->value0_id = i->value0_id;
          o->value1_id = i->value1_id;
        }
        offset += sizeof(PNRuntimeInstructionBinop);
        break;
      }

      case PN_FUNCTION_CODE_INST_CAST: {
        if (write) {
          PNInstructionCast* i = (PNInstructionCast*)inst;
          PNRuntimeInstructionCast* o = (PNRuntimeInstructionCast*)offset;

          PNValue* value = pn_function_get_value(module, function, i->value_id);
          PNType* type0 = pn_module_get_type(module, value->type_id);
          PNType* type1 = pn_module_get_type(module, i->type_id);
          PNBasicType basic_type0 = type0->basic_type;
          PNBasicType basic_type1 = type1->basic_type;

          switch (i->cast_opcode) {
            // clang-format off
            PN_BEGIN_CASE_OPCODE(CAST_TRUNC)
              PN_IF_TYPE2(CAST_TRUNC, INT8, INT1)
              PN_IF_TYPE2(CAST_TRUNC, INT16, INT1)
              PN_IF_TYPE2(CAST_TRUNC, INT16, INT8)
              PN_IF_TYPE2(CAST_TRUNC, INT32, INT1)
              PN_IF_TYPE2(CAST_TRUNC, INT32, INT8)
              PN_IF_TYPE2(CAST_TRUNC, INT32, INT16)
              PN_IF_TYPE2(CAST_TRUNC, INT64, INT8)
              PN_IF_TYPE2(CAST_TRUNC, INT64, INT16)
              PN_IF_TYPE2(CAST_TRUNC, INT64, INT32)
            PN_END_CASE_OPCODE2(CAST_TRUNC)

            PN_BEGIN_CASE_OPCODE(CAST_ZEXT)
              PN_IF_TYPE2(CAST_ZEXT, INT1, INT8)
              PN_IF_TYPE2(CAST_ZEXT, INT1, INT16)
              PN_IF_TYPE2(CAST_ZEXT, INT1, INT32)
              PN_IF_TYPE2(CAST_ZEXT, INT1, INT64)
              PN_IF_TYPE2(CAST_ZEXT, INT8, INT16)
              PN_IF_TYPE2(CAST_ZEXT, INT8, INT32)
              PN_IF_TYPE2(CAST_ZEXT, INT8, INT64)
              PN_IF_TYPE2(CAST_ZEXT, INT16, INT32)
              PN_IF_TYPE2(CAST_ZEXT, INT16, INT64)
              PN_IF_TYPE2(CAST_ZEXT, INT32, INT64)
            PN_END_CASE_OPCODE2(CAST_ZEXT)

            PN_BEGIN_CASE_OPCODE(CAST_SEXT)
              PN_IF_TYPE2(CAST_SEXT, INT1, INT8)
              PN_IF_TYPE2(CAST_SEXT, INT1, INT16)
              PN_IF_TYPE2(CAST_SEXT, INT1, INT32)
              PN_IF_TYPE2(CAST_SEXT, INT1, INT64)
              PN_IF_TYPE2(CAST_SEXT, INT8, INT16)
              PN_IF_TYPE2(CAST_SEXT, INT8, INT32)
              PN_IF_TYPE2(CAST_SEXT, INT8, INT64)
              PN_IF_TYPE2(CAST_SEXT, INT16, INT32)
              PN_IF_TYPE2(CAST_SEXT, INT16, INT64)
              PN_IF_TYPE2(CAST_SEXT, INT32, INT64)
            PN_END_CASE_OPCODE2(CAST_SEXT)

            PN_BEGIN_CASE_OPCODE(CAST_FPTOUI)
              PN_IF_TYPE2(CAST_FPTOUI, DOUBLE, INT8)
              PN_IF_TYPE2(CAST_FPTOUI, DOUBLE, INT16)
              PN_IF_TYPE2(CAST_FPTOUI, DOUBLE, INT32)
              PN_IF_TYPE2(CAST_FPTOUI, DOUBLE, INT64)
              PN_IF_TYPE2(CAST_FPTOUI, FLOAT, INT8)
              PN_IF_TYPE2(CAST_FPTOUI, FLOAT, INT16)
              PN_IF_TYPE2(CAST_FPTOUI, FLOAT, INT32)
              PN_IF_TYPE2(CAST_FPTOUI, FLOAT, INT64)
            PN_END_CASE_OPCODE2(CAST_FPTOUI)

            PN_BEGIN_CASE_OPCODE(CAST_FPTOSI)
              PN_IF_TYPE2(CAST_FPTOSI, DOUBLE, INT8)
              PN_IF_TYPE2(CAST_FPTOSI, DOUBLE, INT16)
              PN_IF_TYPE2(CAST_FPTOSI, DOUBLE, INT32)
              PN_IF_TYPE2(CAST_FPTOSI, DOUBLE, INT64)
              PN_IF_TYPE2(CAST_FPTOSI, FLOAT, INT8)
              PN_IF_TYPE2(CAST_FPTOSI, FLOAT, INT16)
              PN_IF_TYPE2(CAST_FPTOSI, FLOAT, INT32)
              PN_IF_TYPE2(CAST_FPTOSI, FLOAT, INT64)
            PN_END_CASE_OPCODE2(CAST_FPTOSI)

            PN_BEGIN_CASE_OPCODE(CAST_UITOFP)
              PN_IF_TYPE2(CAST_UITOFP, INT8, DOUBLE)
              PN_IF_TYPE2(CAST_UITOFP, INT8, FLOAT)
              PN_IF_TYPE2(CAST_UITOFP, INT16, DOUBLE)
              PN_IF_TYPE2(CAST_UITOFP, INT16, FLOAT)
              PN_IF_TYPE2(CAST_UITOFP, INT32, DOUBLE)
              PN_IF_TYPE2(CAST_UITOFP, INT32, FLOAT)
              PN_IF_TYPE2(CAST_UITOFP, INT64, DOUBLE)
              PN_IF_TYPE2(CAST_UITOFP, INT64, FLOAT)
            PN_END_CASE_OPCODE2(CAST_UITOFP)

            PN_BEGIN_CASE_OPCODE(CAST_SITOFP)
              PN_IF_TYPE2(CAST_SITOFP, INT8, DOUBLE)
              PN_IF_TYPE2(CAST_SITOFP, INT8, FLOAT)
              PN_IF_TYPE2(CAST_SITOFP, INT16, DOUBLE)
              PN_IF_TYPE2(CAST_SITOFP, INT16, FLOAT)
              PN_IF_TYPE2(CAST_SITOFP, INT32, DOUBLE)
              PN_IF_TYPE2(CAST_SITOFP, INT32, FLOAT)
              PN_IF_TYPE2(CAST_SITOFP, INT64, DOUBLE)
              PN_IF_TYPE2(CAST_SITOFP, INT64, FLOAT)
            PN_END_CASE_OPCODE2(CAST_SITOFP)

            PN_BEGIN_CASE_OPCODE(CAST_FPTRUNC)
              PN_IF_TYPE2(CAST_FPTRUNC, DOUBLE, FLOAT)
            PN_END_CASE_OPCODE2(CAST_FPTRUNC)

            PN_BEGIN_CASE_OPCODE(CAST_FPEXT)
              PN_IF_TYPE2(CAST_FPEXT, FLOAT, DOUBLE)
            PN_END_CASE_OPCODE2(CAST_FPEXT)

            PN_BEGIN_CASE_OPCODE(CAST_BITCAST)
              PN_IF_TYPE2(CAST_BITCAST, DOUBLE, INT64)
              PN_IF_TYPE2(CAST_BITCAST, FLOAT, INT32)
              PN_IF_TYPE2(CAST_BITCAST, INT32, FLOAT)
              PN_IF_TYPE2(CAST_BITCAST, INT64, DOUBLE)
            PN_END_CASE_OPCODE2(CAST_BITCAST)
            // clang-format on
          }

          o->result_value_id = i->result_value_id;
          o->value_id = i->value_id;
        }
        offset += sizeof(PNRuntimeInstructionCast);
        break;
      }

      case PN_FUNCTION_CODE_INST_RET: {
        PNInstructionRet* i = (PNInstructionRet*)inst;
        if (i->value_id != PN_INVALID_VALUE_ID) {
          if (write) {
            PNRuntimeInstructionRetValue* o =
                (PNRuntimeInstructionRetValue*)offset;
            o->base.opcode = PN_OPCODE_RET_VALUE;
            o->value_id = i->value_id;
          }
          offset += sizeof(PNRuntimeInstructionRetValue);
        } else {
          if (write) {
            PNRuntimeInstructionRet* o = (PNRuntimeInstructionRet*)offset;
            o->base.opcode = PN_OPCODE_RET;
          }
          offset += sizeof(PNRuntimeInstructionRet);
        }
        break;
      }

      case PN_FUNCTION_CODE_INST_BR: {
        PNInstructionBr* i = (PNInstructionBr*)inst;
        if (i->value_id != PN_INVALID_VALUE_ID) {
          if (write) {
            PNRuntimeInstructionBrInt1* o = (PNRuntimeInstructionBrInt1*)offset;

            PNValue* value =
                pn_function_get_value(module, function, i->value_id);
            PNBasicType basic_type0 =
                pn_module_get_type(module, value->type_id)->basic_type;

            PN_IF_TYPE(BR, INT1)
            PN_END_IF_TYPE(BR)

            o->value_id = i->value_id;
            o->true_inst = bb_offsets[i->true_bb_id];
            o->false_inst = bb_offsets[i->false_bb_id];
          }
          offset += sizeof(PNRuntimeInstructionBrInt1);
        } else {
          if (write) {
            PNRuntimeInstructionBr* o = (PNRuntimeInstructionBr*)offset;
            o->base.opcode = PN_OPCODE_BR;
            o->inst = bb_offsets[i->true_bb_id];
          }
          offset += sizeof(PNRuntimeInstructionBr);
        }
        write_phi_assigns = PN_TRUE;
        break;
      }

      case PN_FUNCTION_CODE_INST_SWITCH: {
        PNInstructionSwitch* i = (PNInstructionSwitch*)inst;
        if (write) {
          PNRuntimeInstructionSwitch* o = (PNRuntimeInstructionSwitch*)offset;

          PNValue* value = pn_function_get_value(module, function, i->value_id);
          PNBasicType basic_type0 =
              pn_module_get_type(module, value->type_id)->basic_type;

          PN_IF_TYPE(SWITCH, INT1)
          PN_IF_TYPE(SWITCH, INT8)
          PN_IF_TYPE(SWITCH, INT16)
          PN_IF_TYPE(SWITCH, INT32)
          PN_IF_TYPE(SWITCH, INT64)
          PN_END_IF_TYPE(SWITCH)

          o->value_id = i->value_id;
          o->default_inst = bb_offsets[i->default_bb_id];
          o->num_cases = i->num_cases;
        }
        offset += sizeof(PNRuntimeInstructionSwitch);
        uint32_t c;
        for (c = 0; c < i->num_cases; ++c) {
          if (write) {
            PNRuntimeSwitchCase* switch_case =(PNRuntimeSwitchCase*)offset;
            switch_case->value = i->cases[c].value;
            switch_case->inst = bb_offsets[i->cases[c].bb_id];
          }
          offset += sizeof(PNRuntimeSwitchCase);
        }
        write_phi_assigns = PN_TRUE;
        break;
      }

      case PN_FUNCTION_CODE_INST_UNREACHABLE: {
        if (write) {
          PNRuntimeInstructionUnreachable* o =
              (PNRuntimeInstructionUnreachable*)offset;

          o->base.opcode = PN_OPCODE_UNREACHABLE;
        }
        offset += sizeof(PNRuntimeInstructionUnreachable);
        break;
      }

      case PN_FUNCTION_CODE_INST_ALLOCA: {
        if (write) {
          PNInstructionAlloca* i = (PNInstructionAlloca*)inst;
          PNRuntimeInstructionAlloca* o = (PNRuntimeInstructionAlloca*)offset;

          PNValue* value = pn_function_get_value(module, function, i->size_id);
          PNBasicType basic_type0 =
              pn_module_get_type(module, value->type_id)->basic_type;

          PN_IF_TYPE(ALLOCA, INT32)
          PN_END_IF_TYPE(ALLOCA)

          o->result_value_id = i->result_value_id;
          o->size_id = i->size_id;
          o->alignment = i->alignment;
        }
        offset += sizeof(PNRuntimeInstructionAlloca);
        break;
      }

      case PN_FUNCTION_CODE_INST_LOAD: {
        if (write) {
          PNInstructionLoad* i = (PNInstructionLoad*)inst;
          PNRuntimeInstructionLoad* o = (PNRuntimeInstructionLoad*)offset;

          PNBasicType basic_type0 =
              pn_module_get_type(module, i->type_id)->basic_type;

          PNValue* src = pn_function_get_value(module, function, i->src_id);
          PNBasicType src_basic_type =
              pn_module_get_type(module, src->type_id)->basic_type;
          if (src_basic_type != PN_BASIC_TYPE_INT32) {
            PN_FATAL(
                "Expected load src to have the int32 basic type, not %d.\n",
                src_basic_type);
          }

          PN_IF_TYPE(LOAD, DOUBLE)
          PN_IF_TYPE(LOAD, FLOAT)
          PN_IF_TYPE(LOAD, INT8)
          PN_IF_TYPE(LOAD, INT16)
          PN_IF_TYPE(LOAD, INT32)
          PN_IF_TYPE(LOAD, INT64)
          PN_END_IF_TYPE(LOAD)

          o->result_value_id = i->result_value_id;
          o->src_id = i->src_id;
          o->alignment = i->alignment;
        }
        offset += sizeof(PNRuntimeInstructionLoad);
        break;
      }

      case PN_FUNCTION_CODE_INST_STORE: {
        if (write) {
          PNInstructionStore* i = (PNInstructionStore*)inst;
          PNRuntimeInstructionStore* o = (PNRuntimeInstructionStore*)offset;

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

          PN_IF_TYPE(STORE, DOUBLE)
          PN_IF_TYPE(STORE, FLOAT)
          PN_IF_TYPE(STORE, INT8)
          PN_IF_TYPE(STORE, INT16)
          PN_IF_TYPE(STORE, INT32)
          PN_IF_TYPE(STORE, INT64)
          PN_END_IF_TYPE(STORE)

          o->dest_id = i->dest_id;
          o->value_id = i->value_id;
          o->alignment = i->alignment;
        }
        offset += sizeof(PNRuntimeInstructionStore);
        break;
      }

      case PN_FUNCTION_CODE_INST_CMP2: {
        if (write) {
          PNInstructionCmp2* i = (PNInstructionCmp2*)inst;
          PNRuntimeInstructionCmp2* o = (PNRuntimeInstructionCmp2*)offset;

          PNValue* value0 =
              pn_function_get_value(module, function, i->value0_id);
          PNValue* value1 =
              pn_function_get_value(module, function, i->value1_id);
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
            // clang-format off
            PN_BEGIN_CASE_OPCODE(FCMP_FALSE)
            PN_END_CASE_OPCODE(FCMP_FALSE)

            PN_BEGIN_CASE_OPCODE(FCMP_OEQ)
              PN_IF_TYPE(FCMP_OEQ, DOUBLE)
              PN_IF_TYPE(FCMP_OEQ, FLOAT)
            PN_END_CASE_OPCODE(FCMP_OEQ)

            PN_BEGIN_CASE_OPCODE(FCMP_OGT)
              PN_IF_TYPE(FCMP_OGT, DOUBLE)
              PN_IF_TYPE(FCMP_OGT, FLOAT)
            PN_END_CASE_OPCODE(FCMP_OGT)

            PN_BEGIN_CASE_OPCODE(FCMP_OGE)
              PN_IF_TYPE(FCMP_OGE, DOUBLE)
              PN_IF_TYPE(FCMP_OGE, FLOAT)
            PN_END_CASE_OPCODE(FCMP_OGE)

            PN_BEGIN_CASE_OPCODE(FCMP_OLT)
              PN_IF_TYPE(FCMP_OLT, DOUBLE)
              PN_IF_TYPE(FCMP_OLT, FLOAT)
            PN_END_CASE_OPCODE(FCMP_OLT)

            PN_BEGIN_CASE_OPCODE(FCMP_OLE)
              PN_IF_TYPE(FCMP_OLE, DOUBLE)
              PN_IF_TYPE(FCMP_OLE, FLOAT)
            PN_END_CASE_OPCODE(FCMP_OLE)

            PN_BEGIN_CASE_OPCODE(FCMP_ONE)
              PN_IF_TYPE(FCMP_ONE, DOUBLE)
              PN_IF_TYPE(FCMP_ONE, FLOAT)
            PN_END_CASE_OPCODE(FCMP_ONE)

            PN_BEGIN_CASE_OPCODE(FCMP_ORD)
              PN_IF_TYPE(FCMP_ORD, DOUBLE)
              PN_IF_TYPE(FCMP_ORD, FLOAT)
            PN_END_CASE_OPCODE(FCMP_ORD)

            PN_BEGIN_CASE_OPCODE(FCMP_UNO)
              PN_IF_TYPE(FCMP_UNO, DOUBLE)
              PN_IF_TYPE(FCMP_UNO, FLOAT)
            PN_END_CASE_OPCODE(FCMP_UNO)

            PN_BEGIN_CASE_OPCODE(FCMP_UEQ)
              PN_IF_TYPE(FCMP_UEQ, DOUBLE)
              PN_IF_TYPE(FCMP_UEQ, FLOAT)
            PN_END_CASE_OPCODE(FCMP_UEQ)

            PN_BEGIN_CASE_OPCODE(FCMP_UGT)
              PN_IF_TYPE(FCMP_UGT, DOUBLE)
              PN_IF_TYPE(FCMP_UGT, FLOAT)
            PN_END_CASE_OPCODE(FCMP_UGT)

            PN_BEGIN_CASE_OPCODE(FCMP_UGE)
              PN_IF_TYPE(FCMP_UGE, DOUBLE)
              PN_IF_TYPE(FCMP_UGE, FLOAT)
            PN_END_CASE_OPCODE(FCMP_UGE)

            PN_BEGIN_CASE_OPCODE(FCMP_ULT)
              PN_IF_TYPE(FCMP_ULT, DOUBLE)
              PN_IF_TYPE(FCMP_ULT, FLOAT)
            PN_END_CASE_OPCODE(FCMP_ULT)

            PN_BEGIN_CASE_OPCODE(FCMP_ULE)
              PN_IF_TYPE(FCMP_ULE, DOUBLE)
              PN_IF_TYPE(FCMP_ULE, FLOAT)
            PN_END_CASE_OPCODE(FCMP_ULE)

            PN_BEGIN_CASE_OPCODE(FCMP_UNE)
              PN_IF_TYPE(FCMP_UNE, DOUBLE)
              PN_IF_TYPE(FCMP_UNE, FLOAT)
            PN_END_CASE_OPCODE(FCMP_UNE)

            PN_BEGIN_CASE_OPCODE(FCMP_TRUE)
            PN_END_CASE_OPCODE(FCMP_TRUE)

            PN_BEGIN_CASE_OPCODE(ICMP_EQ)
              PN_IF_TYPE(ICMP_EQ, INT8)
              PN_IF_TYPE(ICMP_EQ, INT16)
              PN_IF_TYPE(ICMP_EQ, INT32)
              PN_IF_TYPE(ICMP_EQ, INT64)
            PN_END_CASE_OPCODE(ICMP_EQ)

            PN_BEGIN_CASE_OPCODE(ICMP_NE)
              PN_IF_TYPE(ICMP_NE, INT8)
              PN_IF_TYPE(ICMP_NE, INT16)
              PN_IF_TYPE(ICMP_NE, INT32)
              PN_IF_TYPE(ICMP_NE, INT64)
            PN_END_CASE_OPCODE(ICMP_NE)

            PN_BEGIN_CASE_OPCODE(ICMP_UGT)
              PN_IF_TYPE(ICMP_UGT, INT8)
              PN_IF_TYPE(ICMP_UGT, INT16)
              PN_IF_TYPE(ICMP_UGT, INT32)
              PN_IF_TYPE(ICMP_UGT, INT64)
            PN_END_CASE_OPCODE(ICMP_UGT)

            PN_BEGIN_CASE_OPCODE(ICMP_UGE)
              PN_IF_TYPE(ICMP_UGE, INT8)
              PN_IF_TYPE(ICMP_UGE, INT16)
              PN_IF_TYPE(ICMP_UGE, INT32)
              PN_IF_TYPE(ICMP_UGE, INT64)
            PN_END_CASE_OPCODE(ICMP_UGE)

            PN_BEGIN_CASE_OPCODE(ICMP_ULT)
              PN_IF_TYPE(ICMP_ULT, INT8)
              PN_IF_TYPE(ICMP_ULT, INT16)
              PN_IF_TYPE(ICMP_ULT, INT32)
              PN_IF_TYPE(ICMP_ULT, INT64)
            PN_END_CASE_OPCODE(ICMP_ULT)

            PN_BEGIN_CASE_OPCODE(ICMP_ULE)
              PN_IF_TYPE(ICMP_ULE, INT8)
              PN_IF_TYPE(ICMP_ULE, INT16)
              PN_IF_TYPE(ICMP_ULE, INT32)
              PN_IF_TYPE(ICMP_ULE, INT64)
            PN_END_CASE_OPCODE(ICMP_ULE)

            PN_BEGIN_CASE_OPCODE(ICMP_SGT)
              PN_IF_TYPE(ICMP_SGT, INT8)
              PN_IF_TYPE(ICMP_SGT, INT16)
              PN_IF_TYPE(ICMP_SGT, INT32)
              PN_IF_TYPE(ICMP_SGT, INT64)
            PN_END_CASE_OPCODE(ICMP_SGT)

            PN_BEGIN_CASE_OPCODE(ICMP_SGE)
              PN_IF_TYPE(ICMP_SGE, INT8)
              PN_IF_TYPE(ICMP_SGE, INT16)
              PN_IF_TYPE(ICMP_SGE, INT32)
              PN_IF_TYPE(ICMP_SGE, INT64)
            PN_END_CASE_OPCODE(ICMP_SGE)

            PN_BEGIN_CASE_OPCODE(ICMP_SLT)
              PN_IF_TYPE(ICMP_SLT, INT8)
              PN_IF_TYPE(ICMP_SLT, INT16)
              PN_IF_TYPE(ICMP_SLT, INT32)
              PN_IF_TYPE(ICMP_SLT, INT64)
            PN_END_CASE_OPCODE(ICMP_SLT)

            PN_BEGIN_CASE_OPCODE(ICMP_SLE)
              PN_IF_TYPE(ICMP_SLE, INT8)
              PN_IF_TYPE(ICMP_SLE, INT16)
              PN_IF_TYPE(ICMP_SLE, INT32)
              PN_IF_TYPE(ICMP_SLE, INT64)
            PN_END_CASE_OPCODE(ICMP_SLE)
            // clang-format on
          }

          o->result_value_id = i->result_value_id;
          o->value0_id = i->value0_id;
          o->value1_id = i->value1_id;
        }
        offset += sizeof(PNRuntimeInstructionCmp2);
        break;
      }

      case PN_FUNCTION_CODE_INST_VSELECT: {
        if (write) {
          PNInstructionVselect* i = (PNInstructionVselect*)inst;
          PNRuntimeInstructionVselect* o = (PNRuntimeInstructionVselect*)offset;

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
                "Expected vselect to have the same basic type for true and "
                "false "
                "branches. Got %d and %d\n",
                basic_type0, basic_type1);
          }

          o->base.opcode = PN_OPCODE_VSELECT;
          o->result_value_id = i->result_value_id;
          o->cond_id = i->cond_id;
          o->true_value_id = i->true_value_id;
          o->false_value_id = i->false_value_id;
        }
        offset += sizeof(PNRuntimeInstructionVselect);
        break;
      }

      case PN_FUNCTION_CODE_INST_CALL:
      case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
        PNInstructionCall* i = (PNInstructionCall*)inst;
        if (write) {
          PNRuntimeInstructionCall* o = (PNRuntimeInstructionCall*)offset;

          if (!i->is_indirect) {
            PNValue* callee =
                pn_function_get_value(module, function, i->callee_id);
            assert(callee->code == PN_VALUE_CODE_FUNCTION);
            PNFunction* callee_function =
                pn_module_get_function(module, callee->index);
            if (callee_function->intrinsic_id != PN_INTRINSIC_NULL) {
              switch (callee_function->intrinsic_id) {
#define PN_INTRINSIC_CHECK(e, name)           \
  case PN_INTRINSIC_##e:                      \
    o->base.opcode = PN_OPCODE_INTRINSIC_##e; \
    break;
              PN_FOREACH_INTRINSIC(PN_INTRINSIC_CHECK)
#undef PN_INTRINSIC_CHECK
                default:
                  o->base.opcode = PN_OPCODE_CALL;
                  break;
              }
            } else {
              o->base.opcode = PN_OPCODE_CALL;
            }

            /* Specialize some intrinsics based on constant args */
            switch (o->base.opcode) {
              case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I8:
              case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I16:
              case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I32:
              case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I64: {
                uint32_t type_offset =
                    o->base.opcode -
                    PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I8;

                PN_CHECK(i->num_args == 4);
                PNValue* opcode =
                    pn_function_get_value(module, function, i->arg_ids[0]);
                PN_CHECK(opcode->code == PN_VALUE_CODE_CONSTANT);
                PNConstant* op =
                    pn_function_get_constant(function, opcode->index);
                PN_CHECK(op->basic_type == PN_BASIC_TYPE_INT32);
                switch (op->value.u32) {
                  case 1:
                    o->base.opcode =
                        PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I8 +
                        type_offset;
                    break;
                  case 2:
                    o->base.opcode =
                        PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I8 +
                        type_offset;
                    break;
                  case 3:
                    o->base.opcode =
                        PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I8 +
                        type_offset;
                    break;
                  case 4:
                    o->base.opcode =
                        PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I8 +
                        type_offset;
                    break;
                  case 5:
                    o->base.opcode =
                        PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I8 +
                        type_offset;
                    break;
                  case 6:
                    o->base.opcode =
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
          } else {
            /* indirect function call */
            o->base.opcode = PN_OPCODE_CALL;
          }

          /* TODO(binji): check arg types against function type? */
          PNType* return_type = pn_module_get_type(module, i->return_type_id);
          o->result_value_id = i->result_value_id;
          o->callee_id = i->callee_id;
          o->num_args = i->num_args;
          o->flags = (i->is_indirect ? PN_CALL_FLAGS_INDIRECT : 0);
          o->flags |= (i->is_tail_call ? PN_CALL_FLAGS_TAIL_CALL : 0);
          o->flags |= (return_type->code == PN_TYPE_CODE_VOID
                           ? PN_CALL_FLAGS_RETURN_TYPE_VOID
                           : 0);
        }
        offset += sizeof(PNRuntimeInstructionCall);
        uint32_t a;
        for (a = 0; a < i->num_args; ++a) {
          if (write) {
            *(PNValueId*)offset = i->arg_ids[a];
          }
          offset += sizeof(PNValueId);
        }
        break;
      }

      case PN_FUNCTION_CODE_INST_PHI:
      case PN_FUNCTION_CODE_INST_FORWARDTYPEREF:
        break;

      default:
        PN_FATAL("Invalid instruction code: %d\n", inst->code);
        break;
    }
  }
  if (write_phi_assigns) {
    if (write) {
      *(uint32_t*)offset = bb->num_phi_assigns;
    }
    offset += sizeof(uint32_t);
    uint32_t n;
    for (n = 0; n < bb->num_phi_assigns; ++n) {
      if (write) {
        PNRuntimePhiAssign* o = (PNRuntimePhiAssign*)offset;
        o->inst = bb_offsets[bb->phi_assigns[n].bb_id];
        o->source_value_id = bb->phi_assigns[n].source_value_id;
        o->dest_value_id = bb->phi_assigns[n].dest_value_id;
      }
      offset += sizeof(PNRuntimePhiAssign);
    }
  }
  return offset;

#undef PN_BEGIN_CASE_OPCODE
#undef PN_IF_TYPE
#undef PN_IF_TYPE2
#undef PN_END_CASE_OPCODE
#undef PN_END_CASE_OPCODE2
}

static void pn_function_calculate_opcodes(PNModule* module,
                                          PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_OPCODES);
  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);

  void** bb_offsets = pn_allocator_alloc(&module->temp_allocator,
                                         function->num_bbs * sizeof(void*),
                                         sizeof(PNBasicBlockId));

  void* offset = 0;
  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    /* Always align basic blocks to 4 bytes. */
    offset = pn_align_up_pointer(offset, 4);
    bb_offsets[n] = offset;
    offset = pn_basic_block_write_instruction_stream(
        module, function, &function->bbs[n], NULL, offset, PN_FALSE);
  }
  uint32_t istream_size = (uint32_t)(uintptr_t)offset;
  function->instructions = pn_allocator_alloc(&module->instruction_allocator,
                                              istream_size, PN_DEFAULT_ALIGN);

  for (n = 0; n < function->num_bbs; ++n) {
    bb_offsets[n] = function->instructions + (size_t)bb_offsets[n];
  }

  offset = function->instructions;
  for (n = 0; n < function->num_bbs; ++n) {
    offset = pn_align_up_pointer(offset, 4);
    PN_CHECK(offset == bb_offsets[n]);
    offset = pn_basic_block_write_instruction_stream(
        module, function, &function->bbs[n], bb_offsets, offset, PN_TRUE);
  }

  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
  PN_END_TIME(CALCULATE_OPCODES);
}

#endif /* PN_CALCULATE_OPCODES_H_ */
