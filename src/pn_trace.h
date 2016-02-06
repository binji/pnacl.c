/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_TRACE_H_
#define PN_TRACE_H_

#if PN_TRACING
static void pn_string_concat(PNAllocator* allocator,
                             char** dest,
                             uint32_t* dest_len,
                             const char* src,
                             uint32_t src_len) {
  if (src_len == 0) {
    src_len = strlen(src);
  }

  uint32_t old_dest_len = *dest_len;
  *dest_len += src_len;
  pn_allocator_realloc_add(allocator, (void**)dest, src_len, 1);
  memcpy(*dest + old_dest_len - 1, src, src_len);
  (*dest)[*dest_len - 1] = 0;
}

static const char* pn_type_describe(PNModule* module, PNTypeId type_id);

static const char* pn_type_describe_all(PNModule* module,
                                        PNTypeId type_id,
                                        const char* name,
                                        PNBool with_param_names) {
  if (type_id == PN_INVALID_TYPE_ID) {
    return "<invalid>";
  }

  PNType* type = &module->types[type_id];
  switch (type->code) {
    case PN_TYPE_CODE_VOID:
      return "void";

    case PN_TYPE_CODE_INTEGER:
      switch (type->width) {
        case 1:
          return "i1";
        case 8:
          return "i8";
        case 16:
          return "i16";
        case 32:
          return "i32";
        case 64:
          return "i64";
        default:
          PN_FATAL("Integer with bad width: %d\n", type->width);
          return "badInteger";
      }
    case PN_TYPE_CODE_FLOAT:
      return "float";

    case PN_TYPE_CODE_DOUBLE:
      return "double";

    case PN_TYPE_CODE_FUNCTION: {
      char* buffer = pn_allocator_alloc(&module->temp_allocator, 1, 1);
      uint32_t buffer_len = 1;
      buffer[0] = 0;

      pn_string_concat(&module->temp_allocator, &buffer, &buffer_len,
                       pn_type_describe(module, type->return_type), 0);
      pn_string_concat(&module->temp_allocator, &buffer, &buffer_len, " ", 1);
      if (name) {
        pn_string_concat(&module->temp_allocator, &buffer, &buffer_len, name,
                         strlen(name));
      }
      pn_string_concat(&module->temp_allocator, &buffer, &buffer_len, "(", 1);
      uint32_t n;
      for (n = 0; n < type->num_args; ++n) {
        if (n != 0) {
          pn_string_concat(&module->temp_allocator, &buffer, &buffer_len, ", ",
                           2);
        }

        pn_string_concat(&module->temp_allocator, &buffer, &buffer_len,
                         pn_type_describe(module, type->arg_types[n]), 0);

        if (with_param_names) {
          char param_name[14];
          snprintf(param_name, sizeof(param_name), " %%p%d", n);
          pn_string_concat(&module->temp_allocator, &buffer, &buffer_len,
                           param_name, strlen(param_name));
        }
      }
      pn_string_concat(&module->temp_allocator, &buffer, &buffer_len, ")", 1);
      return buffer;
    }

    default:
      return "<unknown>";
  }
}

static const char* pn_type_describe(PNModule* module, PNTypeId type_id) {
  PNType* type = &module->types[type_id];
  // clang-format off
  switch (type->code) {
    case PN_TYPE_CODE_VOID: return "void";
    case PN_TYPE_CODE_INTEGER:
      switch (type->width) {
        case 1: return "i1";
        case 8: return "i8";
        case 16: return "i16";
        case 32: return "i32";
        case 64: return "i64";
        default:
          PN_UNREACHABLE();
          break;
      }
    case PN_TYPE_CODE_FLOAT: return "float";
    case PN_TYPE_CODE_DOUBLE: return "double";
    case PN_TYPE_CODE_FUNCTION: return "i32";
    default:
      PN_UNREACHABLE();
      break;
  }
  // clang-format on
  return "<unknown>";
}

static void pn_value_print_to_string(PNModule* module,
                                     PNFunction* function,
                                     PNValueId value_id,
                                     char* buffer,
                                     size_t buffer_size) {
  PNValue dummy_value;
  PNValue* value;
  if (value_id >= module->num_values) {
    if (function) {
      value = pn_function_get_value(module, function, value_id);
    } else {
      /* This should only happen when we're trying to resolve relocation values
       * when parsing the globalvar block. In that case, there are really only
       * two possibilities; this value is a function, or this value is a
       * global. We can determine which is which because we know how many
       * functions there are. Anything greater than that is a global value. */
      value = &dummy_value;
      if (value_id >= module->num_functions) {
        value->code = PN_VALUE_CODE_GLOBAL_VAR;
      } else {
        value->code = PN_VALUE_CODE_FUNCTION;
      }
    }
  } else {
    value = &module->values[value_id];
  }

  uint32_t base;
  char sigil;
  char code;
  switch (value->code) {
    case PN_VALUE_CODE_FUNCTION:
      sigil = '@';
      code = 'f';
      base = 0;
      break;
    case PN_VALUE_CODE_GLOBAL_VAR:
      sigil = '@';
      code = 'g';
      base = module->num_functions;
      break;
    case PN_VALUE_CODE_CONSTANT:
      sigil = '%';
      code = 'c';
      base = module->num_values + function->num_args;
      break;
    case PN_VALUE_CODE_FUNCTION_ARG:
      sigil = '%';
      code = 'p';
      base = module->num_values;
      break;
    case PN_VALUE_CODE_LOCAL_VAR:
      sigil = '%';
      code = 'v';
      base = module->num_values + function->num_args + function->num_constants;
      break;
    default:
      PN_UNREACHABLE();
      break;
  }

  snprintf(buffer, buffer_size, "%c%c%d", sigil, code, value_id - base);
}

static const char* pn_value_describe(PNModule* module,
                                     PNFunction* function,
                                     PNValueId value_id) {
#define PN_MAX_BUFFER_INDEX 16
#define PN_BUFFER_SIZE 13
  static uint32_t buffer_index = 0;
  static char buffers[PN_MAX_BUFFER_INDEX][PN_BUFFER_SIZE];
  char* buffer = &buffers[buffer_index++ & (PN_MAX_BUFFER_INDEX - 1)][0];
  pn_value_print_to_string(module, function, value_id, buffer, PN_BUFFER_SIZE);
#undef PN_MAX_BUFFER_INDEX
#undef PN_BUFFER_SIZE
  return buffer;
}

static const char* pn_value_describe_type(PNModule* module,
                                          PNFunction* function,
                                          PNValueId value_id) {
  assert(function);
  PNValue* value = pn_function_get_value(module, function, value_id);
  return pn_type_describe(module, value->type_id);
}

static const char* pn_binop_get_name(uint32_t op) {
  const char* names[] = {"add", "sub",  "mul",  "udiv", "sdiv", "urem", "srem",
                         "shl", "lshr", "ashr", "and",  "or",   "xor"};

  if (op >= PN_ARRAY_SIZE(names)) {
    PN_FATAL("Invalid op: %u\n", op);
  }

  return names[op];
}

static const char* pn_binop_get_name_float(uint32_t op) {
  const char* names[] = {"fadd", "fsub", "fmul", NULL, "fdiv", NULL, NULL,
                         NULL,   NULL,   NULL,   NULL, NULL,   NULL};

  if (op >= PN_ARRAY_SIZE(names)) {
    PN_FATAL("Invalid op: %u\n", op);
  }

  const char* name = names[op];
  if (!name) {
    PN_FATAL("Invalid float binop: %u\n", op);
  }

  return name;
}

static const char* pn_cast_get_name(uint32_t op) {
  const char* names[] = {"trunc",  "zext",   "sext",   "fptoui",
                         "fptosi", "uitofp", "sitofp", "fptrunc",
                         "fpext",  NULL,     NULL,     "bitcast"};

  if (op >= PN_ARRAY_SIZE(names)) {
    PN_FATAL("Invalid op: %u\n", op);
  }

  return names[op];
}

static const char* pn_cmp2_get_name(uint32_t op) {
  const char* names[] = {
      "fcmp false", "fcmp oeq", "fcmp ogt", "fcmp oge",  "fcmp olt", "fcmp ole",
      "fcmp one",   "fcmp ord", "fcmp uno", "fcmp ueq",  "fcmp ugt", "fcmp uge",
      "fcmp ult",   "fcmp ule", "fcmp une", "fcmp true", NULL,       NULL,
      NULL,         NULL,       NULL,       NULL,        NULL,       NULL,
      NULL,         NULL,       NULL,       NULL,        NULL,       NULL,
      NULL,         NULL,       "icmp eq",  "icmp ne",   "icmp ugt", "icmp uge",
      "icmp ult",   "icmp ule", "icmp sgt", "icmp sge",  "icmp slt", "icmp sle",
  };

  if (op >= PN_ARRAY_SIZE(names)) {
    PN_FATAL("Invalid op: %u\n", op);
  }

  return names[op];
}

#if PN_CALCULATE_LIVENESS
static const char* pn_liveness_range_describe(PNModule* module,
                                              PNFunction* function,
                                              PNValueId value_id) {
  if (PN_IS_TRACE(BASIC_BLOCK_EXTRAS)) {
    const uint32_t BUFFER_SIZE = 30;
    static char buffer[BUFFER_SIZE];
    PNValueId rel_id = value_id - module->num_values;
    PNLivenessRange* range = &function->value_liveness_range[rel_id];
    if (range->first_bb_id != PN_INVALID_BB_ID &&
        range->last_bb_id != PN_INVALID_BB_ID) {
      if (range->first_bb_id == range->last_bb_id) {
        snprintf(buffer, BUFFER_SIZE, "  live: [%%b%d], %%v%d",
                 range->first_bb_id, range->slot_id);
      } else {
        snprintf(buffer, BUFFER_SIZE, "  live: [%%b%d..%%b%d], %%v%d",
                 range->first_bb_id, range->last_bb_id, range->slot_id);
      }
      return buffer;
    }
  }
  return "";
}
#else
static const char* pn_liveness_range_describe(PNModule* module,
                                              PNFunction* function,
                                              PNValueId value_id) {
  return "";
}
#endif /* PN_CALCULATE_LIVENESS */

static void pn_instruction_trace(PNModule* module,
                                 PNFunction* function,
                                 PNInstruction* inst,
                                 PNBool force) {
  if (!(PN_IS_TRACE(INSTRUCTIONS) || force)) {
    return;
  }

  PN_TRACE_PRINT_INDENT();
  switch (inst->code) {
    case PN_FUNCTION_CODE_INST_BINOP: {
      PNInstructionBinop* i = (PNInstructionBinop*)inst;
      PNValue* result_value =
          pn_function_get_value(module, function, i->result_value_id);
      PNType* result_type = &module->types[result_value->type_id];
      PNBasicType result_basic_type = result_type->basic_type;
      PN_PRINT(
          "%s = %s %s %s, %s;%s\n",
          pn_value_describe(module, function, i->result_value_id),
          result_basic_type < PN_BASIC_TYPE_FLOAT
              ? pn_binop_get_name(i->binop_opcode)
              : pn_binop_get_name_float(i->binop_opcode),
          pn_value_describe_type(module, function, i->result_value_id),
          pn_value_describe(module, function, i->value0_id),
          pn_value_describe(module, function, i->value1_id),
          pn_liveness_range_describe(module, function, i->result_value_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_CAST: {
      PNInstructionCast* i = (PNInstructionCast*)inst;
      PN_PRINT(
          "%s = %s %s %s to %s;%s\n",
          pn_value_describe(module, function, i->result_value_id),
          pn_cast_get_name(i->cast_opcode),
          pn_value_describe_type(module, function, i->value_id),
          pn_value_describe(module, function, i->value_id),
          pn_value_describe_type(module, function, i->result_value_id),
          pn_liveness_range_describe(module, function, i->result_value_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_RET: {
      PNInstructionRet* i = (PNInstructionRet*)inst;
      if (i->value_id != PN_INVALID_VALUE_ID) {
        PN_PRINT("ret %s %s;\n",
                 pn_value_describe_type(module, function, i->value_id),
                 pn_value_describe(module, function, i->value_id));
      } else {
        PN_PRINT("ret void;\n");
      }
      break;
    }

    case PN_FUNCTION_CODE_INST_BR: {
      PNInstructionBr* i = (PNInstructionBr*)inst;
      if (i->false_bb_id != PN_INVALID_BB_ID) {
        PN_PRINT("br %s %s, label %%b%d, label %%b%d;\n",
                 pn_value_describe_type(module, function, i->value_id),
                 pn_value_describe(module, function, i->value_id),
                 i->true_bb_id, i->false_bb_id);
      } else {
        PN_PRINT("br label %%b%d;\n", i->true_bb_id);
      }
      break;
    }

    case PN_FUNCTION_CODE_INST_SWITCH: {
      PNInstructionSwitch* i = (PNInstructionSwitch*)inst;
      const char* type_str =
          pn_value_describe_type(module, function, i->value_id);
      PN_PRINT("switch %s %s {\n", type_str,
               pn_value_describe(module, function, i->value_id));
      PN_TRACE_PRINT_INDENTX(2);
      PN_PRINT("default: br label %%b%d;\n", i->default_bb_id);

      uint32_t c;
      for (c = 0; c < i->num_cases; ++c) {
        PNSwitchCase* switch_case = &i->cases[c];
        PN_TRACE_PRINT_INDENTX(2);
        PN_PRINT("%s %" PRId64 ": br label %%b%d;\n", type_str,
                 switch_case->value, switch_case->bb_id);
      }
      PN_TRACE_PRINT_INDENT();
      PN_PRINT("}\n");
      break;
    }

    case PN_FUNCTION_CODE_INST_UNREACHABLE:
      PN_PRINT("unreachable;\n");
      break;

    case PN_FUNCTION_CODE_INST_PHI: {
      int32_t col = g_pn_trace_indent;
      PNInstructionPhi* i = (PNInstructionPhi*)inst;
      col += PN_PRINT(
          "%s = phi %s ",
          pn_value_describe(module, function, i->result_value_id),
          pn_value_describe_type(module, function, i->result_value_id));
      char buffer[100];
      int32_t n;
      for (n = 0; n < i->num_incoming; ++n) {
        if (n != 0) {
          col += PN_PRINT(", ");
        }
        int32_t len = snprintf(
            buffer, sizeof(buffer), "[%s, %%b%d]",
            pn_value_describe(module, function, i->incoming[n].value_id),
            i->incoming[n].bb_id);
        if (col + len + 2 > 80) { /* +2 for ", " */
          PN_PRINT("\n");
          PN_TRACE_PRINT_INDENTX(4);
          col = g_pn_trace_indent + 4;
        }
        col += PN_PRINT("%s", buffer);
      }
      PN_PRINT(";%s\n", pn_liveness_range_describe(module, function,
                                                   i->result_value_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_ALLOCA: {
      PNInstructionAlloca* i = (PNInstructionAlloca*)inst;
      PN_PRINT(
          "%s = alloca i8, %s %s, align %d;%s\n",
          pn_value_describe(module, function, i->result_value_id),
          pn_value_describe_type(module, function, i->size_id),
          pn_value_describe(module, function, i->size_id), i->alignment,
          pn_liveness_range_describe(module, function, i->result_value_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_LOAD: {
      PNInstructionLoad* i = (PNInstructionLoad*)inst;
      PN_PRINT(
          "%s = load %s* %s, align %d;%s\n",
          pn_value_describe(module, function, i->result_value_id),
          pn_value_describe_type(module, function, i->result_value_id),
          pn_value_describe(module, function, i->src_id), i->alignment,
          pn_liveness_range_describe(module, function, i->result_value_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_STORE: {
      PNInstructionStore* i = (PNInstructionStore*)inst;
      PN_PRINT("store %s %s, %s* %s, align %d;\n",
               pn_value_describe_type(module, function, i->value_id),
               pn_value_describe(module, function, i->value_id),
               pn_value_describe_type(module, function, i->value_id),
               pn_value_describe(module, function, i->dest_id), i->alignment);
      break;
    }

    case PN_FUNCTION_CODE_INST_CMP2: {
      PNInstructionCmp2* i = (PNInstructionCmp2*)inst;
      PN_PRINT(
          "%s = %s %s %s, %s;%s\n",
          pn_value_describe(module, function, i->result_value_id),
          pn_cmp2_get_name(i->cmp2_opcode),
          pn_value_describe_type(module, function, i->value0_id),
          pn_value_describe(module, function, i->value0_id),
          pn_value_describe(module, function, i->value1_id),
          pn_liveness_range_describe(module, function, i->result_value_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_VSELECT: {
      PNInstructionVselect* i = (PNInstructionVselect*)inst;
      PN_PRINT(
          "%s = select %s %s, %s %s, %s %s;%s\n",
          pn_value_describe(module, function, i->result_value_id),
          pn_value_describe_type(module, function, i->cond_id),
          pn_value_describe(module, function, i->cond_id),
          pn_value_describe_type(module, function, i->true_value_id),
          pn_value_describe(module, function, i->true_value_id),
          pn_value_describe_type(module, function, i->false_value_id),
          pn_value_describe(module, function, i->false_value_id),
          pn_liveness_range_describe(module, function, i->result_value_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_FORWARDTYPEREF: {
      PNInstructionForwardtyperef* i = (PNInstructionForwardtyperef*)inst;
      PN_PRINT("declare %s %s;\n", pn_type_describe(module, i->type_id),
               pn_value_describe(module, function, i->value_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_CALL:
    case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PNType* return_type = &module->types[i->return_type_id];
      PNBool is_return_type_void = return_type->code == PN_TYPE_CODE_VOID;
      if (!is_return_type_void) {
        PN_PRINT("%s = %scall %s ",
                 pn_value_describe(module, function, i->result_value_id),
                 i->is_tail_call ? "tail " : "",
                 pn_value_describe_type(module, function, i->result_value_id));
      } else {
        PN_PRINT("%scall void ", i->is_tail_call ? "tail " : "");
      }
      PN_PRINT("%s(", pn_value_describe(module, function, i->callee_id));

      int32_t n;
      for (n = 0; n < i->num_args; ++n) {
        if (n != 0) {
          PN_PRINT(", ");
        }
        PN_PRINT("%s %s",
                 pn_value_describe_type(module, function, i->arg_ids[n]),
                 pn_value_describe(module, function, i->arg_ids[n]));
      }
      PN_PRINT(");");
      if (!is_return_type_void) {
        PN_PRINT("%s", pn_liveness_range_describe(module, function,
                                                  i->result_value_id));
      }
      PN_PRINT("\n");
      break;
    }

    default:
      PN_FATAL("Invalid instruction code: %d\n", inst->code);
      break;
  }
}

static void pn_basic_block_trace(PNModule* module,
                                 PNFunction* function,
                                 PNBasicBlock* bb,
                                 PNBasicBlockId bb_id,
                                 PNBool force) {
  if (!(PN_IS_TRACE(BASIC_BLOCKS) || force)) {
    return;
  }

  PN_TRACE_PRINT_INDENTX(-2);
  PN_PRINT("%%b%d:\n", bb_id);

#define PN_PRINT_LINE_LIST(name, var_count, sep, ...) \
  if (var_count) {                                    \
    PN_TRACE_PRINT_INDENTX(-1);                       \
    PN_PRINT(#name ": ");                             \
    for (n = 0; n < var_count; ++n) {                 \
      PN_PRINT(__VA_ARGS__);                          \
      if (n != var_count - 1) {                       \
        PN_PRINT(sep);                                \
      }                                               \
    }                                                 \
    PN_PRINT(";\n");                                  \
  }

  if (PN_IS_TRACE(BASIC_BLOCK_EXTRAS)) {
    uint32_t n;
#if PN_CALCULATE_PRED_BBS
    PN_PRINT_LINE_LIST(preds, bb->num_pred_bbs, ", ", "%%b%d",
                       bb->pred_bb_ids[n]);
#endif /* PN_CALCULATE_PRED_BBS */
#if PN_CALCULATE_LIVENESS
    if (bb->first_def_id != PN_INVALID_VALUE_ID) {
      PN_TRACE_PRINT_INDENTX(-1);
      PN_PRINT("defs: [%s..%s];\n",
               pn_value_describe(module, function, bb->first_def_id),
               pn_value_describe(module, function, bb->last_def_id));
    }
    PN_PRINT_LINE_LIST(uses, bb->num_uses, ", ", "%s",
                       pn_value_describe(module, function, bb->uses[n]));
#endif /* PN_CALCULATE_LIVENESS */
#if PN_CALCULATE_LOOPS
    if (bb->loop_header_id != PN_INVALID_BB_ID) {
      PN_TRACE_PRINT_INDENTX(-1);
      PN_PRINT("loop header: %%b%d\n", bb->loop_header_id);
    }
    if (bb->is_loop_header || bb->is_irreducible || bb->is_reentry) {
      PN_TRACE_PRINT_INDENTX(-2);
      if (bb->is_loop_header) {
        PN_PRINT(" (is loop header)");
      }
      if (bb->is_irreducible) {
        PN_PRINT(" (is irreducible)");
      }
      if (bb->is_reentry) {
        PN_PRINT(" (is re-entry)");
      }
      PN_PRINT("\n");
    }
#endif /* PN_CALCULATE_LOOPS */
    PN_PRINT_LINE_LIST(
        phi uses, bb->num_phi_uses, ", ", "[%s, %%b%d]",
        pn_value_describe(module, function, bb->phi_uses[n].incoming.value_id),
        bb->phi_uses[n].incoming.bb_id);
#if PN_CALCULATE_PRED_BBS
    PN_PRINT_LINE_LIST(succs, bb->num_succ_bbs, ", ", "%%b%d",
                       bb->succ_bb_ids[n]);
#endif /* PN_CALCULATE_PRED_BBS */
  }

  PNInstruction* inst;
  for (inst = bb->instructions; inst; inst = inst->next) {
    pn_instruction_trace(module, function, inst, force);
  }

  if (PN_IS_TRACE(BASIC_BLOCK_EXTRAS)) {
    uint32_t n;
    PN_PRINT_LINE_LIST(
        phi assigns, bb->num_phi_assigns, "; ", "%%b%d, %s = %s",
        bb->phi_assigns[n].bb_id,
        pn_value_describe(module, function, bb->phi_assigns[n].dest_value_id),
        pn_value_describe(module, function,
                          bb->phi_assigns[n].source_value_id));
  }
#undef PN_PRINT_LINE_LIST
}

static void pn_function_print_header(PNModule* module,
                                     PNFunction* function,
                                     PNFunctionId function_id) {
  PN_PRINT("%*sfunction %s {  // BlockID = %d\n", g_pn_trace_indent, "",
           pn_type_describe_all(module, function->type_id,
                                pn_value_describe(module, NULL, function_id),
                                PN_TRUE),
           PN_BLOCKID_FUNCTION);
  g_pn_trace_indent += 2;
}

static void pn_function_trace(PNModule* module,
                              PNFunction* function,
                              PNFunctionId function_id) {
  PN_BEGIN_TIME(FUNCTION_TRACE);

  PNBool force = PN_FALSE;
  if (g_pn_trace_function_filter && g_pn_trace_function_filter[0] != 0) {
    char first = g_pn_trace_function_filter[0];
    if (first >= '0' && first <= '9') {
      /* Filter based on function id */
      PNFunctionId filter_id = atoi(g_pn_trace_function_filter);
      if (filter_id != function_id) {
        return;
      }
    } else {
      /* Filter based on function name */
      if (strcmp(function->name, g_pn_trace_function_filter) != 0) {
        return;
      }
    }
    pn_function_print_header(module, function, function_id);
    force = PN_TRUE;
  }

  if (!(PN_IS_TRACE(FUNCTION_BLOCK) || force)) {
    return;
  }

  uint32_t i;
  for (i = 0; i < function->num_bbs; ++i) {
    pn_basic_block_trace(module, function, &function->bbs[i], i, force);
  }
  if (force) {
    g_pn_trace_indent -= 2;
    PN_PRINT("%*s}\n", g_pn_trace_indent, "");
  }
  PN_END_TIME(FUNCTION_TRACE);
}

static void pn_runtime_instruction_trace(PNModule* module,
                                         PNFunction* function,
                                         PNRuntimeInstruction* inst) {
  PN_TRACE_PRINT_INDENT();
  const char* opname = NULL;

  switch (inst->opcode) {
    case PN_OPCODE_ALLOCA_INT32: {
      PNRuntimeInstructionAlloca* i = (PNRuntimeInstructionAlloca*)inst;
      PN_PRINT("%s = alloca i8, %s %s, align %d;\n",
               pn_value_describe(module, function, i->result_value_id),
               pn_value_describe_type(module, function, i->size_id),
               pn_value_describe(module, function, i->size_id), i->alignment);
      break;
    }

    // clang-format off
    case PN_OPCODE_BINOP_ADD_DOUBLE:
    case PN_OPCODE_BINOP_ADD_FLOAT:  opname = "fadd"; goto binop;
    case PN_OPCODE_BINOP_ADD_INT8:
    case PN_OPCODE_BINOP_ADD_INT16:
    case PN_OPCODE_BINOP_ADD_INT32:
    case PN_OPCODE_BINOP_ADD_INT64:  opname = "add"; goto binop;
    case PN_OPCODE_BINOP_AND_INT1:
    case PN_OPCODE_BINOP_AND_INT8:
    case PN_OPCODE_BINOP_AND_INT16:
    case PN_OPCODE_BINOP_AND_INT32:
    case PN_OPCODE_BINOP_AND_INT64:  opname = "and"; goto binop;
    case PN_OPCODE_BINOP_ASHR_INT8:
    case PN_OPCODE_BINOP_ASHR_INT16:
    case PN_OPCODE_BINOP_ASHR_INT32:
    case PN_OPCODE_BINOP_ASHR_INT64: opname = "ashr"; goto binop;
    case PN_OPCODE_BINOP_LSHR_INT8:
    case PN_OPCODE_BINOP_LSHR_INT16:
    case PN_OPCODE_BINOP_LSHR_INT32:
    case PN_OPCODE_BINOP_LSHR_INT64: opname = "lshr"; goto binop;
    case PN_OPCODE_BINOP_MUL_DOUBLE:
    case PN_OPCODE_BINOP_MUL_FLOAT:  opname = "fmul"; goto binop;
    case PN_OPCODE_BINOP_MUL_INT8:
    case PN_OPCODE_BINOP_MUL_INT16:
    case PN_OPCODE_BINOP_MUL_INT32:
    case PN_OPCODE_BINOP_MUL_INT64:  opname = "mul"; goto binop;
    case PN_OPCODE_BINOP_OR_INT1:
    case PN_OPCODE_BINOP_OR_INT8:
    case PN_OPCODE_BINOP_OR_INT16:
    case PN_OPCODE_BINOP_OR_INT32:
    case PN_OPCODE_BINOP_OR_INT64:   opname = "or"; goto binop;
    case PN_OPCODE_BINOP_SDIV_DOUBLE:
    case PN_OPCODE_BINOP_SDIV_FLOAT: opname = "fdiv"; goto binop;
    case PN_OPCODE_BINOP_SDIV_INT32:
    case PN_OPCODE_BINOP_SDIV_INT64: opname = "sdiv"; goto binop;
    case PN_OPCODE_BINOP_SHL_INT8:
    case PN_OPCODE_BINOP_SHL_INT16:
    case PN_OPCODE_BINOP_SHL_INT32:
    case PN_OPCODE_BINOP_SHL_INT64:  opname = "shl"; goto binop;
    case PN_OPCODE_BINOP_SREM_INT32:
    case PN_OPCODE_BINOP_SREM_INT64: opname = "srem"; goto binop;
    case PN_OPCODE_BINOP_SUB_DOUBLE:
    case PN_OPCODE_BINOP_SUB_FLOAT:  opname = "fsub"; goto binop;
    case PN_OPCODE_BINOP_SUB_INT8:
    case PN_OPCODE_BINOP_SUB_INT16:
    case PN_OPCODE_BINOP_SUB_INT32:
    case PN_OPCODE_BINOP_SUB_INT64:  opname = "sub"; goto binop;
    case PN_OPCODE_BINOP_UDIV_INT8:
    case PN_OPCODE_BINOP_UDIV_INT16:
    case PN_OPCODE_BINOP_UDIV_INT32:
    case PN_OPCODE_BINOP_UDIV_INT64: opname = "udiv"; goto binop;
    case PN_OPCODE_BINOP_UREM_INT8:
    case PN_OPCODE_BINOP_UREM_INT16:
    case PN_OPCODE_BINOP_UREM_INT32:
    case PN_OPCODE_BINOP_UREM_INT64: opname = "urem"; goto binop;
    case PN_OPCODE_BINOP_XOR_INT1:
    case PN_OPCODE_BINOP_XOR_INT8:
    case PN_OPCODE_BINOP_XOR_INT16:
    case PN_OPCODE_BINOP_XOR_INT32:
    case PN_OPCODE_BINOP_XOR_INT64:  opname = "xor"; goto binop;
    binop: {
      // clang-format on
      PNRuntimeInstructionBinop* i = (PNRuntimeInstructionBinop*)inst;
      PN_PRINT("%s = %s %s %s, %s;\n",
               pn_value_describe(module, function, i->result_value_id), opname,
               pn_value_describe_type(module, function, i->result_value_id),
               pn_value_describe(module, function, i->value0_id),
               pn_value_describe(module, function, i->value1_id));
      break;
    }

    case PN_OPCODE_BR: {
      PNRuntimeInstructionBr* i = (PNRuntimeInstructionBr*)inst;
      PN_PRINT("br label %%%zd;\n", i->inst - function->instructions);
      break;
    }

    case PN_OPCODE_BR_INT1: {
      PNRuntimeInstructionBrInt1* i = (PNRuntimeInstructionBrInt1*)inst;
      PN_PRINT("br %s %s, label %%%zd, label %%%zd;\n",
               pn_value_describe_type(module, function, i->value_id),
               pn_value_describe(module, function, i->value_id),
               i->true_inst - function->instructions,
               i->false_inst - function->instructions);
      break;
    }

    // Display all intrinsics as function calls.
#define PN_INTRINSIC_OPCODE(e, name) case PN_OPCODE_INTRINSIC_##e:
  PN_FOREACH_INTRINSIC(PN_INTRINSIC_OPCODE)
#undef PN_INTRINSIC_OPCODE

#define PN_ATOMIC_RMW_INTRINSIC_OPCODE(e) case PN_OPCODE_INTRINSIC_##e:
  PN_FOREACH_ATOMIC_RMW_INTRINSIC_OPCODE(PN_ATOMIC_RMW_INTRINSIC_OPCODE)
#undef PN_ATOMIC_RMW_INTRINSIC_OPCODE

    case PN_OPCODE_CALL: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);

      if (i->flags & PN_CALL_FLAGS_RETURN_TYPE_VOID) {
        PN_PRINT("%scall void ",
                 (i->flags & PN_CALL_FLAGS_TAIL_CALL) ? "tail " : "");
      } else {
        PN_PRINT("%s = %scall %s ",
                 pn_value_describe(module, function, i->result_value_id),
                 (i->flags & PN_CALL_FLAGS_TAIL_CALL) ? "tail " : "",
                 pn_value_describe_type(module, function, i->result_value_id));
      }
      PN_PRINT("%s(", pn_value_describe(module, function, i->callee_id));

      int32_t n;
      for (n = 0; n < i->num_args; ++n) {
        if (n != 0) {
          PN_PRINT(", ");
        }
        PN_PRINT("%s %s", pn_value_describe_type(module, function, arg_ids[n]),
                 pn_value_describe(module, function, arg_ids[n]));
      }
      PN_PRINT(");\n");
      break;
    }

    // clang-format off
    case PN_OPCODE_CAST_BITCAST_DOUBLE_INT64:
    case PN_OPCODE_CAST_BITCAST_FLOAT_INT32:
    case PN_OPCODE_CAST_BITCAST_INT32_FLOAT:
    case PN_OPCODE_CAST_BITCAST_INT64_DOUBLE: opname = "bitcast"; goto cast;
    case PN_OPCODE_CAST_FPEXT_FLOAT_DOUBLE:   opname = "fpext"; goto cast;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT8:
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT16:
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT32:
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT64:
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT8:
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT16:
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT32:
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT64:   opname = "fptosi"; goto cast;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT8:
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT16:
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT32:
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT64:
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT8:
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT16:
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT32:
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT64:   opname = "fptoui"; goto cast;
    case PN_OPCODE_CAST_FPTRUNC_DOUBLE_FLOAT: opname = "fptrunc"; goto cast;
    case PN_OPCODE_CAST_SEXT_INT1_INT8:
    case PN_OPCODE_CAST_SEXT_INT1_INT16:
    case PN_OPCODE_CAST_SEXT_INT1_INT32:
    case PN_OPCODE_CAST_SEXT_INT1_INT64:
    case PN_OPCODE_CAST_SEXT_INT8_INT16:
    case PN_OPCODE_CAST_SEXT_INT8_INT32:
    case PN_OPCODE_CAST_SEXT_INT8_INT64:
    case PN_OPCODE_CAST_SEXT_INT16_INT32:
    case PN_OPCODE_CAST_SEXT_INT16_INT64:
    case PN_OPCODE_CAST_SEXT_INT32_INT64:     opname = "sext"; goto cast;
    case PN_OPCODE_CAST_SITOFP_INT8_DOUBLE:
    case PN_OPCODE_CAST_SITOFP_INT8_FLOAT:
    case PN_OPCODE_CAST_SITOFP_INT16_DOUBLE:
    case PN_OPCODE_CAST_SITOFP_INT16_FLOAT:
    case PN_OPCODE_CAST_SITOFP_INT32_DOUBLE:
    case PN_OPCODE_CAST_SITOFP_INT32_FLOAT:
    case PN_OPCODE_CAST_SITOFP_INT64_DOUBLE:
    case PN_OPCODE_CAST_SITOFP_INT64_FLOAT:   opname = "sitofp"; goto cast;
    case PN_OPCODE_CAST_TRUNC_INT8_INT1:
    case PN_OPCODE_CAST_TRUNC_INT16_INT1:
    case PN_OPCODE_CAST_TRUNC_INT16_INT8:
    case PN_OPCODE_CAST_TRUNC_INT32_INT1:
    case PN_OPCODE_CAST_TRUNC_INT32_INT8:
    case PN_OPCODE_CAST_TRUNC_INT32_INT16:
    case PN_OPCODE_CAST_TRUNC_INT64_INT8:
    case PN_OPCODE_CAST_TRUNC_INT64_INT16:
    case PN_OPCODE_CAST_TRUNC_INT64_INT32:    opname = "trunc"; goto cast;
    case PN_OPCODE_CAST_UITOFP_INT8_DOUBLE:
    case PN_OPCODE_CAST_UITOFP_INT8_FLOAT:
    case PN_OPCODE_CAST_UITOFP_INT16_DOUBLE:
    case PN_OPCODE_CAST_UITOFP_INT16_FLOAT:
    case PN_OPCODE_CAST_UITOFP_INT32_DOUBLE:
    case PN_OPCODE_CAST_UITOFP_INT32_FLOAT:
    case PN_OPCODE_CAST_UITOFP_INT64_DOUBLE:
    case PN_OPCODE_CAST_UITOFP_INT64_FLOAT:   opname = "uitofp"; goto cast;
    case PN_OPCODE_CAST_ZEXT_INT1_INT8:
    case PN_OPCODE_CAST_ZEXT_INT1_INT16:
    case PN_OPCODE_CAST_ZEXT_INT1_INT32:
    case PN_OPCODE_CAST_ZEXT_INT1_INT64:
    case PN_OPCODE_CAST_ZEXT_INT8_INT16:
    case PN_OPCODE_CAST_ZEXT_INT8_INT32:
    case PN_OPCODE_CAST_ZEXT_INT8_INT64:
    case PN_OPCODE_CAST_ZEXT_INT16_INT32:
    case PN_OPCODE_CAST_ZEXT_INT16_INT64:
    case PN_OPCODE_CAST_ZEXT_INT32_INT64:     opname = "zext"; goto cast;
    cast: {
      // clang-format on
      PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;
      PN_PRINT("%s = %s %s %s to %s;\n",
               pn_value_describe(module, function, i->result_value_id), opname,
               pn_value_describe_type(module, function, i->value_id),
               pn_value_describe(module, function, i->value_id),
               pn_value_describe_type(module, function, i->result_value_id));
      break;
    }

    // clang-format off
    case PN_OPCODE_FCMP_OEQ_DOUBLE:
    case PN_OPCODE_FCMP_OEQ_FLOAT:  opname = "fcmp oeq"; goto cmp2;
    case PN_OPCODE_FCMP_OGE_DOUBLE:
    case PN_OPCODE_FCMP_OGE_FLOAT:  opname = "fcmp oge"; goto cmp2;
    case PN_OPCODE_FCMP_OGT_DOUBLE:
    case PN_OPCODE_FCMP_OGT_FLOAT:  opname = "fcmp ogt"; goto cmp2;
    case PN_OPCODE_FCMP_OLE_DOUBLE:
    case PN_OPCODE_FCMP_OLE_FLOAT:  opname = "fcmp ogt"; goto cmp2;
    case PN_OPCODE_FCMP_OLT_DOUBLE:
    case PN_OPCODE_FCMP_OLT_FLOAT:  opname = "fcmp olt"; goto cmp2;
    case PN_OPCODE_FCMP_ONE_DOUBLE:
    case PN_OPCODE_FCMP_ONE_FLOAT:  opname = "fcmp one"; goto cmp2;
    case PN_OPCODE_FCMP_ORD_DOUBLE:
    case PN_OPCODE_FCMP_ORD_FLOAT:  opname = "fcmp ord"; goto cmp2;
    case PN_OPCODE_FCMP_UEQ_DOUBLE:
    case PN_OPCODE_FCMP_UEQ_FLOAT:  opname = "fcmp ueq"; goto cmp2;
    case PN_OPCODE_FCMP_UGE_DOUBLE:
    case PN_OPCODE_FCMP_UGE_FLOAT:  opname = "fcmp uge"; goto cmp2;
    case PN_OPCODE_FCMP_UGT_DOUBLE:
    case PN_OPCODE_FCMP_UGT_FLOAT:  opname = "fcmp ugt"; goto cmp2;
    case PN_OPCODE_FCMP_ULE_DOUBLE:
    case PN_OPCODE_FCMP_ULE_FLOAT:  opname = "fcmp ule"; goto cmp2;
    case PN_OPCODE_FCMP_ULT_DOUBLE:
    case PN_OPCODE_FCMP_ULT_FLOAT:  opname = "fcmp ult"; goto cmp2;
    case PN_OPCODE_FCMP_UNE_DOUBLE:
    case PN_OPCODE_FCMP_UNE_FLOAT:  opname = "fcmp une"; goto cmp2;
    case PN_OPCODE_FCMP_UNO_DOUBLE:
    case PN_OPCODE_FCMP_UNO_FLOAT:  opname = "fcmp uno"; goto cmp2;
    case PN_OPCODE_ICMP_EQ_INT8:
    case PN_OPCODE_ICMP_EQ_INT16:
    case PN_OPCODE_ICMP_EQ_INT32:
    case PN_OPCODE_ICMP_EQ_INT64:  opname = "icmp eq"; goto cmp2;
    case PN_OPCODE_ICMP_NE_INT8:
    case PN_OPCODE_ICMP_NE_INT16:
    case PN_OPCODE_ICMP_NE_INT32:
    case PN_OPCODE_ICMP_NE_INT64:  opname = "icmp ne"; goto cmp2;
    case PN_OPCODE_ICMP_SGE_INT8:
    case PN_OPCODE_ICMP_SGE_INT16:
    case PN_OPCODE_ICMP_SGE_INT32:
    case PN_OPCODE_ICMP_SGE_INT64:  opname = "icmp sge"; goto cmp2;
    case PN_OPCODE_ICMP_SGT_INT8:
    case PN_OPCODE_ICMP_SGT_INT16:
    case PN_OPCODE_ICMP_SGT_INT32:
    case PN_OPCODE_ICMP_SGT_INT64:  opname = "icmp sgt"; goto cmp2;
    case PN_OPCODE_ICMP_SLE_INT8:
    case PN_OPCODE_ICMP_SLE_INT16:
    case PN_OPCODE_ICMP_SLE_INT32:
    case PN_OPCODE_ICMP_SLE_INT64:  opname = "icmp sle"; goto cmp2;
    case PN_OPCODE_ICMP_SLT_INT8:
    case PN_OPCODE_ICMP_SLT_INT16:
    case PN_OPCODE_ICMP_SLT_INT32:
    case PN_OPCODE_ICMP_SLT_INT64:  opname = "icmp slt"; goto cmp2;
    case PN_OPCODE_ICMP_UGE_INT8:
    case PN_OPCODE_ICMP_UGE_INT16:
    case PN_OPCODE_ICMP_UGE_INT32:
    case PN_OPCODE_ICMP_UGE_INT64:  opname = "icmp uge"; goto cmp2;
    case PN_OPCODE_ICMP_UGT_INT8:
    case PN_OPCODE_ICMP_UGT_INT16:
    case PN_OPCODE_ICMP_UGT_INT32:
    case PN_OPCODE_ICMP_UGT_INT64:  opname = "icmp ugt"; goto cmp2;
    case PN_OPCODE_ICMP_ULE_INT8:
    case PN_OPCODE_ICMP_ULE_INT16:
    case PN_OPCODE_ICMP_ULE_INT32:
    case PN_OPCODE_ICMP_ULE_INT64:  opname = "icmp ule"; goto cmp2;
    case PN_OPCODE_ICMP_ULT_INT8:
    case PN_OPCODE_ICMP_ULT_INT16:
    case PN_OPCODE_ICMP_ULT_INT32:
    case PN_OPCODE_ICMP_ULT_INT64:  opname = "icmp ult"; goto cmp2;
    cmp2: {
      // clang-format on
      PNRuntimeInstructionCmp2* i = (PNRuntimeInstructionCmp2*)inst;
      PN_PRINT("%s = %s %s %s, %s;\n",
               pn_value_describe(module, function, i->result_value_id), opname,
               pn_value_describe_type(module, function, i->value0_id),
               pn_value_describe(module, function, i->value0_id),
               pn_value_describe(module, function, i->value1_id));
      break;
    }

    case PN_OPCODE_LOAD_DOUBLE:
    case PN_OPCODE_LOAD_FLOAT:
    case PN_OPCODE_LOAD_INT8:
    case PN_OPCODE_LOAD_INT16:
    case PN_OPCODE_LOAD_INT32:
    case PN_OPCODE_LOAD_INT64: {
      PNRuntimeInstructionLoad* i = (PNRuntimeInstructionLoad*)inst;
      PN_PRINT("%s = load %s* %s, align %d;\n",
               pn_value_describe(module, function, i->result_value_id),
               pn_value_describe_type(module, function, i->result_value_id),
               pn_value_describe(module, function, i->src_id), i->alignment);
      break;
    }

    case PN_OPCODE_RET: {
      PN_PRINT("ret void;\n");
      break;
    }

    case PN_OPCODE_RET_VALUE: {
      PNRuntimeInstructionRetValue* i = (PNRuntimeInstructionRetValue*)inst;
      PN_PRINT("ret %s %s;\n",
               pn_value_describe_type(module, function, i->value_id),
               pn_value_describe(module, function, i->value_id));
      break;
    }

    case PN_OPCODE_STORE_DOUBLE:
    case PN_OPCODE_STORE_FLOAT:
    case PN_OPCODE_STORE_INT8:
    case PN_OPCODE_STORE_INT16:
    case PN_OPCODE_STORE_INT32:
    case PN_OPCODE_STORE_INT64: {
      PNRuntimeInstructionStore* i = (PNRuntimeInstructionStore*)inst;
      PN_PRINT("store %s %s, %s* %s, align %d;\n",
               pn_value_describe_type(module, function, i->value_id),
               pn_value_describe(module, function, i->value_id),
               pn_value_describe_type(module, function, i->value_id),
               pn_value_describe(module, function, i->dest_id), i->alignment);
      break;
    }

    case PN_OPCODE_SWITCH_INT1:
    case PN_OPCODE_SWITCH_INT8:
    case PN_OPCODE_SWITCH_INT16:
    case PN_OPCODE_SWITCH_INT32:
    case PN_OPCODE_SWITCH_INT64: {
      PNRuntimeInstructionSwitch* i = (PNRuntimeInstructionSwitch*)inst;
      PNRuntimeSwitchCase* cases =
          (void*)inst + sizeof(PNRuntimeInstructionSwitch);
      const char* type_str =
          pn_value_describe_type(module, function, i->value_id);
      PN_PRINT("switch %s %s {\n", type_str,
               pn_value_describe(module, function, i->value_id));
      PN_TRACE_PRINT_INDENTX(2);
      PN_PRINT("default: br label %%%zd;\n",
               i->default_inst - function->instructions);

      uint32_t c;
      for (c = 0; c < i->num_cases; ++c) {
        PNRuntimeSwitchCase* switch_case = &cases[c];
        PN_TRACE_PRINT_INDENTX(2);
        PN_PRINT("%s %" PRId64 ": br label %%%zd;\n", type_str,
                 switch_case->value,
                 switch_case->inst - function->instructions);
      }
      PN_TRACE_PRINT_INDENT();
      PN_PRINT("}\n");
      break;
    }

    case PN_OPCODE_UNREACHABLE:
      PN_PRINT("unreachable;\n");
      break;

    case PN_OPCODE_VSELECT: {
      PNRuntimeInstructionVselect* i = (PNRuntimeInstructionVselect*)inst;
      PN_PRINT("%s = select %s %s, %s %s, %s %s;\n",
               pn_value_describe(module, function, i->result_value_id),
               pn_value_describe_type(module, function, i->cond_id),
               pn_value_describe(module, function, i->cond_id),
               pn_value_describe_type(module, function, i->true_value_id),
               pn_value_describe(module, function, i->true_value_id),
               pn_value_describe_type(module, function, i->false_value_id),
               pn_value_describe(module, function, i->false_value_id));
      break;
    }

    default:
      PN_FATAL("Invalid runtime instruction opcode: %d\n", inst->opcode);
      break;
  }
}

static void pn_executor_value_trace(PNExecutor* executor,
                                    PNFunction* function,
                                    PNValueId value_id,
                                    PNRuntimeValue value,
                                    const char* prefix,
                                    const char* postfix) {
  if (PN_IS_TRACE(EXECUTE)) {
    PNModule* module = executor->module;
    PNValue* val = pn_function_get_value(module, function, value_id);
    PNTypeId type_id = val->type_id;

    PNType* type = &executor->module->types[type_id];
    switch (type->basic_type) {
      case PN_BASIC_TYPE_INT1:
        PN_PRINT("%s%s = %u%s", prefix,
                 pn_value_describe(module, function, value_id), value.u8,
                 postfix);
        break;
      case PN_BASIC_TYPE_INT8:
        PN_PRINT("%s%s = %u%s", prefix,
                 pn_value_describe(module, function, value_id), value.u8,
                 postfix);
        break;
      case PN_BASIC_TYPE_INT16:
        PN_PRINT("%s%s = %u%s", prefix,
                 pn_value_describe(module, function, value_id), value.u16,
                 postfix);
        break;
      case PN_BASIC_TYPE_INT32:
        PN_PRINT("%s%s = %u%s", prefix,
                 pn_value_describe(module, function, value_id), value.u32,
                 postfix);
        break;
      case PN_BASIC_TYPE_INT64:
        PN_PRINT("%s%s = %" PRIu64 "%s", prefix,
                 pn_value_describe(module, function, value_id), value.u64,
                 postfix);
        break;
      case PN_BASIC_TYPE_FLOAT:
        PN_PRINT("%s%s = %f%s", prefix,
                 pn_value_describe(module, function, value_id), value.f32,
                 postfix);
        break;
      case PN_BASIC_TYPE_DOUBLE:
        PN_PRINT("%s%s = %f%s", prefix,
                 pn_value_describe(module, function, value_id), value.f64,
                 postfix);
        break;
      default:
        PN_UNREACHABLE();
        break;
    }
  }
}

static PNRuntimeValue pn_thread_get_value(PNThread* thread, PNValueId value_id);
static PNRuntimeValue pn_executor_get_value_from_frame(PNExecutor* executor,
                                                       PNCallFrame* frame,
                                                       PNValueId value_id);

static void pn_basic_block_trace_phi_assigns(PNThread* thread,
                                             PNFunction* old_function,
                                             PNRuntimeInstruction* inst) {
  PNModule* module = thread->module;
  PNRuntimeInstruction* dest_inst = thread->inst;
  void* istream = inst;
  uint16_t num_phi_assigns = *(uint16_t*)istream;
  istream += sizeof(uint16_t) + sizeof(uint16_t);
  PNRuntimePhiAssign* phi_assigns = (PNRuntimePhiAssign*)istream;
  istream += num_phi_assigns * sizeof(PNRuntimePhiAssign);

  uint32_t i;
  for (i = 0; i < num_phi_assigns; ++i) {
    PNRuntimePhiAssign* assign = &phi_assigns[i];
    if (assign->inst == dest_inst) {
      PN_TRACE(
          EXECUTE, "    %s <= %s\n",
          pn_value_describe(module, old_function, assign->dest_value_id),
          pn_value_describe(module, old_function, assign->source_value_id));
    }
  }
}

static void pn_runtime_instruction_trace_values(PNThread* thread,
                                                PNFunction* old_function,
                                                PNCallFrame* old_frame,
                                                PNRuntimeInstruction* inst) {
#define PN_VALUE(id, ty) \
  pn_value_describe(module, function, id), pn_thread_get_value(thread, id).ty
#define PN_VALUE_OLD(id, ty)               \
  pn_value_describe(module, old_function, id), \
      pn_executor_get_value_from_frame(thread->executor, old_frame, id).ty

  PNModule* module = thread->module;
  PNFunction* function = thread->function;

  switch (inst->opcode) {
    case PN_OPCODE_ALLOCA_INT32: {
      PNRuntimeInstructionAlloca* i = (PNRuntimeInstructionAlloca*)inst;
      PN_TRACE(EXECUTE, "    %s = %u  %s = %d\n",
               PN_VALUE(i->result_value_id, u32), PN_VALUE(i->size_id, u32));
      break;
    }

#define PN_OPCODE_BINOP(ty)                                                \
  do {                                                                     \
    PNRuntimeInstructionBinop* i = (PNRuntimeInstructionBinop*)inst;       \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty "  %s = " PN_FORMAT_##ty  \
             "  %s = " PN_FORMAT_##ty "\n",                                \
             PN_VALUE(i->result_value_id, ty), PN_VALUE(i->value0_id, ty), \
             PN_VALUE(i->value1_id, ty));                                  \
  } while (0) /* no semicolon */

    case PN_OPCODE_BINOP_ADD_DOUBLE:
    case PN_OPCODE_BINOP_MUL_DOUBLE:
    case PN_OPCODE_BINOP_SDIV_DOUBLE:
    case PN_OPCODE_BINOP_SUB_DOUBLE:
      PN_OPCODE_BINOP(f64);
      break;

    case PN_OPCODE_BINOP_ADD_FLOAT:
    case PN_OPCODE_BINOP_MUL_FLOAT:
    case PN_OPCODE_BINOP_SDIV_FLOAT:
    case PN_OPCODE_BINOP_SUB_FLOAT:
      PN_OPCODE_BINOP(f32);
      break;

    case PN_OPCODE_BINOP_AND_INT1:
    case PN_OPCODE_BINOP_OR_INT1:
    case PN_OPCODE_BINOP_XOR_INT1:
    case PN_OPCODE_BINOP_ADD_INT8:
    case PN_OPCODE_BINOP_AND_INT8:
    case PN_OPCODE_BINOP_ASHR_INT8:
    case PN_OPCODE_BINOP_LSHR_INT8:
    case PN_OPCODE_BINOP_MUL_INT8:
    case PN_OPCODE_BINOP_OR_INT8:
    case PN_OPCODE_BINOP_SHL_INT8:
    case PN_OPCODE_BINOP_SUB_INT8:
    case PN_OPCODE_BINOP_UDIV_INT8:
    case PN_OPCODE_BINOP_UREM_INT8:
    case PN_OPCODE_BINOP_XOR_INT8:
      PN_OPCODE_BINOP(u8);
      break;

    case PN_OPCODE_BINOP_ADD_INT16:
    case PN_OPCODE_BINOP_AND_INT16:
    case PN_OPCODE_BINOP_ASHR_INT16:
    case PN_OPCODE_BINOP_LSHR_INT16:
    case PN_OPCODE_BINOP_MUL_INT16:
    case PN_OPCODE_BINOP_OR_INT16:
    case PN_OPCODE_BINOP_SHL_INT16:
    case PN_OPCODE_BINOP_SUB_INT16:
    case PN_OPCODE_BINOP_UDIV_INT16:
    case PN_OPCODE_BINOP_UREM_INT16:
    case PN_OPCODE_BINOP_XOR_INT16:
      PN_OPCODE_BINOP(u16);
      break;

    case PN_OPCODE_BINOP_ADD_INT32:
    case PN_OPCODE_BINOP_AND_INT32:
    case PN_OPCODE_BINOP_ASHR_INT32:
    case PN_OPCODE_BINOP_LSHR_INT32:
    case PN_OPCODE_BINOP_MUL_INT32:
    case PN_OPCODE_BINOP_OR_INT32:
    case PN_OPCODE_BINOP_SDIV_INT32:
    case PN_OPCODE_BINOP_SHL_INT32:
    case PN_OPCODE_BINOP_SREM_INT32:
    case PN_OPCODE_BINOP_SUB_INT32:
    case PN_OPCODE_BINOP_UDIV_INT32:
    case PN_OPCODE_BINOP_UREM_INT32:
    case PN_OPCODE_BINOP_XOR_INT32:
      PN_OPCODE_BINOP(u32);
      break;

    case PN_OPCODE_BINOP_ADD_INT64:
    case PN_OPCODE_BINOP_AND_INT64:
    case PN_OPCODE_BINOP_ASHR_INT64:
    case PN_OPCODE_BINOP_LSHR_INT64:
    case PN_OPCODE_BINOP_MUL_INT64:
    case PN_OPCODE_BINOP_OR_INT64:
    case PN_OPCODE_BINOP_SDIV_INT64:
    case PN_OPCODE_BINOP_SHL_INT64:
    case PN_OPCODE_BINOP_SREM_INT64:
    case PN_OPCODE_BINOP_SUB_INT64:
    case PN_OPCODE_BINOP_UDIV_INT64:
    case PN_OPCODE_BINOP_UREM_INT64:
    case PN_OPCODE_BINOP_XOR_INT64:
      PN_OPCODE_BINOP(u64);
      break;

#undef PN_OPCODE_BINOP

    case PN_OPCODE_BR:
      pn_basic_block_trace_phi_assigns(
          thread, old_function, (void*)inst + sizeof(PNRuntimeInstructionBr));
      PN_TRACE(EXECUTE, "pc = %%%zd\n", thread->inst - function->instructions);
      break;

    case PN_OPCODE_BR_INT1: {
      PNRuntimeInstructionBrInt1* i = (PNRuntimeInstructionBrInt1*)inst;
      pn_basic_block_trace_phi_assigns(
          thread, old_function,
          (void*)inst + sizeof(PNRuntimeInstructionBrInt1));
      PN_TRACE(EXECUTE, "    %s = %u\n", PN_VALUE(i->value_id, u8));
      PN_TRACE(EXECUTE, "pc = %%%zd\n", thread->inst - function->instructions);
      break;
    }

    case PN_OPCODE_CALL: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);

      if (i->flags & PN_CALL_FLAGS_INDIRECT) {
        PN_TRACE(EXECUTE, "    %s = %u ", PN_VALUE_OLD(i->callee_id, u32));
      } else {
        PN_TRACE(EXECUTE, "    ");
      }

      uint32_t n;
      for (n = 0; n < i->num_args; ++n) {
        PNRuntimeValue arg = pn_executor_get_value_from_frame(
            thread->executor, old_frame, arg_ids[n]);

        pn_executor_value_trace(thread->executor, old_function, arg_ids[n], arg,
                                "", "  ");
      }

      if (function != old_function) {
        PN_TRACE(EXECUTE, "\nfunction = %%f%d  pc = %%0\n",
                 thread->current_frame->location.function_id);
      } else {
        PN_TRACE(EXECUTE, "\n");
      }
      break;
    }

#define PN_OPCODE_CAST(from, to)                                             \
  do {                                                                       \
    PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;           \
    PN_TRACE(EXECUTE,                                                        \
             "    %s = " PN_FORMAT_##to "  %s = " PN_FORMAT_##from "\n",     \
             PN_VALUE(i->result_value_id, to), PN_VALUE(i->value_id, from)); \
  } while (0) /* no semicolon */

    // clang-format off
    case PN_OPCODE_CAST_SEXT_INT1_INT8:
    case PN_OPCODE_CAST_TRUNC_INT8_INT1:      PN_OPCODE_CAST(i8, i8); break;
    case PN_OPCODE_CAST_SEXT_INT1_INT16:
    case PN_OPCODE_CAST_SEXT_INT8_INT16:      PN_OPCODE_CAST(i8, i16); break;
    case PN_OPCODE_CAST_SEXT_INT1_INT32:
    case PN_OPCODE_CAST_SEXT_INT8_INT32:      PN_OPCODE_CAST(i8, i32); break;
    case PN_OPCODE_CAST_SEXT_INT1_INT64:
    case PN_OPCODE_CAST_SEXT_INT8_INT64:      PN_OPCODE_CAST(i8, i64); break;
    case PN_OPCODE_CAST_SITOFP_INT8_FLOAT:    PN_OPCODE_CAST(i8, f32); break;
    case PN_OPCODE_CAST_SITOFP_INT8_DOUBLE:   PN_OPCODE_CAST(i8, f64); break;
    case PN_OPCODE_CAST_TRUNC_INT16_INT1:
    case PN_OPCODE_CAST_TRUNC_INT16_INT8:     PN_OPCODE_CAST(i16, i8); break;
    case PN_OPCODE_CAST_SEXT_INT16_INT32:     PN_OPCODE_CAST(i16, i32); break;
    case PN_OPCODE_CAST_SEXT_INT16_INT64:     PN_OPCODE_CAST(i16, i64); break;
    case PN_OPCODE_CAST_SITOFP_INT16_FLOAT:   PN_OPCODE_CAST(i16, f32); break;
    case PN_OPCODE_CAST_SITOFP_INT16_DOUBLE:  PN_OPCODE_CAST(i16, f64); break;
    case PN_OPCODE_CAST_TRUNC_INT32_INT1:
    case PN_OPCODE_CAST_TRUNC_INT32_INT8:     PN_OPCODE_CAST(i32, i8); break;
    case PN_OPCODE_CAST_TRUNC_INT32_INT16:    PN_OPCODE_CAST(i32, i16); break;
    case PN_OPCODE_CAST_SEXT_INT32_INT64:     PN_OPCODE_CAST(i32, i64); break;
    case PN_OPCODE_CAST_BITCAST_INT32_FLOAT:
    case PN_OPCODE_CAST_SITOFP_INT32_FLOAT:   PN_OPCODE_CAST(i32, f32); break;
    case PN_OPCODE_CAST_SITOFP_INT32_DOUBLE:  PN_OPCODE_CAST(i32, f64); break;
    case PN_OPCODE_CAST_TRUNC_INT64_INT8:     PN_OPCODE_CAST(i64, i8); break;
    case PN_OPCODE_CAST_TRUNC_INT64_INT16:    PN_OPCODE_CAST(i64, i16); break;
    case PN_OPCODE_CAST_TRUNC_INT64_INT32:    PN_OPCODE_CAST(i64, i32); break;
    case PN_OPCODE_CAST_SITOFP_INT64_FLOAT:   PN_OPCODE_CAST(i64, f32); break;
    case PN_OPCODE_CAST_BITCAST_INT64_DOUBLE:
    case PN_OPCODE_CAST_SITOFP_INT64_DOUBLE:  PN_OPCODE_CAST(i64, f64); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT8:       PN_OPCODE_CAST(u8, u8); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT16:      PN_OPCODE_CAST(u8, u16); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT32:      PN_OPCODE_CAST(u8, u32); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT64:      PN_OPCODE_CAST(u8, u64); break;
    case PN_OPCODE_CAST_ZEXT_INT8_INT16:      PN_OPCODE_CAST(u8, u16); break;
    case PN_OPCODE_CAST_ZEXT_INT8_INT32:      PN_OPCODE_CAST(u8, u32); break;
    case PN_OPCODE_CAST_ZEXT_INT8_INT64:      PN_OPCODE_CAST(u8, u64); break;
    case PN_OPCODE_CAST_UITOFP_INT8_FLOAT:    PN_OPCODE_CAST(u8, f32); break;
    case PN_OPCODE_CAST_UITOFP_INT8_DOUBLE:   PN_OPCODE_CAST(u8, f64); break;
    case PN_OPCODE_CAST_ZEXT_INT16_INT32:     PN_OPCODE_CAST(u16, u32); break;
    case PN_OPCODE_CAST_ZEXT_INT16_INT64:     PN_OPCODE_CAST(u16, u64); break;
    case PN_OPCODE_CAST_UITOFP_INT16_FLOAT:   PN_OPCODE_CAST(u16, f32); break;
    case PN_OPCODE_CAST_UITOFP_INT16_DOUBLE:  PN_OPCODE_CAST(u16, f64); break;
    case PN_OPCODE_CAST_ZEXT_INT32_INT64:     PN_OPCODE_CAST(u32, u64); break;
    case PN_OPCODE_CAST_UITOFP_INT32_FLOAT:   PN_OPCODE_CAST(u32, f32); break;
    case PN_OPCODE_CAST_UITOFP_INT32_DOUBLE:  PN_OPCODE_CAST(u32, f64); break;
    case PN_OPCODE_CAST_UITOFP_INT64_FLOAT:   PN_OPCODE_CAST(u64, f32); break;
    case PN_OPCODE_CAST_UITOFP_INT64_DOUBLE:  PN_OPCODE_CAST(u64, f64); break;
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT8:    PN_OPCODE_CAST(f32, i8); break;
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT16:   PN_OPCODE_CAST(f32, i16); break;
    case PN_OPCODE_CAST_BITCAST_FLOAT_INT32:
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT32:   PN_OPCODE_CAST(f32, i32); break;
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT64:   PN_OPCODE_CAST(f32, i64); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT8:    PN_OPCODE_CAST(f32, u8); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT16:   PN_OPCODE_CAST(f32, u16); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT32:   PN_OPCODE_CAST(f32, u32); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT64:   PN_OPCODE_CAST(f32, u64); break;
    case PN_OPCODE_CAST_FPEXT_FLOAT_DOUBLE:   PN_OPCODE_CAST(f32, f64); break;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT8:   PN_OPCODE_CAST(f64, i8); break;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT16:  PN_OPCODE_CAST(f64, i16); break;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT32:  PN_OPCODE_CAST(f64, i32); break;
    case PN_OPCODE_CAST_BITCAST_DOUBLE_INT64:
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT64:  PN_OPCODE_CAST(f64, i64); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT8:   PN_OPCODE_CAST(f64, u8); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT16:  PN_OPCODE_CAST(f64, u16); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT32:  PN_OPCODE_CAST(f64, u32); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT64:  PN_OPCODE_CAST(f64, u64); break;
    case PN_OPCODE_CAST_FPTRUNC_DOUBLE_FLOAT: PN_OPCODE_CAST(f64, f32); break;
    // clang-format on

#undef PN_OPCODE_CAST

#define PN_OPCODE_CMP2(ty)                                                 \
  do {                                                                     \
    PNRuntimeInstructionCmp2* i = (PNRuntimeInstructionCmp2*)inst;         \
    PN_TRACE(EXECUTE, "    %s = %u  %s = " PN_FORMAT_##ty                  \
             "  %s = " PN_FORMAT_##ty "\n",                                \
             PN_VALUE(i->result_value_id, u8), PN_VALUE(i->value0_id, ty), \
             PN_VALUE(i->value1_id, ty));                                  \
  } while (0) /* no semicolon */

    case PN_OPCODE_FCMP_OEQ_DOUBLE:
    case PN_OPCODE_FCMP_OGE_DOUBLE:
    case PN_OPCODE_FCMP_OGT_DOUBLE:
    case PN_OPCODE_FCMP_OLE_DOUBLE:
    case PN_OPCODE_FCMP_OLT_DOUBLE:
    case PN_OPCODE_FCMP_ONE_DOUBLE:
    case PN_OPCODE_FCMP_ORD_DOUBLE:
    case PN_OPCODE_FCMP_UEQ_DOUBLE:
    case PN_OPCODE_FCMP_UGE_DOUBLE:
    case PN_OPCODE_FCMP_UGT_DOUBLE:
    case PN_OPCODE_FCMP_ULE_DOUBLE:
    case PN_OPCODE_FCMP_ULT_DOUBLE:
    case PN_OPCODE_FCMP_UNE_DOUBLE:
    case PN_OPCODE_FCMP_UNO_DOUBLE:
      PN_OPCODE_CMP2(f64);
      break;

    case PN_OPCODE_FCMP_OEQ_FLOAT:
    case PN_OPCODE_FCMP_OGE_FLOAT:
    case PN_OPCODE_FCMP_OGT_FLOAT:
    case PN_OPCODE_FCMP_OLE_FLOAT:
    case PN_OPCODE_FCMP_OLT_FLOAT:
    case PN_OPCODE_FCMP_ONE_FLOAT:
    case PN_OPCODE_FCMP_ORD_FLOAT:
    case PN_OPCODE_FCMP_UEQ_FLOAT:
    case PN_OPCODE_FCMP_UGE_FLOAT:
    case PN_OPCODE_FCMP_UGT_FLOAT:
    case PN_OPCODE_FCMP_ULE_FLOAT:
    case PN_OPCODE_FCMP_ULT_FLOAT:
    case PN_OPCODE_FCMP_UNE_FLOAT:
    case PN_OPCODE_FCMP_UNO_FLOAT:
      PN_OPCODE_CMP2(f32);
      break;

    case PN_OPCODE_ICMP_EQ_INT8:
    case PN_OPCODE_ICMP_NE_INT8:
    case PN_OPCODE_ICMP_SGE_INT8:
    case PN_OPCODE_ICMP_SGT_INT8:
    case PN_OPCODE_ICMP_SLE_INT8:
    case PN_OPCODE_ICMP_SLT_INT8:
    case PN_OPCODE_ICMP_UGE_INT8:
    case PN_OPCODE_ICMP_UGT_INT8:
    case PN_OPCODE_ICMP_ULE_INT8:
    case PN_OPCODE_ICMP_ULT_INT8:
      PN_OPCODE_CMP2(i8);
      break;

    case PN_OPCODE_ICMP_EQ_INT16:
    case PN_OPCODE_ICMP_NE_INT16:
    case PN_OPCODE_ICMP_SGE_INT16:
    case PN_OPCODE_ICMP_SGT_INT16:
    case PN_OPCODE_ICMP_SLE_INT16:
    case PN_OPCODE_ICMP_SLT_INT16:
    case PN_OPCODE_ICMP_UGE_INT16:
    case PN_OPCODE_ICMP_UGT_INT16:
    case PN_OPCODE_ICMP_ULE_INT16:
    case PN_OPCODE_ICMP_ULT_INT16:
      PN_OPCODE_CMP2(i16);
      break;

    case PN_OPCODE_ICMP_EQ_INT32:
    case PN_OPCODE_ICMP_NE_INT32:
    case PN_OPCODE_ICMP_SGE_INT32:
    case PN_OPCODE_ICMP_SGT_INT32:
    case PN_OPCODE_ICMP_SLE_INT32:
    case PN_OPCODE_ICMP_SLT_INT32:
    case PN_OPCODE_ICMP_UGE_INT32:
    case PN_OPCODE_ICMP_UGT_INT32:
    case PN_OPCODE_ICMP_ULE_INT32:
    case PN_OPCODE_ICMP_ULT_INT32:
      PN_OPCODE_CMP2(i32);
      break;

    case PN_OPCODE_ICMP_EQ_INT64:
    case PN_OPCODE_ICMP_NE_INT64:
    case PN_OPCODE_ICMP_SGE_INT64:
    case PN_OPCODE_ICMP_SGT_INT64:
    case PN_OPCODE_ICMP_SLE_INT64:
    case PN_OPCODE_ICMP_SLT_INT64:
    case PN_OPCODE_ICMP_UGE_INT64:
    case PN_OPCODE_ICMP_UGT_INT64:
    case PN_OPCODE_ICMP_ULE_INT64:
    case PN_OPCODE_ICMP_ULT_INT64:
      PN_OPCODE_CMP2(i64);
      break;

#undef PN_OPCODE_CMP2

    case PN_OPCODE_INTRINSIC_LLVM_CTLZ_I32:
    case PN_OPCODE_INTRINSIC_LLVM_CTTZ_I32: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %u\n", PN_VALUE(arg_ids[0], u32),
               PN_VALUE(arg_ids[1], u8));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_CTLZ_I64:
    case PN_OPCODE_INTRINSIC_LLVM_CTTZ_I64: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_u64 "  %s = %u\n",
               PN_VALUE(arg_ids[0], u64), PN_VALUE(arg_ids[1], u8));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_MEMCPY:
    case PN_OPCODE_INTRINSIC_LLVM_MEMMOVE: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %u  %s = %u  %s = %u  %s = %u\n",
               PN_VALUE(arg_ids[0], u32), PN_VALUE(arg_ids[1], u32),
               PN_VALUE(arg_ids[2], u32), PN_VALUE(arg_ids[3], u32),
               PN_VALUE(arg_ids[4], u8));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_MEMSET: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %u  %s = %u  %s = %u  %s = %u\n",
               PN_VALUE(arg_ids[0], u32), PN_VALUE(arg_ids[1], u8),
               PN_VALUE(arg_ids[2], u32), PN_VALUE(arg_ids[3], u32),
               PN_VALUE(arg_ids[4], u8));
      break;
    }

#define PN_OPCODE_INTRINSIC_CMPXCHG(ty)                                   \
  do {                                                                    \
    PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;        \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);  \
    PN_TRACE(EXECUTE,                                                     \
             "    %s = " PN_FORMAT_##ty "  %s = %u  %s = " PN_FORMAT_##ty \
             "  %s = " PN_FORMAT_##ty " %s = %u  %s = %u\n",              \
             PN_VALUE(i->result_value_id, ty), PN_VALUE(arg_ids[0], u32), \
             PN_VALUE(arg_ids[1], ty), PN_VALUE(arg_ids[2], ty),          \
             PN_VALUE(arg_ids[3], u32), PN_VALUE(arg_ids[4], u32));       \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I8:
      PN_OPCODE_INTRINSIC_CMPXCHG(u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I16:
      PN_OPCODE_INTRINSIC_CMPXCHG(u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I32:
      PN_OPCODE_INTRINSIC_CMPXCHG(u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I64:
      PN_OPCODE_INTRINSIC_CMPXCHG(u64);
      break;

#undef PN_OPCODE_INTRINSIC_CMPXCHG

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_FENCE_ALL:
      break;

#define PN_OPCODE_INTRINSIC_LOAD(ty)                                        \
  do {                                                                      \
    PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;          \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);    \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty "  %s = %u  %s = %u\n",    \
             PN_VALUE(i->result_value_id, ty), PN_VALUE(arg_ids[0], u32),   \
             PN_VALUE(arg_ids[1], u32));                                    \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I8:
      PN_OPCODE_INTRINSIC_LOAD(i8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I16:
      PN_OPCODE_INTRINSIC_LOAD(i16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I32:
      PN_OPCODE_INTRINSIC_LOAD(i32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I64:
      PN_OPCODE_INTRINSIC_LOAD(i64);
      break;

#undef PN_OPCODE_INTRINSIC_LOAD

#define PN_OPCODE_INTRINSIC_RMW(ty)                                       \
  do {                                                                    \
    PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;        \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);  \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty                          \
             "  %s = %u  %s = %u  %s = " PN_FORMAT_##ty "  %s = %u\n",    \
             PN_VALUE(i->result_value_id, ty), PN_VALUE(arg_ids[0], u32), \
             PN_VALUE(arg_ids[1], u32), PN_VALUE(arg_ids[2], ty),         \
             PN_VALUE(arg_ids[3], u32));                                  \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I8:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I8:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I8:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I8:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I8:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I8:
      PN_OPCODE_INTRINSIC_RMW(u8);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I16:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I16:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I16:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I16:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I16:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I16:
      PN_OPCODE_INTRINSIC_RMW(u16);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I32:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I32:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I32:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I32:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I32:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I32:
      PN_OPCODE_INTRINSIC_RMW(u32);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I64:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I64:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I64:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I64:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I64:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I64:
      PN_OPCODE_INTRINSIC_RMW(u64);
      break;

#undef PN_OPCODE_INTRINSIC_RMW

    case PN_OPCODE_INTRINSIC_LLVM_NACL_LONGJMP: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %u\n", PN_VALUE(arg_ids[0], u32),
               PN_VALUE(arg_ids[1], u32));
      PN_TRACE(EXECUTE, "function = %%f%d  pc = %%%zd\n",
               thread->current_frame->location.function_id,
               thread->inst - function->instructions);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_NACL_SETJMP: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %u\n",
               PN_VALUE(i->result_value_id, u32), PN_VALUE(arg_ids[0], u32));
      break;
    }

#define PN_OPCODE_INTRINSIC_STORE(ty)                                    \
  do {                                                                   \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall); \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty "  %s = %u  %s = %u\n", \
             PN_VALUE(arg_ids[0], ty), PN_VALUE(arg_ids[1], u32),        \
             PN_VALUE(arg_ids[2], u32));                                 \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_STORE_I8:
      PN_OPCODE_INTRINSIC_STORE(u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_STORE_I16:
      PN_OPCODE_INTRINSIC_STORE(u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_STORE_I32:
      PN_OPCODE_INTRINSIC_STORE(u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_STORE_I64:
      PN_OPCODE_INTRINSIC_STORE(u64);
      break;

#undef PN_OPCODE_INTRINSIC_STORE

    case PN_OPCODE_INTRINSIC_LLVM_NACL_READ_TP: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PN_TRACE(EXECUTE, "    %s = %u\n", PN_VALUE(i->result_value_id, u32));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_SQRT_F32: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(EXECUTE, "    %s = %f  %s = %f\n",
               PN_VALUE(i->result_value_id, f32), PN_VALUE(arg_ids[0], f32));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_SQRT_F64: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(EXECUTE, "    %s = %f  %s = %f\n",
               PN_VALUE(i->result_value_id, f64), PN_VALUE(arg_ids[0], f64));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_STACKRESTORE: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(EXECUTE, "    %s = %u\n", PN_VALUE(arg_ids[0], u32));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_STACKSAVE: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PN_TRACE(EXECUTE, "    %s = %u\n", PN_VALUE(i->result_value_id, u32));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_TRAP:
      break;

    case PN_OPCODE_INTRINSIC_LLVM_BSWAP_I16:
    case PN_OPCODE_INTRINSIC_LLVM_BSWAP_I32:
    case PN_OPCODE_INTRINSIC_LLVM_BSWAP_I64:
    case PN_OPCODE_INTRINSIC_LLVM_FABS_F32:
    case PN_OPCODE_INTRINSIC_LLVM_FABS_F64:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I8:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I16:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I32:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I64:
    case PN_OPCODE_INTRINSIC_START:
      break;

#define PN_OPCODE_LOAD(ty)                                                \
  do {                                                                    \
    PNRuntimeInstructionLoad* i = (PNRuntimeInstructionLoad*)inst;        \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty "  %s = %u\n",           \
             PN_VALUE(i->result_value_id, ty), PN_VALUE(i->src_id, u32)); \
  } while (0) /*no semicolon */

    // clang-format off
    case PN_OPCODE_LOAD_DOUBLE: PN_OPCODE_LOAD(f64); break;
    case PN_OPCODE_LOAD_FLOAT: PN_OPCODE_LOAD(f32); break;
    case PN_OPCODE_LOAD_INT8: PN_OPCODE_LOAD(u8); break;
    case PN_OPCODE_LOAD_INT16: PN_OPCODE_LOAD(u16); break;
    case PN_OPCODE_LOAD_INT32: PN_OPCODE_LOAD(u32); break;
    case PN_OPCODE_LOAD_INT64: PN_OPCODE_LOAD(u64); break;
// clang-format on

#undef PN_OPCODE_LOAD

    case PN_OPCODE_RET: {
      if (thread->executor->exiting) {
        PN_TRACE(EXECUTE, "exiting\n");
      } else {
        PN_TRACE(EXECUTE, "function = %%f%d  pc = %%%zd\n",
                 thread->current_frame->location.function_id,
                 thread->inst - function->instructions);
      }
      break;
    }

    case PN_OPCODE_RET_VALUE: {
      if (thread->executor->exiting) {
        PN_TRACE(EXECUTE, "exiting\n");
      } else {
        PNRuntimeInstructionCall* c = (PNRuntimeInstructionCall*)thread->inst;
        PNRuntimeValue value = pn_thread_get_value(thread, c->result_value_id);
        pn_executor_value_trace(thread->executor, function, c->result_value_id,
                                value, "    ", "\n");
        PN_TRACE(EXECUTE, "function = %%f%d  pc = %%%zd\n",
                 thread->current_frame->location.function_id,
                 thread->inst - function->instructions);
      }
      break;
    }

#define PN_OPCODE_STORE(ty)                                          \
  do {                                                               \
    PNRuntimeInstructionStore* i = (PNRuntimeInstructionStore*)inst; \
    PN_TRACE(EXECUTE, "    %s = %u  %s = " PN_FORMAT_##ty "\n",      \
             PN_VALUE(i->dest_id, u32), PN_VALUE(i->value_id, ty));  \
  } while (0) /*no semicolon */

    // clang-format off
    case PN_OPCODE_STORE_DOUBLE: PN_OPCODE_STORE(f64); break;
    case PN_OPCODE_STORE_FLOAT: PN_OPCODE_STORE(f32); break;
    case PN_OPCODE_STORE_INT8: PN_OPCODE_STORE(u8); break;
    case PN_OPCODE_STORE_INT16: PN_OPCODE_STORE(u16); break;
    case PN_OPCODE_STORE_INT32: PN_OPCODE_STORE(u32); break;
    case PN_OPCODE_STORE_INT64: PN_OPCODE_STORE(u64); break;
// clang-format on

#undef PN_OPCODE_STORE

#define PN_OPCODE_SWITCH(ty)                                                  \
  do {                                                                        \
    PNRuntimeInstructionSwitch* i = (PNRuntimeInstructionSwitch*)inst;        \
    pn_basic_block_trace_phi_assigns(                                         \
        thread, old_function,                                                 \
        (void*)inst + sizeof(PNRuntimeInstructionSwitch));                    \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty "\n",                        \
             PN_VALUE(i->value_id, ty));                                      \
    PN_TRACE(EXECUTE, "pc = %%%zd\n", thread->inst - function->instructions); \
  } while (0) /* no semicolon */

    // clang-format off
    case PN_OPCODE_SWITCH_INT1:
    case PN_OPCODE_SWITCH_INT8:  PN_OPCODE_SWITCH(i8); break;
    case PN_OPCODE_SWITCH_INT16: PN_OPCODE_SWITCH(i16); break;
    case PN_OPCODE_SWITCH_INT32: PN_OPCODE_SWITCH(i32); break;
    case PN_OPCODE_SWITCH_INT64: PN_OPCODE_SWITCH(i64); break;
// clang-format on

    case PN_OPCODE_UNREACHABLE:
      break;

#undef PN_OPCODE_SWITCH

    case PN_OPCODE_VSELECT: {
      PNRuntimeInstructionVselect* i = (PNRuntimeInstructionVselect*)inst;
      PNRuntimeValue result = pn_thread_get_value(thread, i->result_value_id);
      PNRuntimeValue cond = pn_thread_get_value(thread, i->cond_id);
      PNValueId value_id = (cond.u8 & 1) ? i->true_value_id : i->false_value_id;
      pn_executor_value_trace(thread->executor, function, i->result_value_id,
                              result, "    ", "  ");
      pn_executor_value_trace(thread->executor, function, i->cond_id, cond, "",
                              "  ");
      pn_executor_value_trace(thread->executor, function, value_id, result, "",
                              "\n");
      break;
    }

    default:
      // PN_FATAL("Invalid runtime instruction opcode: %d\n", inst->opcode);
      break;
  }
#undef PN_VALUE
#undef PN_VALUE_OLD
}

static void pn_runtime_instruction_trace_intrinsics(
    PNThread* thread,
    PNRuntimeInstruction* inst) {
#define PN_ARG(i, ty) pn_thread_get_value(thread, arg_ids[i]).ty

  if (!PN_IS_TRACE(INTRINSICS)) {
    return;
  }

  switch (inst->opcode) {
    case PN_OPCODE_INTRINSIC_LLVM_MEMCPY: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(INTRINSICS,
               "    llvm.memcpy(dst_p:%u, src_p:%u, len:%u, align:%u, "
               "is_volatile:%u)\n",
               PN_ARG(0, u32), PN_ARG(1, u32), PN_ARG(2, u32), PN_ARG(3, u32),
               PN_ARG(4, u8));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_MEMSET: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(INTRINSICS,
               "    llvm.memset(dst_p:%u, value:%u, len:%u, align:%u, "
               "is_volatile:%u)\n",
               PN_ARG(0, u32), PN_ARG(1, u8), PN_ARG(2, u32), PN_ARG(3, u32),
               PN_ARG(4, u8));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_MEMMOVE: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(INTRINSICS,
               "    llvm.memmove(dst_p:%u, src_p:%u, len:%u, align:%u, "
               "is_volatile:%u)\n",
               PN_ARG(0, u32), PN_ARG(1, u32), PN_ARG(2, u32), PN_ARG(3, u32),
               PN_ARG(4, u8));
      break;
    }

#define PN_OPCODE_INTRINSIC_CMPXCHG(ty)                                  \
  do {                                                                   \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall); \
    PN_TRACE(INTRINSICS, "    llvm.nacl.atomic.cmpxchg." #ty             \
                         "(addr_p:%u, expected:" PN_FORMAT_##ty          \
             ", desired:" PN_FORMAT_##ty ", ...)\n",                     \
             PN_ARG(0, u32), PN_ARG(1, ty), PN_ARG(2, ty));              \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I8:
      PN_OPCODE_INTRINSIC_CMPXCHG(u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I16:
      PN_OPCODE_INTRINSIC_CMPXCHG(u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I32:
      PN_OPCODE_INTRINSIC_CMPXCHG(u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I64:
      PN_OPCODE_INTRINSIC_CMPXCHG(u64);
      break;

#undef PN_OPCODE_INTRINSIC_CMPXCHG

#define PN_OPCODE_INTRINSIC_LOAD(ty)                                     \
  do {                                                                   \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall); \
    PN_TRACE(INTRINSICS,                                                 \
             "    llvm.nacl.atomic.load." #ty "(addr_p:%u, flags:%u)\n", \
             PN_ARG(0, u32), PN_ARG(1, u32));                            \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I8:
      PN_OPCODE_INTRINSIC_LOAD(i8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I16:
      PN_OPCODE_INTRINSIC_LOAD(i16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I32:
      PN_OPCODE_INTRINSIC_LOAD(i32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I64:
      PN_OPCODE_INTRINSIC_LOAD(i64);
      break;

#undef PN_OPCODE_INTRINSIC_LOAD

#define PN_OPCODE_INTRINSIC_RMW(op, ty)                                      \
  do {                                                                       \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);     \
    PN_TRACE(INTRINSICS, "    llvm.nacl.atomic.rmw." #ty                     \
                         "(op: %s, addr_p:%u, value: " PN_FORMAT_##ty ")\n", \
             #op, PN_ARG(0, u32), PN_ARG(1, ty));                            \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I8:
      PN_OPCODE_INTRINSIC_RMW(+, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I8:
      PN_OPCODE_INTRINSIC_RMW(-, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I8:
      PN_OPCODE_INTRINSIC_RMW(&, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I8:
      PN_OPCODE_INTRINSIC_RMW(|, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I8:
      PN_OPCODE_INTRINSIC_RMW(^, u8);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I16:
      PN_OPCODE_INTRINSIC_RMW(+, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I16:
      PN_OPCODE_INTRINSIC_RMW(-, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I16:
      PN_OPCODE_INTRINSIC_RMW(&, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I16:
      PN_OPCODE_INTRINSIC_RMW(|, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I16:
      PN_OPCODE_INTRINSIC_RMW(^, u16);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I32:
      PN_OPCODE_INTRINSIC_RMW(+, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I32:
      PN_OPCODE_INTRINSIC_RMW(-, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I32:
      PN_OPCODE_INTRINSIC_RMW(&, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I32:
      PN_OPCODE_INTRINSIC_RMW(|, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I32:
      PN_OPCODE_INTRINSIC_RMW(^, u32);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I64:
      PN_OPCODE_INTRINSIC_RMW(+, u64);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I64:
      PN_OPCODE_INTRINSIC_RMW(-, u64);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I64:
      PN_OPCODE_INTRINSIC_RMW(&, u64);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I64:
      PN_OPCODE_INTRINSIC_RMW(|, u64);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I64:
      PN_OPCODE_INTRINSIC_RMW(^, u64);
      break;

#undef PN_OPCODE_INTRINSIC_RMW

#define PN_OPCODE_INTRINSIC_EXCHANGE(ty)                                 \
  do {                                                                   \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall); \
    PN_TRACE(INTRINSICS, "    llvm.nacl.atomic.exchange." #ty            \
                         "(addr_p:%u, value: " PN_FORMAT_##ty ")\n",     \
             PN_ARG(1, u32), PN_ARG(2, ty));                             \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I8:
      PN_OPCODE_INTRINSIC_EXCHANGE(u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I16:
      PN_OPCODE_INTRINSIC_EXCHANGE(u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I32:
      PN_OPCODE_INTRINSIC_EXCHANGE(u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I64:
      PN_OPCODE_INTRINSIC_EXCHANGE(u64);
      break;

#undef PN_OPCODE_INTRINSIC_EXCHANGE

    case PN_OPCODE_INTRINSIC_LLVM_NACL_LONGJMP: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(INTRINSICS, "    llvm.nacl.longjmp(jmpbuf: %u, value: %u)\n",
               PN_ARG(0, u32), PN_ARG(1, u32));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_NACL_SETJMP: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(INTRINSICS, "    llvm.nacl.setjmp(jmpbuf: %u)\n",
               PN_ARG(0, u32));
      break;
    }

#define PN_OPCODE_INTRINSIC_STORE(ty)                                    \
  do {                                                                   \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall); \
    PN_TRACE(INTRINSICS,                                                 \
             "    llvm.nacl.atomic.store.u32(value: " PN_FORMAT_##ty     \
             " addr_p:%u, flags: %u)\n",                                 \
             PN_ARG(0, ty), PN_ARG(1, u32), PN_ARG(2, u32));             \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_STORE_I8:
      PN_OPCODE_INTRINSIC_STORE(u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_STORE_I16:
      PN_OPCODE_INTRINSIC_STORE(u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_STORE_I32:
      PN_OPCODE_INTRINSIC_STORE(u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_STORE_I64:
      PN_OPCODE_INTRINSIC_STORE(u64);
      break;

#undef PN_OPCODE_INTRINSIC_STORE

    case PN_OPCODE_INTRINSIC_LLVM_NACL_READ_TP:
      PN_TRACE(INTRINSICS, "    llvm.nacl.read.tp()\n");
      break;

    case PN_OPCODE_INTRINSIC_LLVM_SQRT_F32: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(INTRINSICS, "    llvm.sqrt.f32(%f)\n", PN_ARG(0, f32));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_SQRT_F64: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(INTRINSICS, "    llvm.sqrt.f64(%f)\n", PN_ARG(0, f64));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_STACKRESTORE: {
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_TRACE(INTRINSICS, "    llvm.stackrestore(%u)\n", PN_ARG(0, u32));
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_STACKSAVE:
      PN_TRACE(INTRINSICS, "    llvm.stacksave()\n");
      break;

    case PN_OPCODE_INTRINSIC_LLVM_TRAP:
      PN_TRACE(INTRINSICS, "    llvm.trap()\n");
      break;

    case PN_OPCODE_INTRINSIC_LLVM_BSWAP_I16:
    case PN_OPCODE_INTRINSIC_LLVM_BSWAP_I32:
    case PN_OPCODE_INTRINSIC_LLVM_BSWAP_I64:
    case PN_OPCODE_INTRINSIC_LLVM_FABS_F32:
    case PN_OPCODE_INTRINSIC_LLVM_FABS_F64:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I8:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I16:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I32:
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_RMW_I64:
    case PN_OPCODE_INTRINSIC_START:

    default:
      break;
  }
#undef PN_ARG
}

void pn_trace_define_abbrev(PNModule* module,
                            PNAbbrevId abbrev_id,
                            PNAbbrev* abbrev,
                            PNBool in_blockinfo,
                            void* user_data) {
  pn_abbrev_trace(abbrev, abbrev_id, in_blockinfo);
}

void pn_trace_before_blockinfo_block(PNModule* module, void* user_data) {
  PN_TRACE(BLOCKINFO_BLOCK, "abbreviations {  // BlockID = %d\n",
           PN_BLOCKID_BLOCKINFO);
  PN_TRACE_INDENT(BLOCKINFO_BLOCK, 2);

  /* Indent 2 more, that we we can always dedent 2 on SETBID */
  PN_TRACE_INDENT(BLOCKINFO_BLOCK, 2);
}

void pn_trace_blockinfo_setbid(PNModule* module, PNBlockId block_id, void* user_data) {
  PN_TRACE_DEDENT(BLOCKINFO_BLOCK, 2);
  const char* name = NULL;
  (void)name;
  switch (block_id) {
    // clang-format off
    case PN_BLOCKID_BLOCKINFO: name = "abbreviations"; break;
    case PN_BLOCKID_MODULE: name = "module"; break;
    case PN_BLOCKID_CONSTANTS: name = "constants"; break;
    case PN_BLOCKID_FUNCTION: name = "function"; break;
    case PN_BLOCKID_VALUE_SYMTAB: name = "valuesymtab"; break;
    case PN_BLOCKID_TYPE: name = "type"; break;
    case PN_BLOCKID_GLOBALVAR: name = "globals"; break;
    default: PN_UNREACHABLE(); break;
      // clang-format on
  }
  PN_TRACE(BLOCKINFO_BLOCK, "%s:\n", name);
  PN_TRACE_INDENT(BLOCKINFO_BLOCK, 2);
}

void pn_trace_blockinfo_blockname(PNModule* module, void* user_data) {
  PN_TRACE(BLOCKINFO_BLOCK, "block name\n");
}

void pn_trace_blockinfo_setrecordname(PNModule* module, void* user_data) {
  PN_TRACE(BLOCKINFO_BLOCK, "block record name\n");
}

void pn_trace_after_blockinfo_block(PNModule* module, void* user_data) {
  PN_TRACE_DEDENT(BLOCKINFO_BLOCK, 4);
  PN_TRACE(BLOCKINFO_BLOCK, "}\n");
}

void pn_trace_before_type_block(PNModule* module, void* user_data) {
  PN_TRACE(TYPE_BLOCK, "types {  // BlockID = %d\n", PN_BLOCKID_TYPE);
  PN_TRACE_INDENT(TYPE_BLOCK, 2);
}

void pn_trace_type_num_entries(PNModule* module,
                               uint32_t num_types,
                               void* user_data) {
  PN_TRACE(TYPE_BLOCK, "count %d;\n", module->num_types);
}

void pn_trace_type_entry(PNModule* module,
                         PNTypeId type_id,
                         PNType* type,
                         void* user_data) {
  PN_TRACE(TYPE_BLOCK, "@t%d = %s;\n", type_id - 1,
           pn_type_describe_all(module, type_id - 1, NULL, PN_FALSE));
}

void pn_trace_after_type_block(PNModule* module, void* user_data) {
  PN_TRACE_DEDENT(TYPE_BLOCK, 2);
  PN_TRACE(TYPE_BLOCK, "}\n");
}

void pn_trace_before_globalvar_block(PNModule* module, void* user_data) {
  PN_TRACE(GLOBALVAR_BLOCK, "globals {  // BlockID = %d\n",
           PN_BLOCKID_GLOBALVAR);
  PN_TRACE_INDENT(GLOBALVAR_BLOCK, 2);
}

void pn_trace_globalvar_before_var(PNModule* module,
                                   PNGlobalVarId var_id,
                                   PNGlobalVar* var,
                                   PNValueId value_id,
                                   void* user_data) {
  PN_TRACE(GLOBALVAR_BLOCK, "%s %s, align %d,\n",
           var->is_constant ? "const" : "var",
           pn_value_describe(module, NULL, value_id), var->alignment);
  PN_TRACE_INDENT(GLOBALVAR_BLOCK, 2);
}

void pn_trace_globalvar_compound(PNModule* module,
                                 PNGlobalVarId var_id,
                                 PNGlobalVar* var,
                                 uint32_t num_initializers,
                                 void* user_data) {
  PN_TRACE(GLOBALVAR_BLOCK, "initializers %d {\n", var->num_initializers);
  PN_TRACE_INDENT(GLOBALVAR_BLOCK, 2);
}

void pn_trace_globalvar_zerofill(PNModule* module,
                                 PNGlobalVarId var_id,
                                 PNGlobalVar* var,
                                 uint32_t num_bytes,
                                 void* user_data) {
  PN_TRACE(GLOBALVAR_BLOCK, "zerofill %d;\n", num_bytes);
}

void pn_trace_globalvar_data(PNModule* module,
                             PNGlobalVarId var_id,
                             PNGlobalVar* var,
                             uint8_t* data,
                             uint32_t num_bytes,
                             void* user_data) {
  PN_TRACE(GLOBALVAR_BLOCK, "{");

  if (PN_IS_TRACE(GLOBALVAR_BLOCK)) {
    uint32_t i;
    for (i = 0; i < num_bytes; ++i) {
      if (i) {
        PN_PRINT(", ");
        if (i % 14 == 0) {
          PN_PRINT("\n");
          PN_TRACE_PRINT_INDENT();
          PN_PRINT(" ");
        }
      }
      PN_PRINT("%3d", data[i]);
    }
  }

  if (PN_IS_TRACE(GLOBALVAR_BLOCK)) {
    /* Use PN_PRINT instead of PN_TRACE to prevent the automatic inclusion of
     * the current indentation */
    PN_PRINT("}\n");
  }
}

void pn_trace_globalvar_reloc(PNModule* module,
                              PNGlobalVarId var_id,
                              PNGlobalVar* var,
                              uint32_t value_index,
                              int32_t addend,
                              void* user_data) {
  if (addend > 0) {
    PN_TRACE(GLOBALVAR_BLOCK, "reloc %s + %d;\n",
             pn_value_describe(module, NULL, value_index), addend);
  } else if (addend < 0) {
    PN_TRACE(GLOBALVAR_BLOCK, "reloc %s - %d;\n",
             pn_value_describe(module, NULL, value_index), -addend);
  } else {
    PN_TRACE(GLOBALVAR_BLOCK, "reloc %s;\n",
             pn_value_describe(module, NULL, value_index));
  }
}

void pn_trace_globalvar_count(PNModule * module, uint32_t count,
                              void* user_data) {
  PN_TRACE(GLOBALVAR_BLOCK, "count %d;\n", count);
}

void pn_trace_globalvar_after_var(PNModule* module,
                                  PNGlobalVarId var_id,
                                  PNGlobalVar* var,
                                  void* user_data) {
  /* Dedent if there was a previous variable */
  PN_TRACE_DEDENT(GLOBALVAR_BLOCK, 2);
  /* Additional dedent if there was a previous compound initializer
   */
  if (var->num_initializers > 1) {
    PN_TRACE(GLOBALVAR_BLOCK, "}\n");
    PN_TRACE_DEDENT(GLOBALVAR_BLOCK, 2);
  }
}

void pn_trace_after_globalvar_block(PNModule* module, void* user_data) {
  PN_TRACE_DEDENT(GLOBALVAR_BLOCK, 2);
  PN_TRACE(GLOBALVAR_BLOCK, "}\n");
}

void pn_trace_before_value_symtab_block(PNModule* module, void* user_data) {
  PN_TRACE(VALUE_SYMTAB_BLOCK, "valuesymtab {  // BlockID = %d\n",
           PN_BLOCKID_VALUE_SYMTAB);
  PN_TRACE_INDENT(VALUE_SYMTAB_BLOCK, 2);
}

void pn_trace_value_symtab_entry(PNModule* module,
                                 PNValueId value_id,
                                 const char* name,
                                 void* user_data) {
  PN_TRACE(VALUE_SYMTAB_BLOCK, "%s : \"%s\";\n",
           pn_value_describe(module, NULL, value_id), name);
}

void pn_trace_value_symtab_intrinsic(PNModule* module,
                                     PNIntrinsicId id,
                                     const char* name,
                                     void* user_data) {
  PN_TRACE(INTRINSICS, "intrinsic \"%s\" (%d)\n", name, id);
}

void pn_trace_after_value_symtab_block(PNModule* module, void* user_data) {
  PN_TRACE_DEDENT(VALUE_SYMTAB_BLOCK, 2);
  PN_TRACE(VALUE_SYMTAB_BLOCK, "}\n");
}

void pn_trace_before_constants_block(PNModule* module,
                                     PNFunction* function,
                                     void* user_data) {
  PN_TRACE(CONSTANTS_BLOCK, "constants {  // BlockID = %d\n",
           PN_BLOCKID_CONSTANTS);
  PN_TRACE_INDENT(CONSTANTS_BLOCK, 2);
  /* Indent 2 more, that we we can always dedent 2 on PN_CONSTANTS_CODE_SETTYPE
   */
  PN_TRACE_INDENT(CONSTANTS_BLOCK, 2);
}

void pn_trace_constants_settype(PNModule* module,
                                PNFunction* function,
                                PNTypeId type_id,
                                void* user_data) {
  PN_TRACE_DEDENT(CONSTANTS_BLOCK, 2);
  PN_TRACE(CONSTANTS_BLOCK, "%s:\n", pn_type_describe(module, type_id));
  PN_TRACE_INDENT(CONSTANTS_BLOCK, 2);
}

void pn_trace_constants_value(PNModule* module,
                              PNFunction* function,
                              PNConstantId constant_id,
                              PNConstant* constant,
                              PNValueId value_id,
                              void* user_data) {
  switch (constant->code) {
    case PN_CONSTANTS_CODE_UNDEF:
      PN_TRACE(CONSTANTS_BLOCK, "%s = %s undef;\n",
               pn_value_describe(module, function, value_id),
               pn_type_describe(module, constant->type_id));
      break;

    case PN_CONSTANTS_CODE_INTEGER: {
      uint32_t value;
      switch (constant->basic_type) {
        case PN_BASIC_TYPE_INT1:
        case PN_BASIC_TYPE_INT8:
          value = constant->value.i8;
          goto int32;

        case PN_BASIC_TYPE_INT16:
          value = constant->value.i16;
          goto int32;

        case PN_BASIC_TYPE_INT32:
          value = constant->value.i32;
          goto int32;

        int32:
          PN_TRACE(CONSTANTS_BLOCK, "%s = %s %d;\n",
                   pn_value_describe(module, function, value_id),
                   pn_type_describe(module, constant->type_id), value);
        break;

        case PN_BASIC_TYPE_INT64:
          PN_TRACE(CONSTANTS_BLOCK, "%s = %s %" PRId64 ";\n",
                   pn_value_describe(module, function, value_id),
                   pn_type_describe(module, constant->type_id),
                   constant->value.i64);
          break;

        default:
          PN_UNREACHABLE();
          break;
      }
      break;
    }

    case PN_CONSTANTS_CODE_FLOAT:
      if (constant->basic_type == PN_BASIC_TYPE_FLOAT) {
        PN_TRACE(CONSTANTS_BLOCK, "%s = %s %g;\n",
                 pn_value_describe(module, function, value_id),
                 pn_type_describe(module, constant->type_id),
                 constant->value.f32);
      } else {
        PN_TRACE(CONSTANTS_BLOCK, "%s = %s %g;\n",
                 pn_value_describe(module, function, value_id),
                 pn_type_describe(module, constant->type_id),
                 constant->value.f64);
      }
      break;

    default:
      PN_UNREACHABLE();
      break;
  }
}

void pn_trace_after_constants_block(PNModule* module,
                                    PNFunction* function,
                                    void* user_data) {
  PN_TRACE_DEDENT(CONSTANTS_BLOCK, 2);
  PN_TRACE(CONSTANTS_BLOCK, "}\n");
  PN_TRACE_DEDENT(CONSTANTS_BLOCK, 2);
}

void pn_trace_before_function_block(PNModule* module,
                                    PNFunctionId function_id,
                                    PNFunction* function,
                                    void* user_data) {
  if (PN_IS_TRACE(FUNCTION_BLOCK)) {
    pn_function_print_header(module, function, function_id);
  }
}

void pn_trace_function_numblocks(PNModule* module,
                                 PNFunctionId function_id,
                                 PNFunction* function,
                                 uint32_t num_bbs,
                                 void* user_data) {
  PN_TRACE(FUNCTION_BLOCK, "blocks %d;\n", num_bbs);
}

void pn_trace_after_function_block(PNModule* module,
                                   PNFunctionId function_id,
                                   PNFunction* function,
                                   void* user_data) {
  pn_function_trace(module, function, function_id);
  PN_TRACE_DEDENT(FUNCTION_BLOCK, 2);
  PN_TRACE(FUNCTION_BLOCK, "}\n");
}

void pn_trace_before_module_block(PNModule* module, void* user_data) {
  PN_TRACE(MODULE_BLOCK, "module {  // BlockID = %d\n", PN_BLOCKID_MODULE);
  PN_TRACE_INDENT(MODULE_BLOCK, 2);
}

void pn_trace_module_version(PNModule* module,
                             uint32_t version,
                             void* user_data) {
  PN_TRACE(MODULE_BLOCK, "version %d;\n", module->version);
}

void pn_trace_module_function(PNModule* module,
                              PNFunctionId function_id,
                              PNFunction* function,
                              PNValueId value_id,
                              void* user_data) {
  if (PN_IS_TRACE(MODULE_BLOCK)) {
    PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);

    PN_TRACE(MODULE_BLOCK, "%s %s %s;\n",
             function->is_proto ? "declare" : "define",
             function->linkage ? "internal" : "external",
             pn_type_describe_all(module, function->type_id,
                                  pn_value_describe(module, function, value_id),
                                  PN_FALSE));

    pn_allocator_reset_to_mark(&module->temp_allocator, mark);
  }
}

void pn_trace_after_module_block(PNModule* module, void* user_data) {
  PN_TRACE_DEDENT(MODULE_BLOCK, 2);
  PN_TRACE(MODULE_BLOCK, "}\n");
}

#else

static void pn_instruction_trace(PNModule* module,
                                 PNFunction* function,
                                 PNInstruction* inst,
                                 PNBool force) {}

static void pn_function_print_header(PNModule* module,
                                     PNFunction* function,
                                     PNFunctionId function_id) {};

static void pn_function_trace(PNModule* module,
                              PNFunction* function,
                              PNFunctionId function_id) {}

#endif /* PN_TRACING */

#endif /* PN_TRACE_H_ */
