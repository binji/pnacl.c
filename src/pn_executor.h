/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_EXECUTOR_H_
#define PN_EXECUTOR_H_

#define PN_VALUE_DESCRIBE(id) pn_value_describe(module, function, id)

#define PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(ty, ctype)          \
  static inline PNRuntimeValue pn_executor_value_##ty(ctype x) { \
    PNRuntimeValue ret;                                          \
    ret.ty = x;                                                  \
    return ret;                                                  \
  }

PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(i8, int8_t)
PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(u8, uint8_t)
PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(i16, int16_t)
PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(u16, uint16_t)
PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(i32, int32_t)
PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(u32, uint32_t)
PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(i64, int64_t)
PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(u64, uint64_t)
PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(f32, float)
PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(f64, double)

#undef PN_DEFINE_EXECUTOR_VALUE_CONSTRUCTOR

static PNRuntimeValue pn_thread_get_value(PNThread* thread,
                                          PNValueId value_id) {
  if (value_id >= thread->executor->module->num_values) {
    value_id -= thread->executor->module->num_values;
    return thread->current_frame->function_values[value_id];
  } else {
    return thread->executor->module_values[value_id];
  }
}

static PNRuntimeValue pn_executor_get_value_from_frame(PNExecutor* executor,
                                                       PNCallFrame* frame,
                                                       PNValueId value_id) {
  if (value_id >= executor->module->num_values) {
    value_id -= executor->module->num_values;
    return frame->function_values[value_id];
  } else {
    return executor->module_values[value_id];
  }
}

static void pn_thread_set_value(PNThread* thread,
                                PNValueId value_id,
                                PNRuntimeValue value) {
  assert(value_id >= thread->executor->module->num_values);
  value_id -= thread->executor->module->num_values;
  thread->current_frame->function_values[value_id] = value;
}

static void pn_executor_init_module_values(PNExecutor* executor) {
  PNModule* module = executor->module;
  executor->module_values = pn_allocator_alloc(
      &executor->allocator, sizeof(PNRuntimeValue) * module->num_values,
      sizeof(PNRuntimeValue));

  PNValueId value_id;
  for (value_id = 0; value_id < module->num_values; ++value_id) {
    PNValue* value = &module->values[value_id];
    switch (value->code) {
      case PN_VALUE_CODE_FUNCTION:
        executor->module_values[value_id].u32 =
            pn_function_id_to_pointer(value->index);
        break;

      case PN_VALUE_CODE_GLOBAL_VAR: {
        PNGlobalVar* global_var =
            pn_module_get_global_var(module, value->index);
        executor->module_values[value_id].u32 = global_var->offset;
        break;
      }

      default:
        PN_UNREACHABLE();
        break;
    }
  }
}

static void pn_thread_push_function(PNThread* thread,
                                    PNFunctionId function_id,
                                    PNFunction* function) {
  PNCallFrame* frame = pn_allocator_alloc(
      &thread->allocator, sizeof(PNCallFrame), PN_DEFAULT_ALIGN);
  PNCallFrame* prev_frame = thread->current_frame;

  frame->location.function_id = function_id;
  frame->location.inst = function->instructions;
  frame->function_values = pn_allocator_alloc(
      &thread->allocator, sizeof(PNRuntimeValue) * function->num_values,
      sizeof(PNRuntimeValue));
  frame->memory_stack_top = prev_frame->memory_stack_top;
  frame->parent = prev_frame;
  frame->jmpbuf_head = NULL;
  frame->mark = pn_allocator_mark(&thread->allocator);

  thread->current_frame = frame;
  thread->function = function;
  thread->inst = function->instructions;

  uint32_t n;
  for (n = 0; n < function->num_constants; ++n) {
    PNConstant* constant = pn_function_get_constant(function, n);
    PNValueId value_id =
        thread->executor->module->num_values + function->num_args + n;
#ifndef NDEBUG
    PNValue* value =
        pn_function_get_value(thread->executor->module, function, value_id);
    assert(value->code == PN_VALUE_CODE_CONSTANT);
#endif /* NDEBUG */
    pn_thread_set_value(thread, value_id, constant->value);
  }
}

static void pn_thread_do_phi_assigns(PNThread* thread,
                                     PNFunction* function,
                                     void* dest_inst) {
  void* istream = thread->inst;
  uint32_t num_phi_assigns = *(uint32_t*)istream;
  PNRuntimePhiAssign* phi_assigns = (PNRuntimePhiAssign*)(istream + 4);
  istream += 4 + num_phi_assigns * sizeof(PNRuntimePhiAssign);

  PNAllocatorMark mark = pn_allocator_mark(&thread->allocator);
  PNModule* module = thread->executor->module;
  (void)module;
  PNRuntimeValue* temp = pn_allocator_alloc(
      &thread->allocator, sizeof(PNRuntimeValue) * num_phi_assigns,
      sizeof(PNRuntimeValue));

  /* First pass, read values to temp */
  uint32_t i;
  for (i = 0; i < num_phi_assigns; ++i) {
    temp[i] = pn_thread_get_value(thread, phi_assigns[i].source_value_id);
  }

  /* Second pass, write values from temp */
  for (i = 0; i < num_phi_assigns; ++i) {
    PNRuntimePhiAssign* assign = &phi_assigns[i];
    if (assign->inst == dest_inst) {
      pn_thread_set_value(thread, assign->dest_value_id, temp[i]);
      PN_TRACE(EXECUTE, "    %s <= %s\n",
               PN_VALUE_DESCRIBE(assign->dest_value_id),
               PN_VALUE_DESCRIBE(assign->source_value_id));
    }
  }

  pn_allocator_reset_to_mark(&thread->allocator, mark);
}

static void pn_executor_init(PNExecutor* executor, PNModule* module) {
  pn_allocator_init(&executor->allocator, PN_MIN_CHUNKSIZE, "executor");
  executor->module = module;
  executor->memory = module->memory;
  executor->heap_end = executor->memory->heap_start;
  executor->sentinel_frame.location.function_id = PN_INVALID_FUNCTION_ID;
  executor->sentinel_frame.location.inst = NULL;
  executor->sentinel_frame.function_values = NULL;
  executor->sentinel_frame.memory_stack_top = executor->memory->stack_end;
  executor->sentinel_frame.parent = NULL;
  executor->sentinel_frame.mark = pn_allocator_mark(&executor->allocator);
  executor->exit_code = 0;
  executor->exiting = PN_FALSE;
  executor->next_thread_id = 0;
  executor->dead_threads = NULL;

  PN_CHECK(pn_is_aligned(executor->memory->size, PN_PAGESIZE));
  PN_CHECK(pn_is_aligned(executor->memory->heap_start, PN_PAGESIZE));
  size_t pages = executor->memory->size >> PN_PAGESHIFT;
  pn_bitset_init(&executor->allocator, &executor->mapped_pages, pages);
  size_t start_pages = executor->memory->heap_start >> PN_PAGESHIFT;
  int i;
  for (i = 0; i < start_pages; ++i) {
    pn_bitset_set(&executor->mapped_pages, i, PN_TRUE);
  }

  pn_executor_init_module_values(executor);

  PNThread* thread = &executor->main_thread;
  pn_allocator_init(&thread->allocator, PN_MIN_CHUNKSIZE, "main thread");
  thread->current_frame = &executor->sentinel_frame;
  thread->tls = 0;
  thread->id = executor->next_thread_id++;
  thread->executor = executor;
  thread->next = thread;
  thread->prev = thread;
  thread->futex_state = PN_FUTEX_NONE;
  thread->module = module;

  PNFunctionId start_function_id = module->known_functions[PN_INTRINSIC_START];
  PN_CHECK(start_function_id != PN_INVALID_FUNCTION_ID);
  PNFunction* start_function =
      pn_module_get_function(module, start_function_id);

  pn_thread_push_function(thread, start_function_id, start_function);

  PN_CHECK(start_function->num_args == 1);

  PNValueId value_id = executor->module->num_values;
  PNRuntimeValue value;
  value.u32 = executor->memory->startinfo_start;
  pn_thread_set_value(thread, value_id, value);
}

#if PN_TRACING
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

    PNType* type = pn_module_get_type(executor->module, type_id);
    switch (type->basic_type) {
      case PN_BASIC_TYPE_INT1:
        PN_PRINT("%s%s = %u%s", prefix, PN_VALUE_DESCRIBE(value_id), value.u8,
                 postfix);
        break;
      case PN_BASIC_TYPE_INT8:
        PN_PRINT("%s%s = %u%s", prefix, PN_VALUE_DESCRIBE(value_id), value.u8,
                 postfix);
        break;
      case PN_BASIC_TYPE_INT16:
        PN_PRINT("%s%s = %u%s", prefix, PN_VALUE_DESCRIBE(value_id), value.u16,
                 postfix);
        break;
      case PN_BASIC_TYPE_INT32:
        PN_PRINT("%s%s = %u%s", prefix, PN_VALUE_DESCRIBE(value_id), value.u32,
                 postfix);
        break;
      case PN_BASIC_TYPE_INT64:
        PN_PRINT("%s%s = %" PRIu64 "%s", prefix, PN_VALUE_DESCRIBE(value_id),
                 value.u64, postfix);
        break;
      case PN_BASIC_TYPE_FLOAT:
        PN_PRINT("%s%s = %f%s", prefix, PN_VALUE_DESCRIBE(value_id), value.f32,
                 postfix);
        break;
      case PN_BASIC_TYPE_DOUBLE:
        PN_PRINT("%s%s = %f%s", prefix, PN_VALUE_DESCRIBE(value_id), value.f64,
                 postfix);
        break;
      default:
        PN_UNREACHABLE();
        break;
    }
  }
}

static void pn_thread_backtrace(PNThread* thread) {
  PNModule* module = thread->executor->module;
  PNCallFrame* frame = thread->current_frame;
  int n = 0;
  while (frame != &thread->executor->sentinel_frame) {
    PNLocation* location = &frame->location;
    PNFunction* function = &module->functions[location->function_id];
    if (function->name && function->name[0]) {
      PN_PRINT("%d. %s(%d) %zd\n", n, function->name, location->function_id,
               location->inst - function->instructions);
    } else {
      PN_PRINT("%d. %d %zd\n", n, location->function_id,
               location->inst - function->instructions);
    }

    frame = frame->parent;
    n++;
  }
}
#else
static void pn_executor_value_trace(PNExecutor* executor,
                                    PNFunction* function,
                                    PNValueId value_id,
                                    PNRuntimeValue value,
                                    const char* prefix,
                                    const char* postfix) {}

static void pn_thread_backtrace(PNThread* thread) {}
#endif

static void pn_thread_execute_instruction(PNThread* thread) {
  PNModule* module = thread->module;
  PNFunction* function = thread->function;
  PNRuntimeInstruction* inst = thread->inst;

#if PN_TRACING
  if (PN_IS_TRACE(EXECUTE)) {
    g_pn_trace_indent += 2;
    pn_runtime_instruction_trace(module, function, inst);
    g_pn_trace_indent -= 2;
  }
#endif

  g_pn_opcode_count[inst->opcode]++;

  switch (inst->opcode) {
    case PN_OPCODE_ALLOCA_INT32: {
      PNRuntimeInstructionAlloca* i = (PNRuntimeInstructionAlloca*)inst;
      PNRuntimeValue size = pn_thread_get_value(thread, i->size_id);
      thread->current_frame->memory_stack_top = pn_align_down(
          thread->current_frame->memory_stack_top - size.i32, i->alignment);
      if (thread == &thread->executor->main_thread &&
          thread->current_frame->memory_stack_top <
              thread->executor->heap_end) {
        PN_FATAL("Out of stack\n");
        break;
      }
      PNRuntimeValue result;
      result.u32 = thread->current_frame->memory_stack_top;
      pn_thread_set_value(thread, i->result_value_id, result);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %d\n",
               PN_VALUE_DESCRIBE(i->result_value_id), result.u32,
               PN_VALUE_DESCRIBE(i->size_id), size.i32);
      thread->inst += sizeof(PNRuntimeInstructionAlloca);
      break;
    }

#define PN_OPCODE_BINOP(op, ty)                                             \
  do {                                                                      \
    PNRuntimeInstructionBinop* i = (PNRuntimeInstructionBinop*)inst;        \
    PNRuntimeValue value0 = pn_thread_get_value(thread, i->value0_id);      \
    PNRuntimeValue value1 = pn_thread_get_value(thread, i->value1_id);      \
    PNRuntimeValue result = pn_executor_value_##ty(value0.ty op value1.ty); \
    pn_thread_set_value(thread, i->result_value_id, result);                \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty "  %s = " PN_FORMAT_##ty   \
             "  %s = " PN_FORMAT_##ty "\n",                                 \
             PN_VALUE_DESCRIBE(i->result_value_id), result.ty,              \
             PN_VALUE_DESCRIBE(i->value0_id), value0.ty,                    \
             PN_VALUE_DESCRIBE(i->value1_id), value1.ty);                   \
    thread->inst += sizeof(PNRuntimeInstructionBinop);                      \
  } while (0) /* no semicolon */

    // clang-format off
    case PN_OPCODE_BINOP_ADD_DOUBLE:  PN_OPCODE_BINOP(+, f64); break;
    case PN_OPCODE_BINOP_ADD_FLOAT:   PN_OPCODE_BINOP(+, f32); break;
    case PN_OPCODE_BINOP_ADD_INT8:    PN_OPCODE_BINOP(+, u8); break;
    case PN_OPCODE_BINOP_ADD_INT16:   PN_OPCODE_BINOP(+, u16); break;
    case PN_OPCODE_BINOP_ADD_INT32:   PN_OPCODE_BINOP(+, u32); break;
    case PN_OPCODE_BINOP_ADD_INT64:   PN_OPCODE_BINOP(+, u64); break;
    case PN_OPCODE_BINOP_AND_INT1:
    case PN_OPCODE_BINOP_AND_INT8:    PN_OPCODE_BINOP(&, u8); break;
    case PN_OPCODE_BINOP_AND_INT16:   PN_OPCODE_BINOP(&, u16); break;
    case PN_OPCODE_BINOP_AND_INT32:   PN_OPCODE_BINOP(&, u32); break;
    case PN_OPCODE_BINOP_AND_INT64:   PN_OPCODE_BINOP(&, u64); break;
    case PN_OPCODE_BINOP_ASHR_INT8:   PN_OPCODE_BINOP(>>, i8); break;
    case PN_OPCODE_BINOP_ASHR_INT16:  PN_OPCODE_BINOP(>>, i16); break;
    case PN_OPCODE_BINOP_ASHR_INT32:  PN_OPCODE_BINOP(>>, i32); break;
    case PN_OPCODE_BINOP_ASHR_INT64:  PN_OPCODE_BINOP(>>, i64); break;
    case PN_OPCODE_BINOP_LSHR_INT8:   PN_OPCODE_BINOP(>>, u8); break;
    case PN_OPCODE_BINOP_LSHR_INT16:  PN_OPCODE_BINOP(>>, u16); break;
    case PN_OPCODE_BINOP_LSHR_INT32:  PN_OPCODE_BINOP(>>, u32); break;
    case PN_OPCODE_BINOP_LSHR_INT64:  PN_OPCODE_BINOP(>>, u64); break;
    case PN_OPCODE_BINOP_MUL_DOUBLE:  PN_OPCODE_BINOP(*, f64); break;
    case PN_OPCODE_BINOP_MUL_FLOAT:   PN_OPCODE_BINOP(*, f32); break;
    case PN_OPCODE_BINOP_MUL_INT8:    PN_OPCODE_BINOP(*, u8); break;
    case PN_OPCODE_BINOP_MUL_INT16:   PN_OPCODE_BINOP(*, u16); break;
    case PN_OPCODE_BINOP_MUL_INT32:   PN_OPCODE_BINOP(*, u32); break;
    case PN_OPCODE_BINOP_MUL_INT64:   PN_OPCODE_BINOP(*, u64); break;
    case PN_OPCODE_BINOP_OR_INT1:
    case PN_OPCODE_BINOP_OR_INT8:     PN_OPCODE_BINOP(|, u8); break;
    case PN_OPCODE_BINOP_OR_INT16:    PN_OPCODE_BINOP(|, u16); break;
    case PN_OPCODE_BINOP_OR_INT32:    PN_OPCODE_BINOP(|, u32); break;
    case PN_OPCODE_BINOP_OR_INT64:    PN_OPCODE_BINOP(|, u64); break;
    case PN_OPCODE_BINOP_SDIV_DOUBLE: PN_OPCODE_BINOP(/, f64); break;
    case PN_OPCODE_BINOP_SDIV_FLOAT:  PN_OPCODE_BINOP(/, f32); break;
    case PN_OPCODE_BINOP_SDIV_INT32:  PN_OPCODE_BINOP(/, i32); break;
    case PN_OPCODE_BINOP_SDIV_INT64:  PN_OPCODE_BINOP(/, i64); break;
    case PN_OPCODE_BINOP_SHL_INT8:    PN_OPCODE_BINOP(<<, u8); break;
    case PN_OPCODE_BINOP_SHL_INT16:   PN_OPCODE_BINOP(<<, u16); break;
    case PN_OPCODE_BINOP_SHL_INT32:   PN_OPCODE_BINOP(<<, u32); break;
    case PN_OPCODE_BINOP_SHL_INT64:   PN_OPCODE_BINOP(<<, u64); break;
    case PN_OPCODE_BINOP_SREM_INT32:  PN_OPCODE_BINOP(%, i32); break;
    case PN_OPCODE_BINOP_SREM_INT64:  PN_OPCODE_BINOP(%, i64); break;
    case PN_OPCODE_BINOP_SUB_DOUBLE:  PN_OPCODE_BINOP(-, f64); break;
    case PN_OPCODE_BINOP_SUB_FLOAT:   PN_OPCODE_BINOP(-, f32); break;
    case PN_OPCODE_BINOP_SUB_INT8:    PN_OPCODE_BINOP(-, u8); break;
    case PN_OPCODE_BINOP_SUB_INT16:   PN_OPCODE_BINOP(-, u16); break;
    case PN_OPCODE_BINOP_SUB_INT32:   PN_OPCODE_BINOP(-, u32); break;
    case PN_OPCODE_BINOP_SUB_INT64:   PN_OPCODE_BINOP(-, u64); break;
    case PN_OPCODE_BINOP_UDIV_INT8:   PN_OPCODE_BINOP(/, u8); break;
    case PN_OPCODE_BINOP_UDIV_INT16:  PN_OPCODE_BINOP(/, u16); break;
    case PN_OPCODE_BINOP_UDIV_INT32:  PN_OPCODE_BINOP(/, u32); break;
    case PN_OPCODE_BINOP_UDIV_INT64:  PN_OPCODE_BINOP(/, u64); break;
    case PN_OPCODE_BINOP_UREM_INT8:   PN_OPCODE_BINOP(%, u8); break;
    case PN_OPCODE_BINOP_UREM_INT16:  PN_OPCODE_BINOP(%, u16); break;
    case PN_OPCODE_BINOP_UREM_INT32:  PN_OPCODE_BINOP(%, u32); break;
    case PN_OPCODE_BINOP_UREM_INT64:  PN_OPCODE_BINOP(%, u64); break;
    case PN_OPCODE_BINOP_XOR_INT1:
    case PN_OPCODE_BINOP_XOR_INT8:    PN_OPCODE_BINOP(^, u8); break;
    case PN_OPCODE_BINOP_XOR_INT16:   PN_OPCODE_BINOP(^, u16); break;
    case PN_OPCODE_BINOP_XOR_INT32:   PN_OPCODE_BINOP(^, u32); break;
    case PN_OPCODE_BINOP_XOR_INT64:   PN_OPCODE_BINOP(^, u64); break;
// clang-format on

#undef PN_OPCODE_BINOP

    case PN_OPCODE_BR: {
      PNRuntimeInstructionBr* i = (PNRuntimeInstructionBr*)inst;
      void* new_inst = i->inst;
      thread->inst += sizeof(PNRuntimeInstructionBr);
      pn_thread_do_phi_assigns(thread, function, new_inst);
      thread->inst = new_inst;
      PN_TRACE(EXECUTE, "pc = %%%zd\n", new_inst - function->instructions);
      break;
    }

    case PN_OPCODE_BR_INT1: {
      PNRuntimeInstructionBrInt1* i = (PNRuntimeInstructionBrInt1*)inst;
      PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);
      void* new_inst = value.u8 ? i->true_inst : i->false_inst;
      thread->inst += sizeof(PNRuntimeInstructionBrInt1);
      pn_thread_do_phi_assigns(thread, function, new_inst);
      thread->inst = new_inst;
      PN_TRACE(EXECUTE, "    %s = %u\n", PN_VALUE_DESCRIBE(i->value_id),
               value.u8);
      PN_TRACE(EXECUTE, "pc = %%%zd\n", new_inst - function->instructions);
      break;
    }

    case PN_OPCODE_CALL: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PNCallFrame* old_frame = thread->current_frame;
      old_frame->location.inst = thread->inst;

      PNFunctionId new_function_id;
      if (i->flags & PN_CALL_FLAGS_INDIRECT) {
        PNRuntimeValue function_value =
            pn_thread_get_value(thread, i->callee_id);
        PNFunctionId callee_function_id =
            pn_function_pointer_to_index(function_value.u32);
        if (callee_function_id < PN_MAX_BUILTINS) {
          /* Builtin function. Call it directly, don't set up a new frame */
          switch (callee_function_id) {
#define PN_BUILTIN(e)                                           \
  case PN_BUILTIN_##e: {                                        \
    PNRuntimeValue result =                                     \
        pn_builtin_##e(thread, function, i->num_args, arg_ids); \
    if (i->result_value_id != PN_INVALID_VALUE_ID) {            \
      pn_thread_set_value(thread, i->result_value_id, result);  \
    }                                                           \
    break;                                                      \
  }
            PN_FOREACH_BUILTIN(PN_BUILTIN)
#undef PN_BUILTIN
            default:
              PN_FATAL("Unknown builtin: %d\n", callee_function_id);
              break;
          }
          /* If the builtin was PN_BUILTIN_NACL_IRT_FUTEX_WAIT_ABS, then this
           * thread may have been blocked. If so, do not increment the
           * instruction counter */
          if (thread->state == PN_THREAD_RUNNING) {
            thread->inst += sizeof(PNRuntimeInstructionCall) +
                            i->num_args * sizeof(PNValueId);
          }
          break;
        } else {
          new_function_id = callee_function_id - PN_MAX_BUILTINS;
          assert(new_function_id < module->num_functions);
          PN_TRACE(EXECUTE, "    %s = %u ", PN_VALUE_DESCRIBE(i->callee_id),
                   function_value.u32);
        }
      } else {
        PNValue* function_value = pn_module_get_value(module, i->callee_id);
        assert(function_value->code == PN_VALUE_CODE_FUNCTION);
        new_function_id = function_value->index;
        PN_TRACE(EXECUTE, "    ");
      }

      PNFunction* new_function =
          pn_module_get_function(module, new_function_id);
      pn_thread_push_function(thread, new_function_id, new_function);

      uint32_t n;
      for (n = 0; n < i->num_args; ++n) {
        PNValueId value_id = module->num_values + n;
        PNRuntimeValue arg = pn_executor_get_value_from_frame(
            thread->executor, old_frame, arg_ids[n]);

        pn_executor_value_trace(thread->executor, function, arg_ids[n], arg, "",
                                "  ");
        pn_thread_set_value(thread, value_id, arg);
      }

      PN_TRACE(EXECUTE, "\nfunction = %%f%d  pc = %%0\n", new_function_id);
      break;
    }

    case PN_OPCODE_CAST_BITCAST_DOUBLE_INT64:
    case PN_OPCODE_CAST_BITCAST_FLOAT_INT32:
    case PN_OPCODE_CAST_BITCAST_INT32_FLOAT:
    case PN_OPCODE_CAST_BITCAST_INT64_DOUBLE: {
      PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;
      PNRuntimeValue result = pn_thread_get_value(thread, i->value_id);
      pn_thread_set_value(thread, i->result_value_id, result);
      pn_executor_value_trace(thread->executor, function, i->result_value_id,
                              result, "    ", "\n");
      pn_executor_value_trace(thread->executor, function, i->value_id, result,
                              "    ", "\n");

      thread->inst += sizeof(PNRuntimeInstructionCast);
      break;
    }

#define PN_OPCODE_CAST(from, to)                                         \
  do {                                                                   \
    PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;       \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);     \
    PNRuntimeValue result = pn_executor_value_##to(value.from);          \
    pn_thread_set_value(thread, i->result_value_id, result);             \
    PN_TRACE(EXECUTE,                                                    \
             "    %s = " PN_FORMAT_##to "  %s = " PN_FORMAT_##from "\n", \
             PN_VALUE_DESCRIBE(i->result_value_id), result.to,           \
             PN_VALUE_DESCRIBE(i->value_id), result.from);               \
    thread->inst += sizeof(PNRuntimeInstructionCast);                    \
  } while (0) /* no semicolon */

#define PN_OPCODE_CAST_SEXT1(size)                                   \
  do {                                                               \
    PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;   \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id); \
    PNRuntimeValue result =                                          \
        pn_executor_value_i##size(-(int##size##_t)(value.u8 & 1));   \
    pn_thread_set_value(thread, i->result_value_id, result);         \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_i##size "  %s = %u\n",   \
             PN_VALUE_DESCRIBE(i->result_value_id), result.i##size,  \
             PN_VALUE_DESCRIBE(i->value_id), result.u8);             \
    thread->inst += sizeof(PNRuntimeInstructionCast);                \
  } while (0) /* no semicolon */

#define PN_OPCODE_CAST_TRUNC1(size)                                  \
  do {                                                               \
    PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;   \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id); \
    PNRuntimeValue result = pn_executor_value_u8(value.u##size & 1); \
    pn_thread_set_value(thread, i->result_value_id, result);         \
    PN_TRACE(EXECUTE, "    %s = %u  %s = " PN_FORMAT_u##size "\n",   \
             PN_VALUE_DESCRIBE(i->result_value_id), result.u8,       \
             PN_VALUE_DESCRIBE(i->value_id), result.u##size);        \
    thread->inst += sizeof(PNRuntimeInstructionCast);                \
  } while (0) /* no semicolon */

#define PN_OPCODE_CAST_ZEXT1(size)                                   \
  do {                                                               \
    PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;   \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id); \
    PNRuntimeValue result =                                          \
        pn_executor_value_u##size((uint##size##_t)(value.u8 & 1));   \
    pn_thread_set_value(thread, i->result_value_id, result);         \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_u##size "  %s = %u\n",   \
             PN_VALUE_DESCRIBE(i->result_value_id), result.u##size,  \
             PN_VALUE_DESCRIBE(i->value_id), result.u8);             \
    thread->inst += sizeof(PNRuntimeInstructionCast);                \
  } while (0) /* no semicolon */

    // clang-format off
    case PN_OPCODE_CAST_FPEXT_FLOAT_DOUBLE:   PN_OPCODE_CAST(f32, f64); break;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT8:   PN_OPCODE_CAST(f64, i8); break;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT16:  PN_OPCODE_CAST(f64, i16); break;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT32:  PN_OPCODE_CAST(f64, i32); break;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT64:  PN_OPCODE_CAST(f64, i64); break;
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT8:    PN_OPCODE_CAST(f32, i8); break;
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT16:   PN_OPCODE_CAST(f32, i16); break;
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT32:   PN_OPCODE_CAST(f32, i32); break;
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT64:   PN_OPCODE_CAST(f32, i64); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT8:   PN_OPCODE_CAST(f64, u8); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT16:  PN_OPCODE_CAST(f64, u16); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT32:  PN_OPCODE_CAST(f64, u32); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT64:  PN_OPCODE_CAST(f64, u64); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT8:    PN_OPCODE_CAST(f32, u8); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT16:   PN_OPCODE_CAST(f32, u16); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT32:   PN_OPCODE_CAST(f32, u32); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT64:   PN_OPCODE_CAST(f32, u64); break;
    case PN_OPCODE_CAST_FPTRUNC_DOUBLE_FLOAT: PN_OPCODE_CAST(f64, f32); break;
    case PN_OPCODE_CAST_SEXT_INT1_INT8:       PN_OPCODE_CAST_SEXT1(8); break;
    case PN_OPCODE_CAST_SEXT_INT1_INT16:      PN_OPCODE_CAST_SEXT1(16); break;
    case PN_OPCODE_CAST_SEXT_INT1_INT32:      PN_OPCODE_CAST_SEXT1(32); break;
    case PN_OPCODE_CAST_SEXT_INT1_INT64:      PN_OPCODE_CAST_SEXT1(64); break;
    case PN_OPCODE_CAST_SEXT_INT8_INT16:      PN_OPCODE_CAST(i8, i16); break;
    case PN_OPCODE_CAST_SEXT_INT8_INT32:      PN_OPCODE_CAST(i8, i32); break;
    case PN_OPCODE_CAST_SEXT_INT8_INT64:      PN_OPCODE_CAST(i8, i64); break;
    case PN_OPCODE_CAST_SEXT_INT16_INT32:     PN_OPCODE_CAST(i16, i32); break;
    case PN_OPCODE_CAST_SEXT_INT16_INT64:     PN_OPCODE_CAST(i16, i64); break;
    case PN_OPCODE_CAST_SEXT_INT32_INT64:     PN_OPCODE_CAST(i32, i64); break;
    case PN_OPCODE_CAST_SITOFP_INT8_DOUBLE:   PN_OPCODE_CAST(i8, f64); break;
    case PN_OPCODE_CAST_SITOFP_INT8_FLOAT:    PN_OPCODE_CAST(i8, f32); break;
    case PN_OPCODE_CAST_SITOFP_INT16_DOUBLE:  PN_OPCODE_CAST(i16, f64); break;
    case PN_OPCODE_CAST_SITOFP_INT16_FLOAT:   PN_OPCODE_CAST(i16, f32); break;
    case PN_OPCODE_CAST_SITOFP_INT32_DOUBLE:  PN_OPCODE_CAST(i32, f64); break;
    case PN_OPCODE_CAST_SITOFP_INT32_FLOAT:   PN_OPCODE_CAST(i32, f32); break;
    case PN_OPCODE_CAST_SITOFP_INT64_DOUBLE:  PN_OPCODE_CAST(i64, f64); break;
    case PN_OPCODE_CAST_SITOFP_INT64_FLOAT:   PN_OPCODE_CAST(i64, f32); break;
    case PN_OPCODE_CAST_TRUNC_INT8_INT1:      PN_OPCODE_CAST_TRUNC1(8); break;
    case PN_OPCODE_CAST_TRUNC_INT16_INT1:     PN_OPCODE_CAST_TRUNC1(16); break;
    case PN_OPCODE_CAST_TRUNC_INT16_INT8:     PN_OPCODE_CAST(i16, i8); break;
    case PN_OPCODE_CAST_TRUNC_INT32_INT1:     PN_OPCODE_CAST_TRUNC1(32); break;
    case PN_OPCODE_CAST_TRUNC_INT32_INT8:     PN_OPCODE_CAST(i32, i8); break;
    case PN_OPCODE_CAST_TRUNC_INT32_INT16:    PN_OPCODE_CAST(i32, i16); break;
    case PN_OPCODE_CAST_TRUNC_INT64_INT8:     PN_OPCODE_CAST(i64, i8); break;
    case PN_OPCODE_CAST_TRUNC_INT64_INT16:    PN_OPCODE_CAST(i64, i16); break;
    case PN_OPCODE_CAST_TRUNC_INT64_INT32:    PN_OPCODE_CAST(i64, i32); break;
    case PN_OPCODE_CAST_UITOFP_INT8_DOUBLE:   PN_OPCODE_CAST(u8, f64); break;
    case PN_OPCODE_CAST_UITOFP_INT8_FLOAT:    PN_OPCODE_CAST(u8, f32); break;
    case PN_OPCODE_CAST_UITOFP_INT16_DOUBLE:  PN_OPCODE_CAST(u16, f64); break;
    case PN_OPCODE_CAST_UITOFP_INT16_FLOAT:   PN_OPCODE_CAST(u16, f32); break;
    case PN_OPCODE_CAST_UITOFP_INT32_DOUBLE:  PN_OPCODE_CAST(u32, f64); break;
    case PN_OPCODE_CAST_UITOFP_INT32_FLOAT:   PN_OPCODE_CAST(u32, f32); break;
    case PN_OPCODE_CAST_UITOFP_INT64_DOUBLE:  PN_OPCODE_CAST(u64, f64); break;
    case PN_OPCODE_CAST_UITOFP_INT64_FLOAT:   PN_OPCODE_CAST(u64, f32); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT8:       PN_OPCODE_CAST_ZEXT1(8); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT16:      PN_OPCODE_CAST_ZEXT1(16); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT32:      PN_OPCODE_CAST_ZEXT1(32); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT64:      PN_OPCODE_CAST_ZEXT1(64); break;
    case PN_OPCODE_CAST_ZEXT_INT8_INT16:      PN_OPCODE_CAST(u8, u16); break;
    case PN_OPCODE_CAST_ZEXT_INT8_INT32:      PN_OPCODE_CAST(u8, u32); break;
    case PN_OPCODE_CAST_ZEXT_INT8_INT64:      PN_OPCODE_CAST(u8, u64); break;
    case PN_OPCODE_CAST_ZEXT_INT16_INT32:     PN_OPCODE_CAST(u16, u32); break;
    case PN_OPCODE_CAST_ZEXT_INT16_INT64:     PN_OPCODE_CAST(u16, u64); break;
    case PN_OPCODE_CAST_ZEXT_INT32_INT64:     PN_OPCODE_CAST(u32, u64); break;
// clang-format on

#undef PN_OPCODE_CAST
#undef PN_OPCODE_CAST_SEXT1
#undef PN_OPCODE_CAST_TRUNC1
#undef PN_OPCODE_CAST_ZEXT1

#define PN_OPCODE_CMP2(op, ty)                                            \
  do {                                                                    \
    PNRuntimeInstructionCmp2* i = (PNRuntimeInstructionCmp2*)inst;        \
    PNRuntimeValue value0 = pn_thread_get_value(thread, i->value0_id);    \
    PNRuntimeValue value1 = pn_thread_get_value(thread, i->value1_id);    \
    PNRuntimeValue result = pn_executor_value_u8(value0.ty op value1.ty); \
    pn_thread_set_value(thread, i->result_value_id, result);              \
    PN_TRACE(EXECUTE, "    %s = %u  %s = " PN_FORMAT_##ty                 \
             "  %s = " PN_FORMAT_##ty "\n",                               \
             PN_VALUE_DESCRIBE(i->result_value_id), result.u8,            \
             PN_VALUE_DESCRIBE(i->value0_id), value0.ty,                  \
             PN_VALUE_DESCRIBE(i->value1_id), value1.ty);                 \
    thread->inst += sizeof(PNRuntimeInstructionCmp2);                     \
  } while (0) /* no semicolon */

#define PN_OPCODE_CMP2_NOT(op, ty)                                           \
  do {                                                                       \
    PNRuntimeInstructionCmp2* i = (PNRuntimeInstructionCmp2*)inst;           \
    PNRuntimeValue value0 = pn_thread_get_value(thread, i->value0_id);       \
    PNRuntimeValue value1 = pn_thread_get_value(thread, i->value1_id);       \
    PNRuntimeValue result = pn_executor_value_u8(!(value0.ty op value1.ty)); \
    pn_thread_set_value(thread, i->result_value_id, result);                 \
    PN_TRACE(EXECUTE, "    %s = %u  %s = " PN_FORMAT_##ty                    \
             "  %s = " PN_FORMAT_##ty "\n",                                  \
             PN_VALUE_DESCRIBE(i->result_value_id), result.u8,               \
             PN_VALUE_DESCRIBE(i->value0_id), value0.ty,                     \
             PN_VALUE_DESCRIBE(i->value1_id), value1.ty);                    \
    thread->inst += sizeof(PNRuntimeInstructionCmp2);                        \
  } while (0) /* no semicolon */

#define PN_OPCODE_CMP2_ORD(ty)                                             \
  do {                                                                     \
    PNRuntimeInstructionCmp2* i = (PNRuntimeInstructionCmp2*)inst;         \
    PNRuntimeValue value0 = pn_thread_get_value(thread, i->value0_id);     \
    PNRuntimeValue value1 = pn_thread_get_value(thread, i->value1_id);     \
    PNRuntimeValue result = pn_executor_value_u8(value0.ty == value1.ty || \
                                                 value0.ty != value1.ty);  \
    pn_thread_set_value(thread, i->result_value_id, result);               \
    PN_TRACE(EXECUTE, "    %s = %u  %s = " PN_FORMAT_##ty                  \
             "  %s = " PN_FORMAT_##ty "\n",                                \
             PN_VALUE_DESCRIBE(i->result_value_id), result.u8,             \
             PN_VALUE_DESCRIBE(i->value0_id), value0.ty,                   \
             PN_VALUE_DESCRIBE(i->value1_id), value1.ty);                  \
    thread->inst += sizeof(PNRuntimeInstructionCmp2);                      \
  } while (0) /* no semicolon */

#define PN_OPCODE_CMP2_UNO(ty)                                         \
  do {                                                                 \
    PNRuntimeInstructionCmp2* i = (PNRuntimeInstructionCmp2*)inst;     \
    PNRuntimeValue value0 = pn_thread_get_value(thread, i->value0_id); \
    PNRuntimeValue value1 = pn_thread_get_value(thread, i->value1_id); \
    PNRuntimeValue result = pn_executor_value_u8(                      \
        !(value0.ty == value1.ty || value0.ty != value1.ty));          \
    pn_thread_set_value(thread, i->result_value_id, result);           \
    PN_TRACE(EXECUTE, "    %s = %u  %s = " PN_FORMAT_##ty              \
             "  %s = " PN_FORMAT_##ty "\n",                            \
             PN_VALUE_DESCRIBE(i->result_value_id), result.u8,         \
             PN_VALUE_DESCRIBE(i->value0_id), value0.ty,               \
             PN_VALUE_DESCRIBE(i->value1_id), value1.ty);              \
    thread->inst += sizeof(PNRuntimeInstructionCmp2);                  \
  } while (0) /* no semicolon */

    //        U L G E
    // FALSE  0 0 0 0
    // OEQ    0 0 0 1  A == B
    // OGT    0 0 1 0  A > B
    // OGE    0 0 1 1  A >= B
    // OLT    0 1 0 0  A < B
    // OLE    0 1 0 1  A <= B
    // ONE    0 1 1 0  A != B
    // ORD    0 1 1 1  A == B || A != B
    // UNO    1 0 0 0  !(A == B || A != B)
    // UEQ    1 0 0 1  !(A != B)
    // UGT    1 0 1 0  !(A <= B)
    // UGE    1 0 1 1  !(A < B)
    // ULT    1 1 0 0  !(A >= B)
    // ULE    1 1 0 1  !(A > B)
    // UNE    1 1 1 0  !(A == B)
    // TRUE   1 1 1 1

    // clang-format off
    case PN_OPCODE_FCMP_OEQ_DOUBLE: PN_OPCODE_CMP2(==, f64); break;
    case PN_OPCODE_FCMP_OEQ_FLOAT:  PN_OPCODE_CMP2(==, f32); break;
    case PN_OPCODE_FCMP_OGE_DOUBLE: PN_OPCODE_CMP2(>=, f64); break;
    case PN_OPCODE_FCMP_OGE_FLOAT:  PN_OPCODE_CMP2(>=, f32); break;
    case PN_OPCODE_FCMP_OGT_DOUBLE: PN_OPCODE_CMP2(>, f64); break;
    case PN_OPCODE_FCMP_OGT_FLOAT:  PN_OPCODE_CMP2(>, f32); break;
    case PN_OPCODE_FCMP_OLE_DOUBLE: PN_OPCODE_CMP2(<=, f64); break;
    case PN_OPCODE_FCMP_OLE_FLOAT:  PN_OPCODE_CMP2(<=, f32); break;
    case PN_OPCODE_FCMP_OLT_DOUBLE: PN_OPCODE_CMP2(<, f64); break;
    case PN_OPCODE_FCMP_OLT_FLOAT:  PN_OPCODE_CMP2(<, f32); break;
    case PN_OPCODE_FCMP_ONE_DOUBLE: PN_OPCODE_CMP2(!=, f64); break;
    case PN_OPCODE_FCMP_ONE_FLOAT:  PN_OPCODE_CMP2(!=, f32); break;
    case PN_OPCODE_FCMP_ORD_DOUBLE: PN_OPCODE_CMP2_ORD(f64); break;
    case PN_OPCODE_FCMP_ORD_FLOAT:  PN_OPCODE_CMP2_ORD(f32); break;
    case PN_OPCODE_FCMP_UEQ_DOUBLE: PN_OPCODE_CMP2_NOT(!=, f64); break;
    case PN_OPCODE_FCMP_UEQ_FLOAT:  PN_OPCODE_CMP2_NOT(!=, f32); break;
    case PN_OPCODE_FCMP_UGE_DOUBLE: PN_OPCODE_CMP2_NOT(<, f64); break;
    case PN_OPCODE_FCMP_UGE_FLOAT:  PN_OPCODE_CMP2_NOT(<, f32); break;
    case PN_OPCODE_FCMP_UGT_DOUBLE: PN_OPCODE_CMP2_NOT(<=, f64); break;
    case PN_OPCODE_FCMP_UGT_FLOAT:  PN_OPCODE_CMP2_NOT(<=, f32); break;
    case PN_OPCODE_FCMP_ULE_DOUBLE: PN_OPCODE_CMP2_NOT(>, f64); break;
    case PN_OPCODE_FCMP_ULE_FLOAT:  PN_OPCODE_CMP2_NOT(>, f32); break;
    case PN_OPCODE_FCMP_ULT_DOUBLE: PN_OPCODE_CMP2_NOT(>=, f64); break;
    case PN_OPCODE_FCMP_ULT_FLOAT:  PN_OPCODE_CMP2_NOT(>=, f32); break;
    case PN_OPCODE_FCMP_UNE_DOUBLE: PN_OPCODE_CMP2_NOT(==, f64); break;
    case PN_OPCODE_FCMP_UNE_FLOAT:  PN_OPCODE_CMP2_NOT(==, f32); break;
    case PN_OPCODE_FCMP_UNO_DOUBLE: PN_OPCODE_CMP2_UNO(f64); break;
    case PN_OPCODE_FCMP_UNO_FLOAT:  PN_OPCODE_CMP2_UNO(f32); break;

    case PN_OPCODE_ICMP_EQ_INT8:   PN_OPCODE_CMP2(==, u8); break;
    case PN_OPCODE_ICMP_EQ_INT16:  PN_OPCODE_CMP2(==, u16); break;
    case PN_OPCODE_ICMP_EQ_INT32:  PN_OPCODE_CMP2(==, u32); break;
    case PN_OPCODE_ICMP_EQ_INT64:  PN_OPCODE_CMP2(==, u64); break;
    case PN_OPCODE_ICMP_NE_INT8:   PN_OPCODE_CMP2(!=, u8); break;
    case PN_OPCODE_ICMP_NE_INT16:  PN_OPCODE_CMP2(!=, u16); break;
    case PN_OPCODE_ICMP_NE_INT32:  PN_OPCODE_CMP2(!=, u32); break;
    case PN_OPCODE_ICMP_NE_INT64:  PN_OPCODE_CMP2(!=, u64); break;
    case PN_OPCODE_ICMP_SGE_INT8:  PN_OPCODE_CMP2(>=, i8); break;
    case PN_OPCODE_ICMP_SGE_INT16: PN_OPCODE_CMP2(>=, i16); break;
    case PN_OPCODE_ICMP_SGE_INT32: PN_OPCODE_CMP2(>=, i32); break;
    case PN_OPCODE_ICMP_SGE_INT64: PN_OPCODE_CMP2(>=, i64); break;
    case PN_OPCODE_ICMP_SGT_INT8:  PN_OPCODE_CMP2(>, i8); break;
    case PN_OPCODE_ICMP_SGT_INT16: PN_OPCODE_CMP2(>, i16); break;
    case PN_OPCODE_ICMP_SGT_INT32: PN_OPCODE_CMP2(>, i32); break;
    case PN_OPCODE_ICMP_SGT_INT64: PN_OPCODE_CMP2(>, i64); break;
    case PN_OPCODE_ICMP_SLE_INT8:  PN_OPCODE_CMP2(<=, i8); break;
    case PN_OPCODE_ICMP_SLE_INT16: PN_OPCODE_CMP2(<=, i16); break;
    case PN_OPCODE_ICMP_SLE_INT32: PN_OPCODE_CMP2(<=, i32); break;
    case PN_OPCODE_ICMP_SLE_INT64: PN_OPCODE_CMP2(<=, i64); break;
    case PN_OPCODE_ICMP_SLT_INT8:  PN_OPCODE_CMP2(<, i8); break;
    case PN_OPCODE_ICMP_SLT_INT16: PN_OPCODE_CMP2(<, i16); break;
    case PN_OPCODE_ICMP_SLT_INT32: PN_OPCODE_CMP2(<, i32); break;
    case PN_OPCODE_ICMP_SLT_INT64: PN_OPCODE_CMP2(<, i64); break;
    case PN_OPCODE_ICMP_UGE_INT8:  PN_OPCODE_CMP2(>=, u8); break;
    case PN_OPCODE_ICMP_UGE_INT16: PN_OPCODE_CMP2(>=, u16); break;
    case PN_OPCODE_ICMP_UGE_INT32: PN_OPCODE_CMP2(>=, u32); break;
    case PN_OPCODE_ICMP_UGE_INT64: PN_OPCODE_CMP2(>=, u64); break;
    case PN_OPCODE_ICMP_UGT_INT8:  PN_OPCODE_CMP2(>, u8); break;
    case PN_OPCODE_ICMP_UGT_INT16: PN_OPCODE_CMP2(>, u16); break;
    case PN_OPCODE_ICMP_UGT_INT32: PN_OPCODE_CMP2(>, u32); break;
    case PN_OPCODE_ICMP_UGT_INT64: PN_OPCODE_CMP2(>, u64); break;
    case PN_OPCODE_ICMP_ULE_INT8:  PN_OPCODE_CMP2(<=, u8); break;
    case PN_OPCODE_ICMP_ULE_INT16: PN_OPCODE_CMP2(<=, u16); break;
    case PN_OPCODE_ICMP_ULE_INT32: PN_OPCODE_CMP2(<=, u32); break;
    case PN_OPCODE_ICMP_ULE_INT64: PN_OPCODE_CMP2(<=, u64); break;
    case PN_OPCODE_ICMP_ULT_INT8:  PN_OPCODE_CMP2(<, u8); break;
    case PN_OPCODE_ICMP_ULT_INT16: PN_OPCODE_CMP2(<, u16); break;
    case PN_OPCODE_ICMP_ULT_INT32: PN_OPCODE_CMP2(<, u32); break;
    case PN_OPCODE_ICMP_ULT_INT64: PN_OPCODE_CMP2(<, u64); break;
// clang-format on

#undef PN_OPCODE_CMP2
#undef PN_OPCODE_CMP2_NOT
#undef PN_OPCODE_CMP2_ORD
#undef PN_OPCODE_CMP2_UNO

    case PN_OPCODE_INTRINSIC_LLVM_MEMCPY: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 5);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t dst_p = pn_thread_get_value(thread, arg_ids[0]).u32;
      uint32_t src_p = pn_thread_get_value(thread, arg_ids[1]).u32;
      uint32_t len = pn_thread_get_value(thread, arg_ids[2]).u32;
      uint32_t align = pn_thread_get_value(thread, arg_ids[3]).u32;
      uint8_t is_volatile = pn_thread_get_value(thread, arg_ids[4]).u8;
      PN_TRACE(INTRINSICS,
               "    llvm.memcpy(dst_p:%u, src_p:%u, len:%u, align:%u, "
               "is_volatile:%u)\n",
               dst_p, src_p, len, align, is_volatile);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %u  %s = %u  %s = %u  %s = %u\n",
               PN_VALUE_DESCRIBE(arg_ids[0]), dst_p,
               PN_VALUE_DESCRIBE(arg_ids[1]), src_p,
               PN_VALUE_DESCRIBE(arg_ids[2]), len,
               PN_VALUE_DESCRIBE(arg_ids[3]), align,
               PN_VALUE_DESCRIBE(arg_ids[4]), is_volatile);
      (void)align;
      (void)is_volatile;

      if (len > 0) {
        pn_memory_check(thread->executor->memory, dst_p, len);
        pn_memory_check(thread->executor->memory, src_p, len);
        void* dst_pointer = thread->executor->memory->data + dst_p;
        void* src_pointer = thread->executor->memory->data + src_p;
        memcpy(dst_pointer, src_pointer, len);
      }
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_MEMSET: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 5);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t dst_p = pn_thread_get_value(thread, arg_ids[0]).u32;
      uint8_t value = pn_thread_get_value(thread, arg_ids[1]).u8;
      uint32_t len = pn_thread_get_value(thread, arg_ids[2]).u32;
      uint32_t align = pn_thread_get_value(thread, arg_ids[3]).u32;
      uint8_t is_volatile = pn_thread_get_value(thread, arg_ids[4]).u8;
      PN_TRACE(INTRINSICS,
               "    llvm.memset(dst_p:%u, value:%u, len:%u, align:%u, "
               "is_volatile:%u)\n",
               dst_p, value, len, align, is_volatile);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %u  %s = %u  %s = %u  %s = %u\n",
               PN_VALUE_DESCRIBE(arg_ids[0]), dst_p,
               PN_VALUE_DESCRIBE(arg_ids[1]), value,
               PN_VALUE_DESCRIBE(arg_ids[2]), len,
               PN_VALUE_DESCRIBE(arg_ids[3]), align,
               PN_VALUE_DESCRIBE(arg_ids[4]), is_volatile);
      (void)align;
      (void)is_volatile;

      if (len > 0) {
        pn_memory_check(thread->executor->memory, dst_p, len);
        void* dst_pointer = thread->executor->memory->data + dst_p;
        memset(dst_pointer, value, len);
      }
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_MEMMOVE: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 5);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t dst_p = pn_thread_get_value(thread, arg_ids[0]).u32;
      uint32_t src_p = pn_thread_get_value(thread, arg_ids[1]).u32;
      uint32_t len = pn_thread_get_value(thread, arg_ids[2]).u32;
      uint32_t align = pn_thread_get_value(thread, arg_ids[3]).u32;
      uint8_t is_volatile = pn_thread_get_value(thread, arg_ids[4]).u8;
      PN_TRACE(INTRINSICS,
               "    llvm.memmove(dst_p:%u, src_p:%u, len:%u, align:%u, "
               "is_volatile:%u)\n",
               dst_p, src_p, len, align, is_volatile);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %u  %s = %u  %s = %u  %s = %u\n",
               PN_VALUE_DESCRIBE(arg_ids[0]), dst_p,
               PN_VALUE_DESCRIBE(arg_ids[1]), src_p,
               PN_VALUE_DESCRIBE(arg_ids[2]), len,
               PN_VALUE_DESCRIBE(arg_ids[3]), align,
               PN_VALUE_DESCRIBE(arg_ids[4]), is_volatile);
      (void)align;
      (void)is_volatile;

      if (len > 0) {
        pn_memory_check(thread->executor->memory, dst_p, len);
        pn_memory_check(thread->executor->memory, src_p, len);
        void* dst_pointer = thread->executor->memory->data + dst_p;
        void* src_pointer = thread->executor->memory->data + src_p;
        memmove(dst_pointer, src_pointer, len);
      }
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

#define PN_OPCODE_INTRINSIC_CMPXCHG(ty)                                       \
  do {                                                                        \
    PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;            \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);      \
    PN_CHECK(i->num_args == 5);                                               \
    uint32_t addr_p = pn_thread_get_value(thread, arg_ids[0]).u32;            \
    pn_##ty expected = pn_thread_get_value(thread, arg_ids[1]).ty;            \
    pn_##ty desired = pn_thread_get_value(thread, arg_ids[2]).ty;             \
    uint32_t memory_order_success =                                           \
        pn_thread_get_value(thread, arg_ids[3]).u32;                          \
    uint32_t memory_order_failure =                                           \
        pn_thread_get_value(thread, arg_ids[4]).u32;                          \
    pn_##ty read = pn_memory_read_##ty(thread->executor->memory, addr_p);     \
    PNRuntimeValue result = pn_executor_value_##ty(read);                     \
    if (read == expected) {                                                   \
      pn_memory_write_##ty(thread->executor->memory, addr_p, desired);        \
    }                                                                         \
    pn_thread_set_value(thread, i->result_value_id, result);                  \
    PN_TRACE(INTRINSICS, "    llvm.nacl.atomic.cmpxchg." #ty                  \
                         "(addr_p:%u, expected:" PN_FORMAT_##ty               \
             ", desired:" PN_FORMAT_##ty ", ...)\n",                          \
             addr_p, expected, desired);                                      \
    PN_TRACE(                                                                 \
        EXECUTE, "    %s = " PN_FORMAT_##ty "  %s = %u  %s = " PN_FORMAT_##ty \
        "  %s = " PN_FORMAT_##ty " %s = %u  %s = %u\n",                       \
        PN_VALUE_DESCRIBE(i->result_value_id), result.ty,                     \
        PN_VALUE_DESCRIBE(arg_ids[0]), addr_p, PN_VALUE_DESCRIBE(arg_ids[1]), \
        expected, PN_VALUE_DESCRIBE(arg_ids[2]), desired,                     \
        PN_VALUE_DESCRIBE(arg_ids[3]), memory_order_success,                  \
        PN_VALUE_DESCRIBE(arg_ids[4]), memory_order_failure);                 \
    (void) memory_order_success;                                              \
    (void) memory_order_failure;                                              \
    thread->inst +=                                                           \
        sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);   \
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

#define PN_OPCODE_INTRINSIC_LOAD(ty)                                        \
  do {                                                                      \
    PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;          \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);    \
    PN_CHECK(i->num_args == 2);                                             \
    uint32_t addr_p = pn_thread_get_value(thread, arg_ids[0]).u32;          \
    uint32_t flags = pn_thread_get_value(thread, arg_ids[1]).u32;           \
    pn_##ty value = pn_memory_read_##ty(thread->executor->memory, addr_p);  \
    PNRuntimeValue result = pn_executor_value_##ty(value);                  \
    pn_thread_set_value(thread, i->result_value_id, result);                \
    PN_TRACE(INTRINSICS,                                                    \
             "    llvm.nacl.atomic.load." #ty "(addr_p:%u, flags:%u)\n",    \
             addr_p, flags);                                                \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty "  %s = %u  %s = %u\n",    \
             PN_VALUE_DESCRIBE(i->result_value_id), result.ty,              \
             PN_VALUE_DESCRIBE(arg_ids[0]), addr_p,                         \
             PN_VALUE_DESCRIBE(arg_ids[1]), flags);                         \
    (void) flags;                                                           \
    thread->inst +=                                                         \
        sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId); \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I8:
      PN_OPCODE_INTRINSIC_LOAD(u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I16:
      PN_OPCODE_INTRINSIC_LOAD(u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I32:
      PN_OPCODE_INTRINSIC_LOAD(u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I64:
      PN_OPCODE_INTRINSIC_LOAD(u64);
      break;

#undef PN_OPCODE_INTRINSIC_LOAD

#define PN_OPCODE_INTRINSIC_RMW(opval, op, ty)                                 \
  do {                                                                         \
    PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;             \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);       \
    PN_CHECK(i->num_args == 4);                                                \
    PN_CHECK(pn_thread_get_value(thread, arg_ids[0]).u32 == opval);            \
    uint32_t addr_p = pn_thread_get_value(thread, arg_ids[1]).u32;             \
    pn_##ty value = pn_thread_get_value(thread, arg_ids[2]).ty;                \
    uint32_t memory_order = pn_thread_get_value(thread, arg_ids[3]).u32;       \
    pn_##ty old_value = pn_memory_read_##ty(thread->executor->memory, addr_p); \
    pn_##ty new_value = old_value op value;                                    \
    pn_memory_write_##ty(thread->executor->memory, addr_p, new_value);         \
    PNRuntimeValue result = pn_executor_value_u32(old_value);                  \
    pn_thread_set_value(thread, i->result_value_id, result);                   \
    PN_TRACE(INTRINSICS, "    llvm.nacl.atomic.rmw." #ty                       \
                         "(op: %s, addr_p:%u, value: " PN_FORMAT_##ty ")\n",   \
             #op, addr_p, value);                                              \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty                               \
             "  %s = %u  %s = %u  %s = " PN_FORMAT_##ty "  %s = %u\n",         \
             PN_VALUE_DESCRIBE(i->result_value_id), result.ty,                 \
             PN_VALUE_DESCRIBE(arg_ids[0]), opval,                             \
             PN_VALUE_DESCRIBE(arg_ids[1]), addr_p,                            \
             PN_VALUE_DESCRIBE(arg_ids[2]), value,                             \
             PN_VALUE_DESCRIBE(arg_ids[3]), memory_order);                     \
    (void) memory_order;                                                       \
    thread->inst +=                                                            \
        sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);    \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I8:
      PN_OPCODE_INTRINSIC_RMW(1, +, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I16:
      PN_OPCODE_INTRINSIC_RMW(1, +, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I32:
      PN_OPCODE_INTRINSIC_RMW(1, +, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I64:
      PN_OPCODE_INTRINSIC_RMW(1, +, u64);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I8:
      PN_OPCODE_INTRINSIC_RMW(2, -, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I16:
      PN_OPCODE_INTRINSIC_RMW(2, -, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I32:
      PN_OPCODE_INTRINSIC_RMW(2, -, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I64:
      PN_OPCODE_INTRINSIC_RMW(2, -, u64);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I8:
      PN_OPCODE_INTRINSIC_RMW(3, &, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I16:
      PN_OPCODE_INTRINSIC_RMW(3, &, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I32:
      PN_OPCODE_INTRINSIC_RMW(3, &, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I64:
      PN_OPCODE_INTRINSIC_RMW(3, &, u64);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I8:
      PN_OPCODE_INTRINSIC_RMW(4, |, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I16:
      PN_OPCODE_INTRINSIC_RMW(4, |, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I32:
      PN_OPCODE_INTRINSIC_RMW(4, |, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I64:
      PN_OPCODE_INTRINSIC_RMW(4, |, u64);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I8:
      PN_OPCODE_INTRINSIC_RMW(5, ^, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I16:
      PN_OPCODE_INTRINSIC_RMW(5, ^, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I32:
      PN_OPCODE_INTRINSIC_RMW(5, ^, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I64:
      PN_OPCODE_INTRINSIC_RMW(5, ^, u64);
      break;

#define PN_OPCODE_INTRINSIC_EXCHANGE(opval, ty)                                \
  do {                                                                         \
    PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;             \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);       \
    PN_CHECK(i->num_args == 4);                                                \
    PN_CHECK(pn_thread_get_value(thread, arg_ids[0]).u32 == opval);            \
    uint32_t addr_p = pn_thread_get_value(thread, arg_ids[1]).u32;             \
    pn_##ty value = pn_thread_get_value(thread, arg_ids[2]).ty;                \
    uint32_t memory_order = pn_thread_get_value(thread, arg_ids[3]).u32;       \
    pn_##ty old_value = pn_memory_read_##ty(thread->executor->memory, addr_p); \
    pn_##ty new_value = value;                                                 \
    pn_memory_write_##ty(thread->executor->memory, addr_p, new_value);         \
    PNRuntimeValue result = pn_executor_value_u32(old_value);                  \
    pn_thread_set_value(thread, i->result_value_id, result);                   \
    PN_TRACE(INTRINSICS, "    llvm.nacl.atomic.exchange." #ty                  \
                         "(addr_p:%u, value: " PN_FORMAT_##ty ")\n",           \
             addr_p, value);                                                   \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty                               \
             "  %s = %u  %s = %u  %s = " PN_FORMAT_##ty "  %s = %u\n",         \
             PN_VALUE_DESCRIBE(i->result_value_id), result.ty,                 \
             PN_VALUE_DESCRIBE(arg_ids[0]), opval,                             \
             PN_VALUE_DESCRIBE(arg_ids[1]), addr_p,                            \
             PN_VALUE_DESCRIBE(arg_ids[2]), value,                             \
             PN_VALUE_DESCRIBE(arg_ids[3]), memory_order);                     \
    (void) memory_order;                                                       \
    thread->inst +=                                                            \
        sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);    \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I8:
      PN_OPCODE_INTRINSIC_EXCHANGE(6, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I16:
      PN_OPCODE_INTRINSIC_EXCHANGE(6, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I32:
      PN_OPCODE_INTRINSIC_EXCHANGE(6, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I64:
      PN_OPCODE_INTRINSIC_EXCHANGE(6, u64);
      break;

#undef PN_OPCODE_INTRINSIC_RMW
#undef PN_OPCODE_INTRINSIC_EXCHANGE

    case PN_OPCODE_INTRINSIC_LLVM_NACL_LONGJMP: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 2);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t jmpbuf_p = pn_thread_get_value(thread, arg_ids[0]).u32;
      PNRuntimeValue value = pn_thread_get_value(thread, arg_ids[1]);
      PN_TRACE(INTRINSICS, "    llvm.nacl.longjmp(jmpbuf: %u, value: %u)\n",
               jmpbuf_p, value.u32);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %u\n", PN_VALUE_DESCRIBE(arg_ids[0]),
               jmpbuf_p, PN_VALUE_DESCRIBE(arg_ids[1]), value.u32);

      PNJmpBufId id = pn_memory_read_u32(thread->executor->memory, jmpbuf_p);

      /* Search the call stack for the matching jmpbuf id */
      PNCallFrame* f = thread->current_frame;
      while (f != &thread->executor->sentinel_frame) {
        PNJmpBuf* buf = f->jmpbuf_head;
        while (buf) {
          if (buf->id == id) {
            /* Found it */
            thread->current_frame = f;
            pn_allocator_reset_to_mark(&thread->allocator, f->mark);
            /* Reset the frame to its original state */
            *thread->current_frame = buf->frame;
            PNLocation* location = &thread->current_frame->location;
            PN_TRACE(EXECUTE, "function = %%f%d  pc = %%%zd\n",
                     location->function_id,
                     location->inst - function->instructions);
            /* Set the return value */
            PNRuntimeInstructionCall* c = location->inst;
            pn_thread_set_value(thread, c->result_value_id, value);
            pn_executor_value_trace(thread->executor, function, arg_ids[1],
                                    value, "    ", "\n");
            thread->inst = location->inst + sizeof(PNRuntimeInstructionCall) +
                           c->num_args * sizeof(PNValueId);
            thread->function = &module->functions[location->function_id];
            goto longjmp_done;
          }
          buf = buf->next;
        }
        f = f->parent;
      }
      PN_FATAL("Invalid jmpbuf target: %d\n", id);
    longjmp_done:
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_NACL_SETJMP: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 1);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      thread->current_frame->location.inst = thread->inst;
      uint32_t jmpbuf_p = pn_thread_get_value(thread, arg_ids[0]).u32;
      PNJmpBuf* buf = pn_allocator_alloc(&thread->allocator, sizeof(PNJmpBuf),
                                         PN_DEFAULT_ALIGN);
      buf->id = thread->executor->next_jmpbuf_id++;
      buf->frame = *thread->current_frame;
      buf->next = thread->current_frame->jmpbuf_head;
      thread->current_frame->jmpbuf_head = buf;
      pn_memory_write_u32(thread->executor->memory, jmpbuf_p, buf->id);
      PNRuntimeValue result = pn_executor_value_u32(0);
      pn_thread_set_value(thread, i->result_value_id, result);
      PN_TRACE(INTRINSICS, "    llvm.nacl.setjmp(jmpbuf: %u)\n", jmpbuf_p);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %u\n",
               PN_VALUE_DESCRIBE(i->result_value_id), result.u32,
               PN_VALUE_DESCRIBE(arg_ids[0]), jmpbuf_p);
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_STORE_I32: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 3);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t value = pn_thread_get_value(thread, arg_ids[0]).u32;
      uint32_t addr_p = pn_thread_get_value(thread, arg_ids[1]).u32;
      uint32_t flags = pn_thread_get_value(thread, arg_ids[2]).u32;
      pn_memory_write_u32(thread->executor->memory, addr_p, value);
      PN_TRACE(
          INTRINSICS,
          "    llvm.nacl.atomic.store.u32(value: %u, addr_p:%u, flags: %u)\n",
          value, addr_p, flags);
      PN_TRACE(EXECUTE, "    %s = %u  %s = %u  %s = %u\n",
               PN_VALUE_DESCRIBE(arg_ids[0]), value,
               PN_VALUE_DESCRIBE(arg_ids[1]), addr_p,
               PN_VALUE_DESCRIBE(arg_ids[2]), flags);
      (void)flags;
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_NACL_READ_TP: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PN_CHECK(i->num_args == 0);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      PNRuntimeValue result = pn_executor_value_u32(thread->tls);
      pn_thread_set_value(thread, i->result_value_id, result);
      PN_TRACE(INTRINSICS, "    llvm.nacl.read.tp()\n");
      PN_TRACE(EXECUTE, "    %s = %u\n", PN_VALUE_DESCRIBE(i->result_value_id),
               result.u32);
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_SQRT_F32: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 1);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      float value = pn_thread_get_value(thread, arg_ids[0]).f32;
      PNRuntimeValue result = pn_executor_value_f32(sqrtf(value));
      pn_thread_set_value(thread, i->result_value_id, result);
      PN_TRACE(INTRINSICS, "    llvm.sqrt.f32(%f)\n", value);
      PN_TRACE(EXECUTE, "    %s = %f  %s = %f\n",
               PN_VALUE_DESCRIBE(i->result_value_id), result.f32,
               PN_VALUE_DESCRIBE(arg_ids[0]), value);
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_SQRT_F64: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 1);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      double value = pn_thread_get_value(thread, arg_ids[0]).f64;
      PNRuntimeValue result = pn_executor_value_f64(sqrt(value));
      pn_thread_set_value(thread, i->result_value_id, result);
      PN_TRACE(INTRINSICS, "    llvm.sqrt.f64(%f)\n", value);
      PN_TRACE(EXECUTE, "    %s = %f  %s = %f\n",
               PN_VALUE_DESCRIBE(i->result_value_id), result.f64,
               PN_VALUE_DESCRIBE(arg_ids[0]), value);
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_STACKRESTORE: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 1);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t value = pn_thread_get_value(thread, arg_ids[0]).u32;
      /* TODO(binji): validate stack pointer */
      thread->current_frame->memory_stack_top = value;
      PN_TRACE(INTRINSICS, "    llvm.stackrestore(%u)\n", value);
      PN_TRACE(EXECUTE, "    %s = %u\n", PN_VALUE_DESCRIBE(arg_ids[0]), value);
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_STACKSAVE: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PN_CHECK(i->num_args == 0);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      PNRuntimeValue result =
          pn_executor_value_u32(thread->current_frame->memory_stack_top);
      pn_thread_set_value(thread, i->result_value_id, result);
      PN_TRACE(INTRINSICS, "    llvm.stacksave()\n");
      PN_TRACE(EXECUTE, "    %s = %u\n", PN_VALUE_DESCRIBE(i->result_value_id),
               result.u32);
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_TRAP: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PN_CHECK(i->num_args == 0);
      PN_TRACE(INTRINSICS, "    llvm.trap()\n");
      thread->executor->exit_code = -1;
      thread->executor->exiting = PN_TRUE;
      break;
    }

#define PN_OPCODE_INTRINSIC_STUB(name)                \
  case PN_OPCODE_INTRINSIC_##name: {                  \
    PN_FATAL("Unimplemented intrinsic: %s\n", #name); \
    break;                                            \
  }

      PN_OPCODE_INTRINSIC_STUB(LLVM_BSWAP_I16)
      PN_OPCODE_INTRINSIC_STUB(LLVM_BSWAP_I32)
      PN_OPCODE_INTRINSIC_STUB(LLVM_BSWAP_I64)
      PN_OPCODE_INTRINSIC_STUB(LLVM_CTLZ_I32)
      PN_OPCODE_INTRINSIC_STUB(LLVM_CTTZ_I32)
      PN_OPCODE_INTRINSIC_STUB(LLVM_FABS_F32)
      PN_OPCODE_INTRINSIC_STUB(LLVM_FABS_F64)
      PN_OPCODE_INTRINSIC_STUB(LLVM_NACL_ATOMIC_RMW_I8)
      PN_OPCODE_INTRINSIC_STUB(LLVM_NACL_ATOMIC_RMW_I16)
      PN_OPCODE_INTRINSIC_STUB(LLVM_NACL_ATOMIC_RMW_I32)
      PN_OPCODE_INTRINSIC_STUB(LLVM_NACL_ATOMIC_RMW_I64)
      PN_OPCODE_INTRINSIC_STUB(LLVM_NACL_ATOMIC_STORE_I8)
      PN_OPCODE_INTRINSIC_STUB(LLVM_NACL_ATOMIC_STORE_I16)
      PN_OPCODE_INTRINSIC_STUB(LLVM_NACL_ATOMIC_STORE_I64)
      PN_OPCODE_INTRINSIC_STUB(START)

#define PN_OPCODE_LOAD(ty)                                         \
  do {                                                             \
    PNRuntimeInstructionLoad* i = (PNRuntimeInstructionLoad*)inst; \
    PNRuntimeValue src = pn_thread_get_value(thread, i->src_id);   \
    PNRuntimeValue result = pn_executor_value_##ty(                \
        pn_memory_read_##ty(thread->executor->memory, src.u32));   \
    pn_thread_set_value(thread, i->result_value_id, result);       \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty "  %s = %u\n",    \
             PN_VALUE_DESCRIBE(i->result_value_id), result.ty,     \
             PN_VALUE_DESCRIBE(i->src_id), src.u32);               \
    thread->inst += sizeof(PNRuntimeInstructionLoad);              \
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
      thread->current_frame = thread->current_frame->parent;
      PNLocation* location = &thread->current_frame->location;

      if (location->function_id != PN_INVALID_FUNCTION_ID) {
        PNFunction* new_function = &module->functions[location->function_id];
        PNRuntimeInstructionCall* c = location->inst;

        pn_allocator_reset_to_mark(&thread->allocator,
                                   thread->current_frame->mark);
        PN_TRACE(EXECUTE, "function = %%f%d  pc = %%%zd\n",
                 location->function_id,
                 location->inst - new_function->instructions);
        thread->inst = location->inst + sizeof(PNRuntimeInstructionCall) +
                       c->num_args * sizeof(PNValueId);
        thread->function = new_function;
      } else {
        /* Returning nothing from _start; let's use 0 as the exit code */
        thread->executor->exit_code = 0;
        thread->executor->exiting = PN_TRUE;
        PN_TRACE(EXECUTE, "exiting\n");
      }
      break;
    }

    case PN_OPCODE_RET_VALUE: {
      PNRuntimeInstructionRetValue* i = (PNRuntimeInstructionRetValue*)inst;
      PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);

      thread->current_frame = thread->current_frame->parent;
      PNLocation* location = &thread->current_frame->location;

      if (location->function_id != PN_INVALID_FUNCTION_ID) {
        PNFunction* new_function = &module->functions[location->function_id];
        PNRuntimeInstructionCall* c = location->inst;
        pn_thread_set_value(thread, c->result_value_id, value);
        pn_executor_value_trace(thread->executor, function, i->value_id, value,
                                "    ", "\n");
        pn_allocator_reset_to_mark(&thread->allocator,
                                   thread->current_frame->mark);
        PN_TRACE(EXECUTE, "function = %%f%d  pc = %%%zd\n",
                 location->function_id,
                 location->inst - new_function->instructions);
        pn_executor_value_trace(thread->executor, new_function,
                                c->result_value_id, value, "    ", "\n");
        thread->inst = location->inst + sizeof(PNRuntimeInstructionCall) +
                       c->num_args * sizeof(PNValueId);
        thread->function = new_function;
      } else {
        /* Returning a value from _start; let's consider that the exit code */
        thread->executor->exit_code = value.i32;
        thread->executor->exiting = PN_TRUE;
        pn_executor_value_trace(thread->executor, function, i->value_id, value,
                                "    ", "\n");
        PN_TRACE(EXECUTE, "exiting\n");
      }

      break;
    }

#define PN_OPCODE_STORE(ty)                                             \
  do {                                                                  \
    PNRuntimeInstructionStore* i = (PNRuntimeInstructionStore*)inst;    \
    PNRuntimeValue dest = pn_thread_get_value(thread, i->dest_id);      \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);    \
    PN_TRACE(EXECUTE, "    %s = %u  %s = " PN_FORMAT_##ty "\n",         \
             PN_VALUE_DESCRIBE(i->dest_id), dest.u32,                   \
             PN_VALUE_DESCRIBE(i->value_id), value.ty);                 \
    pn_memory_write_##ty(thread->executor->memory, dest.u32, value.ty); \
    thread->inst += sizeof(PNRuntimeInstructionStore);                  \
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

#define PN_OPCODE_SWITCH(ty)                                              \
  do {                                                                    \
    PNRuntimeInstructionSwitch* i = (PNRuntimeInstructionSwitch*)inst;    \
    PNRuntimeSwitchCase* cases =                                          \
        (void*)inst + sizeof(PNRuntimeInstructionSwitch);                 \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);      \
    void* new_inst = i->default_inst;                                     \
    uint32_t c;                                                           \
    for (c = 0; c < i->num_cases; ++c) {                                  \
      PNRuntimeSwitchCase* switch_case = &cases[c];                       \
      if (value.ty == switch_case->value) {                               \
        new_inst = switch_case->inst;                                     \
        break;                                                            \
      }                                                                   \
    }                                                                     \
    thread->inst += sizeof(PNRuntimeInstructionSwitch) +                  \
                    i->num_cases * sizeof(PNSwitchCase);                  \
    pn_thread_do_phi_assigns(thread, function, new_inst);                 \
    thread->inst = new_inst;                                              \
    PN_TRACE(EXECUTE, "    %s = " PN_FORMAT_##ty "\n",                    \
             PN_VALUE_DESCRIBE(i->value_id), value.ty);                   \
    PN_TRACE(EXECUTE, "pc = %%%zd\n", new_inst - function->instructions); \
  } while (0) /* no semicolon */

    // clang-format off
    case PN_OPCODE_SWITCH_INT1:
    case PN_OPCODE_SWITCH_INT8:  PN_OPCODE_SWITCH(i8); break;
    case PN_OPCODE_SWITCH_INT16: PN_OPCODE_SWITCH(i16); break;
    case PN_OPCODE_SWITCH_INT32: PN_OPCODE_SWITCH(i32); break;
    case PN_OPCODE_SWITCH_INT64: PN_OPCODE_SWITCH(i64); break;
// clang-format on

#undef PN_OPCODE_SWITCH

    case PN_OPCODE_UNREACHABLE:
      PN_FATAL("Reached unreachable instruction!\n");
      break;

    case PN_OPCODE_VSELECT: {
      PNRuntimeInstructionVselect* i = (PNRuntimeInstructionVselect*)inst;
      PNRuntimeValue cond = pn_thread_get_value(thread, i->cond_id);
      PNValueId value_id = (cond.u8 & 1) ? i->true_value_id : i->false_value_id;
      PNRuntimeValue result = pn_thread_get_value(thread, value_id);
      pn_thread_set_value(thread, i->result_value_id, result);
      pn_executor_value_trace(thread->executor, function, i->result_value_id,
                              result, "    ", "  ");
      pn_executor_value_trace(thread->executor, function, i->cond_id, cond, "",
                              "  ");
      pn_executor_value_trace(thread->executor, function, value_id, result, "",
                              "\n");
      thread->inst += sizeof(PNRuntimeInstructionVselect);
      break;
    }

    default:
      PN_FATAL("Invalid opcode: %d\n", inst->opcode);
      break;
  }
}

void pn_executor_run(PNExecutor* executor) {
  PNThread* thread = &executor->main_thread;
  PNBool running = PN_TRUE;
  uint32_t last_thread_id = thread->id;
  while (running) {
    uint32_t i;
    for (i = 0; i < PN_INSTRUCTIONS_QUANTUM && running &&
                thread->state == PN_THREAD_RUNNING;
         ++i) {
      pn_thread_execute_instruction(thread);
      running = thread->current_frame != &executor->sentinel_frame &&
                !executor->exiting;
    }

    if (!running) {
      break;
    }

    /* Remove the dead thread from the executing linked-list. Only the
     * currently executing thread should be in this state. */
    PNThread* next_thread = thread->next;
    if (thread->state == PN_THREAD_DEAD) {
      assert(thread != &executor->main_thread);
      /* Unlink from executing linked list */
      thread->prev->next = thread->next;
      thread->next->prev = thread->prev;

      /* Link into dead list, singly-linked */
      thread->next = executor->dead_threads;
      thread->prev = NULL;
      executor->dead_threads = thread;
    }

    thread = next_thread;

    /* Schedule the next thread */
    while (1) {
      assert(thread->state != PN_THREAD_DEAD);

      if (thread->state == PN_THREAD_RUNNING) {
        if (thread->id != last_thread_id) {
          last_thread_id = thread->id;
          if (PN_IS_TRACE(EXECUTE) || PN_IS_TRACE(IRT) ||
              PN_IS_TRACE(INTRINSICS)) {
            PN_PRINT("Switch thread: %d\n", thread->id);
          }
        }
        break;
      } else if (thread->state == PN_THREAD_BLOCKED) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        if (thread->has_timeout && (tv.tv_sec > thread->timeout_sec ||
                                    (tv.tv_sec == thread->timeout_sec &&
                                     tv.tv_usec > thread->timeout_usec))) {
          /* Run the call instruction again from the timedout state. This
           * will set the proper return value and continue on. */
          thread->state = PN_THREAD_RUNNING;
          thread->futex_state = PN_FUTEX_TIMEDOUT;
          break;
        }

        /* TODO(binji): detect deadlock */
      } else {
        PN_UNREACHABLE();
      }

      thread = thread->next;
    }
  }
}

#undef PN_VALUE_DESCRIBE

#endif /* PN_EXECUTOR_H_ */
