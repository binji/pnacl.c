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
                                        PNBool with_param_names,
                                        PNBool basic) {
  if (type_id == PN_INVALID_TYPE_ID) {
    return "<invalid>";
  }

  PNType* type = pn_module_get_type(module, type_id);
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
      if (basic) {
        return "i32";
      }

      char* buffer = pn_allocator_alloc(&module->temp_allocator, 1, 1);
      uint32_t buffer_len = 1;
      buffer[0] = 0;

      pn_string_concat(&module->temp_allocator, &buffer, &buffer_len,
                       pn_type_describe_all(module, type->return_type, NULL,
                                            PN_FALSE, PN_TRUE),
                       0);
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
                         pn_type_describe_all(module, type->arg_types[n], NULL,
                                              PN_FALSE, PN_TRUE),
                         0);

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
  return pn_type_describe_all(module, type_id, NULL, PN_FALSE, PN_TRUE);
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
    value = pn_module_get_value(module, value_id);
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
    default: PN_UNREACHABLE(); break;
  }

  snprintf(buffer, buffer_size, "%c%c%d", sigil, code, value_id - base);
}

static const char* pn_value_describe(PNModule* module,
                                     PNFunction* function,
                                     PNValueId value_id) {
  char buffer[13];
  pn_value_print_to_string(module, function, value_id, buffer, sizeof(buffer));
  size_t len = strlen(buffer) + 1;
  char* retval = pn_allocator_alloc(&module->temp_allocator, len, 1);
  strcpy(retval, buffer);
  return retval;
}

static const char* pn_value_describe_temp(PNModule* module,
                                          PNFunction* function,
                                          PNValueId value_id) {
  static char buffer[13];
  pn_value_print_to_string(module, function, value_id, buffer, sizeof(buffer));
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

static void pn_instruction_trace(PNModule* module,
                                 PNFunction* function,
                                 PNInstruction* inst,
                                 PNBool force) {
  if (!(PN_IS_TRACE(INSTRUCTIONS) || force)) {
    return;
  }

  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);

  PN_TRACE_PRINT_INDENT();
  switch (inst->code) {
    case PN_FUNCTION_CODE_INST_BINOP: {
      PNInstructionBinop* i = (PNInstructionBinop*)inst;
      PNValue* result_value =
          pn_function_get_value(module, function, i->result_value_id);
      PNType* result_type = pn_module_get_type(module, result_value->type_id);
      PNBasicType result_basic_type = result_type->basic_type;
      PN_PRINT("%s = %s %s %s, %s;\n",
               pn_value_describe(module, function, i->result_value_id),
               result_basic_type < PN_BASIC_TYPE_FLOAT
                   ? pn_binop_get_name(i->binop_opcode)
                   : pn_binop_get_name_float(i->binop_opcode),
               pn_value_describe_type(module, function, i->result_value_id),
               pn_value_describe(module, function, i->value0_id),
               pn_value_describe(module, function, i->value1_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_CAST: {
      PNInstructionCast* i = (PNInstructionCast*)inst;
      PN_PRINT("%s = %s %s %s to %s;\n",
               pn_value_describe(module, function, i->result_value_id),
               pn_cast_get_name(i->cast_opcode),
               pn_value_describe_type(module, function, i->value_id),
               pn_value_describe(module, function, i->value_id),
               pn_value_describe_type(module, function, i->result_value_id));
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
        if (col + len + 2 > 80) {  /* +2 for ", " */
          PN_PRINT("\n");
          PN_TRACE_PRINT_INDENTX(4);
          col = g_pn_trace_indent + 4;
        }
        col += PN_PRINT("%s", buffer);
      }
      PN_PRINT(";\n");
      break;
    }

    case PN_FUNCTION_CODE_INST_ALLOCA: {
      PNInstructionAlloca* i = (PNInstructionAlloca*)inst;
      PN_PRINT("%s = alloca i8, %s %s, align %d;\n",
               pn_value_describe(module, function, i->result_value_id),
               pn_value_describe_type(module, function, i->size_id),
               pn_value_describe(module, function, i->size_id), i->alignment);
      break;
    }

    case PN_FUNCTION_CODE_INST_LOAD: {
      PNInstructionLoad* i = (PNInstructionLoad*)inst;
      PN_PRINT("%s = load %s* %s, align %d;\n",
               pn_value_describe(module, function, i->result_value_id),
               pn_value_describe_type(module, function, i->result_value_id),
               pn_value_describe(module, function, i->src_id), i->alignment);
      break;
    }

    case PN_FUNCTION_CODE_INST_STORE: {
      PNInstructionStore* i = (PNInstructionStore*)inst;
      PNValue* value = pn_function_get_value(module, function, i->value_id);
      const char* type_str =
          pn_type_describe_all(module, value->type_id, NULL, PN_FALSE, PN_TRUE);
      PN_PRINT("store %s %s, %s* %s, align %d;\n", type_str,
               pn_value_describe(module, function, i->value_id), type_str,
               pn_value_describe(module, function, i->dest_id), i->alignment);
      break;
    }

    case PN_FUNCTION_CODE_INST_CMP2: {
      PNInstructionCmp2* i = (PNInstructionCmp2*)inst;
      PN_PRINT("%s = %s %s %s, %s;\n",
               pn_value_describe(module, function, i->result_value_id),
               pn_cmp2_get_name(i->cmp2_opcode),
               pn_value_describe_type(module, function, i->value0_id),
               pn_value_describe(module, function, i->value0_id),
               pn_value_describe(module, function, i->value1_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_VSELECT: {
      PNInstructionVselect* i = (PNInstructionVselect*)inst;
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

    case PN_FUNCTION_CODE_INST_FORWARDTYPEREF: {
      PNInstructionForwardtyperef* i = (PNInstructionForwardtyperef*)inst;
      PN_PRINT("declare %s %s;\n",
               pn_type_describe(module, i->type_id),
               pn_value_describe(module, function, i->value_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_CALL:
    case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PNType* return_type = pn_module_get_type(module, i->return_type_id);
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
        PNValue* value = pn_function_get_value(module, function, i->arg_ids[n]);
        PN_PRINT("%s %s", pn_type_describe_all(module, value->type_id, NULL,
                                               PN_FALSE, PN_TRUE),
                 pn_value_describe(module, function, i->arg_ids[n]));
      }
      PN_PRINT(");\n");
      break;
    }

    default:
      PN_FATAL("Invalid instruction code: %d\n", inst->code);
      break;
  }

  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
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
#if 0
  uint32_t n;
#if PN_CALCULATE_LIVENESS
  PN_PRINT("preds:");
  for (n = 0; n < bb->num_pred_bbs; ++n) {
    PN_PRINT(" %d", bb->pred_bb_ids[n]);
  }
  PN_PRINT(" ");
#endif
  PN_PRINT("succs:");
  for (n = 0; n < bb->num_succ_bbs; ++n) {
    PN_PRINT(" %d", bb->succ_bb_ids[n]);
  }
  PN_PRINT(")\n");
  if (bb->first_def_id != PN_INVALID_VALUE_ID) {
    PN_PRINT(" defs: [%%%d,%%%d]\n", bb->first_def_id, bb->last_def_id);
  }
  if (bb->num_uses) {
    PN_PRINT(" uses:");
    for (n = 0; n < bb->num_uses; ++n) {
      PN_PRINT(" %%%d", bb->uses[n]);
    }
    PN_PRINT("\n");
  }
  if (bb->num_phi_uses) {
    PN_PRINT(" phi uses:");
    for (n = 0; n < bb->num_phi_uses; ++n) {
      PN_PRINT(" bb:%d=>%%%d", bb->phi_uses[n].incoming.bb_id,
               bb->phi_uses[n].incoming.value_id);
    }
    PN_PRINT("\n");
  }
  if (bb->num_phi_assigns) {
    PN_PRINT(" phi assigns:");
    for (n = 0; n < bb->num_phi_assigns; ++n) {
      PN_PRINT(" bb:%d,%%%d<=%%%d", bb->phi_assigns[n].bb_id,
               bb->phi_assigns[n].dest_value_id,
               bb->phi_assigns[n].source_value_id);
    }
    PN_PRINT("\n");
  }
  if (bb->num_livein) {
    PN_PRINT(" livein:");
    for (n = 0; n < bb->num_livein; ++n) {
      PN_PRINT(" %%%d", bb->livein[n]);
    }
    PN_PRINT("\n");
  }
  if (bb->num_liveout) {
    PN_PRINT(" liveout:");
    for (n = 0; n < bb->num_liveout; ++n) {
      PN_PRINT(" %%%d", bb->liveout[n]);
    }
    PN_PRINT("\n");
  }
#endif

  uint32_t i;
  for (i = 0; i < bb->num_instructions; ++i) {
    pn_instruction_trace(module, function, bb->instructions[i], force);
  }
}

static void pn_function_print_header(PNModule* module,
                                     PNFunction* function,
                                     PNFunctionId function_id) {
  PN_PRINT(
      "%*sfunction %s {  // BlockID = %d\n", g_pn_trace_indent, "",
      pn_type_describe_all(module, function->type_id,
                           pn_value_describe_temp(module, NULL, function_id),
                           PN_TRUE, PN_FALSE),
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

#else

static const char* pn_type_describe_all(PNModule* module,
                                        PNTypeId type_id,
                                        const char* name,
                                        PNBool with_param_names) {
  return "Unknown";
}


static const char* pn_type_describe(PNModule* module,
                                    PNTypeId type_id) {
  return "Unknown";
}

static const char* pn_value_describe(PNModule* module,
                                     PNFunction* function,
                                     PNValueId value_id) {
  return "Unknown";
}

static void pn_instruction_trace(PNModule* module,
                                 PNFunction* function,
                                 PNInstruction* inst,
                                 PNBool force) {}

static void pn_basic_block_trace(PNModule* module,
                                 PNFunction* function,
                                 PNBasicBlock* bb,
                                 PNBasicBlockId bb_id) {}

static void pn_function_print_header(PNModule* module,
                                     PNFunction* function,
                                     PNFunctionId function_id) {};

static void pn_function_trace(PNModule* module,
                              PNFunction* function,
                              PNFunctionId function_id) {}

#endif /* PN_TRACING */

#endif /* PN_TRACE_H_ */
