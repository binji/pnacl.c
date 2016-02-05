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

static void pn_thread_set_param_value(PNThread* thread,
                                      uint32_t index,
                                      PNRuntimeValue value) {
  thread->current_frame->function_values[index] = value;
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
        PNGlobalVar* global_var = &module->global_vars[value->index];
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
  if (thread->inst == NULL) {
    PN_CHECK(function->name);
    PN_FATAL("Unimplemented intrinsic: %s\n", function->name);
  }

  uint32_t n;
  for (n = 0; n < function->num_constants; ++n) {
    PNConstant* constant = &function->constants[n];
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

static PNFunction* pn_thread_push_function_pointer(PNThread* thread,
                                                   uint32_t func) {
  PNFunctionId function_id = pn_function_pointer_to_index(func);
  function_id -= PN_MAX_BUILTINS;
  pn_function_id_check(thread->module, function_id);
  PNFunction* function = &thread->module->functions[function_id];
  pn_thread_push_function(thread, function_id, function);
  return function;
}

static void pn_thread_do_phi_assigns(PNThread* thread,
                                     PNFunction* function,
                                     void* dest_inst) {
  void* istream = thread->inst;
  uint16_t num_phi_assigns = *(uint16_t*)istream;
  istream += sizeof(uint16_t);
  uint16_t fast_phi_assign = *(uint16_t*)istream;
  istream += sizeof(uint16_t);
  PNRuntimePhiAssign* phi_assigns = (PNRuntimePhiAssign*)istream;
  istream += num_phi_assigns * sizeof(PNRuntimePhiAssign);

  PNModule* module = thread->executor->module;
  (void)module;

  if (fast_phi_assign) {
    uint32_t i;
    for (i = 0; i < num_phi_assigns; ++i) {
      PNRuntimePhiAssign* assign = &phi_assigns[i];
      if (assign->inst == dest_inst) {
        PNRuntimeValue value =
            pn_thread_get_value(thread, phi_assigns[i].source_value_id);
        pn_thread_set_value(thread, assign->dest_value_id, value);
      }
    }
  } else {
    PNAllocatorMark mark = pn_allocator_mark(&thread->allocator);
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
      }
    }

    pn_allocator_reset_to_mark(&thread->allocator, mark);
  }
}

void pn_executor_init(PNExecutor* executor, PNModule* module) {
  memset(executor, 0, sizeof(PNExecutor));
  pn_allocator_init(&executor->allocator, PN_MIN_CHUNKSIZE, "executor");
  executor->module = module;
  executor->memory = module->memory;
  executor->heap_end = executor->memory->heap_start;
  executor->sentinel_frame.location.function_id = PN_INVALID_FUNCTION_ID;
  executor->sentinel_frame.memory_stack_top = executor->memory->stack_end;
  executor->sentinel_frame.mark = pn_allocator_mark(&executor->allocator);
  executor->main_thread = &executor->start_thread;

  PN_CHECK(pn_is_aligned(executor->memory->size, PN_PAGESIZE));
  PN_CHECK(pn_is_aligned(executor->memory->heap_start, PN_PAGESIZE));
  size_t pages = executor->memory->size >> PN_PAGESHIFT;
  pn_bitset_init(&executor->allocator, &executor->mapped_pages, pages);
  size_t start_pages = executor->memory->heap_start >> PN_PAGESHIFT;
  int i;
  for (i = 0; i < start_pages; ++i) {
    pn_bitset_set(&executor->mapped_pages, i, PN_TRUE);
  }

#if PN_PPAPI
  pn_allocator_init(&executor->ppapi.allocator, PN_MIN_CHUNKSIZE, "ppapi");
  executor->ppapi.sentinel_event.next = &executor->ppapi.sentinel_event;
  executor->ppapi.sentinel_event.prev = &executor->ppapi.sentinel_event;
  /* Initialize to 0xffffffff, this value signifies that the interface must be
   * queried first. After the query, the value will */
  memset(executor->memory->ppapi_ppp_interfaces, 0xff,
         sizeof(executor->memory->ppapi_ppp_interfaces));
#endif /* PN_PPAPI */

  pn_executor_init_module_values(executor);

  PNThread* thread = &executor->start_thread;
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
  PNFunction* start_function = &module->functions[start_function_id];
  PN_CHECK(start_function->instructions);

  pn_thread_push_function(thread, start_function_id, start_function);

  PN_CHECK(start_function->num_args == 1);

  PNValueId value_id = executor->module->num_values;
  PNRuntimeValue value;
  value.u32 = executor->memory->startinfo_start;
  pn_thread_set_value(thread, value_id, value);
}

#if PN_TRACING
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
static void pn_thread_backtrace(PNThread* thread) {}
#endif

static void pn_thread_execute_instruction(PNThread* thread) {
  PNModule* module = thread->module;
  PNFunction* function = thread->function;
  PNRuntimeInstruction* inst = thread->inst;

  g_pn_opcode_count[inst->opcode]++;

  switch (inst->opcode) {
    case PN_OPCODE_ALLOCA_INT32: {
      PNRuntimeInstructionAlloca* i = (PNRuntimeInstructionAlloca*)inst;
      PNRuntimeValue size = pn_thread_get_value(thread, i->size_id);
      thread->current_frame->memory_stack_top = pn_align_down(
          thread->current_frame->memory_stack_top - size.i32, i->alignment);
      if (thread == thread->executor->main_thread &&
          thread->current_frame->memory_stack_top <
              thread->executor->heap_end) {
        PN_FATAL("Out of stack\n");
        break;
      }
      PNRuntimeValue result;
      result.u32 = thread->current_frame->memory_stack_top;
      pn_thread_set_value(thread, i->result_value_id, result);
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
      break;
    }

    case PN_OPCODE_BR_INT1: {
      PNRuntimeInstructionBrInt1* i = (PNRuntimeInstructionBrInt1*)inst;
      PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);
      void* new_inst = value.u8 ? i->true_inst : i->false_inst;
      thread->inst += sizeof(PNRuntimeInstructionBrInt1);
      pn_thread_do_phi_assigns(thread, function, new_inst);
      thread->inst = new_inst;
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
           * instruction counter.
           */
          if (thread->state == PN_THREAD_RUNNING
#if PN_PPAPI
              /* Also don't increment the instruction counter when calling ppapi
               * start; this will actually push a new function.*/
              && callee_function_id != PN_BUILTIN_NACL_IRT_PPAPIHOOK_PPAPI_START
#endif /* PN_PPAPI */
              ) {
            thread->inst += sizeof(PNRuntimeInstructionCall) +
                            i->num_args * sizeof(PNValueId);
          }
          break;
        } else {
          new_function_id = callee_function_id - PN_MAX_BUILTINS;
          assert(new_function_id < module->num_functions);
        }
      } else {
        PNValue* function_value = &module->values[i->callee_id];
        assert(function_value->code == PN_VALUE_CODE_FUNCTION);
        new_function_id = function_value->index;
      }

      PNFunction* new_function = &module->functions[new_function_id];
      pn_thread_push_function(thread, new_function_id, new_function);

      uint32_t n;
      for (n = 0; n < i->num_args; ++n) {
        PNValueId value_id = module->num_values + n;
        PNRuntimeValue arg = pn_executor_get_value_from_frame(
            thread->executor, old_frame, arg_ids[n]);
        pn_thread_set_value(thread, value_id, arg);
      }
      break;
    }

    case PN_OPCODE_CAST_BITCAST_DOUBLE_INT64:
    case PN_OPCODE_CAST_BITCAST_FLOAT_INT32:
    case PN_OPCODE_CAST_BITCAST_INT32_FLOAT:
    case PN_OPCODE_CAST_BITCAST_INT64_DOUBLE: {
      PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;
      PNRuntimeValue result = pn_thread_get_value(thread, i->value_id);
      pn_thread_set_value(thread, i->result_value_id, result);
      thread->inst += sizeof(PNRuntimeInstructionCast);
      break;
    }

#define PN_OPCODE_CAST(from, to)                                     \
  do {                                                               \
    PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;   \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id); \
    PNRuntimeValue result = pn_executor_value_##to(value.from);      \
    pn_thread_set_value(thread, i->result_value_id, result);         \
    thread->inst += sizeof(PNRuntimeInstructionCast);                \
  } while (0) /* no semicolon */

#define PN_OPCODE_CAST_SEXT1(size)                                   \
  do {                                                               \
    PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;   \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id); \
    PNRuntimeValue result =                                          \
        pn_executor_value_i##size(-(int##size##_t)(value.u8 & 1));   \
    pn_thread_set_value(thread, i->result_value_id, result);         \
    thread->inst += sizeof(PNRuntimeInstructionCast);                \
  } while (0) /* no semicolon */

#define PN_OPCODE_CAST_TRUNC1(size)                                  \
  do {                                                               \
    PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;   \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id); \
    PNRuntimeValue result = pn_executor_value_u8(value.u##size & 1); \
    pn_thread_set_value(thread, i->result_value_id, result);         \
    thread->inst += sizeof(PNRuntimeInstructionCast);                \
  } while (0) /* no semicolon */

#define PN_OPCODE_CAST_ZEXT1(size)                                   \
  do {                                                               \
    PNRuntimeInstructionCast* i = (PNRuntimeInstructionCast*)inst;   \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id); \
    PNRuntimeValue result =                                          \
        pn_executor_value_u##size((uint##size##_t)(value.u8 & 1));   \
    pn_thread_set_value(thread, i->result_value_id, result);         \
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
    thread->inst += sizeof(PNRuntimeInstructionCmp2);                     \
  } while (0) /* no semicolon */

#define PN_OPCODE_CMP2_NOT(op, ty)                                           \
  do {                                                                       \
    PNRuntimeInstructionCmp2* i = (PNRuntimeInstructionCmp2*)inst;           \
    PNRuntimeValue value0 = pn_thread_get_value(thread, i->value0_id);       \
    PNRuntimeValue value1 = pn_thread_get_value(thread, i->value1_id);       \
    PNRuntimeValue result = pn_executor_value_u8(!(value0.ty op value1.ty)); \
    pn_thread_set_value(thread, i->result_value_id, result);                 \
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

#define PN_ARG(i, ty) pn_thread_get_value(thread, arg_ids[i]).ty

    case PN_OPCODE_INTRINSIC_LLVM_MEMCPY: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 5);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t dst_p = PN_ARG(0, u32);
      uint32_t src_p = PN_ARG(1, u32);
      uint32_t len = PN_ARG(2, u32);

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
      uint32_t dst_p = PN_ARG(0, u32);
      uint8_t value = PN_ARG(1, u8);
      uint32_t len = PN_ARG(2, u32);

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
      uint32_t dst_p = PN_ARG(0, u32);
      uint32_t src_p = PN_ARG(1, u32);
      uint32_t len = PN_ARG(2, u32);

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

#define PN_OPCODE_INTRINSIC_CMPXCHG(ty)                                     \
  do {                                                                      \
    PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;          \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);    \
    PN_CHECK(i->num_args == 5);                                             \
    uint32_t addr_p = PN_ARG(0, u32);                                       \
    pn_##ty expected = PN_ARG(1, ty);                                       \
    pn_##ty desired = PN_ARG(2, ty);                                        \
    pn_##ty read = pn_memory_read_##ty(thread->executor->memory, addr_p);   \
    PNRuntimeValue result = pn_executor_value_##ty(read);                   \
    if (read == expected) {                                                 \
      pn_memory_write_##ty(thread->executor->memory, addr_p, desired);      \
    }                                                                       \
    pn_thread_set_value(thread, i->result_value_id, result);                \
    thread->inst +=                                                         \
        sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId); \
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

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_FENCE_ALL: {
      /* Do nothing. */
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

#define PN_OPCODE_INTRINSIC_LOAD(ty)                                        \
  do {                                                                      \
    PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;          \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);    \
    PN_CHECK(i->num_args == 2);                                             \
    uint32_t addr_p = PN_ARG(0, u32);                                       \
    pn_##ty value = pn_memory_read_##ty(thread->executor->memory, addr_p);  \
    PNRuntimeValue result = pn_executor_value_##ty(value);                  \
    pn_thread_set_value(thread, i->result_value_id, result);                \
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
    PN_CHECK(PN_ARG(0, u32) == opval);                                         \
    uint32_t addr_p = PN_ARG(1, u32);                                          \
    pn_##ty value = PN_ARG(2, ty);                                             \
    pn_##ty old_value = pn_memory_read_##ty(thread->executor->memory, addr_p); \
    pn_##ty new_value = old_value op value;                                    \
    pn_memory_write_##ty(thread->executor->memory, addr_p, new_value);         \
    PNRuntimeValue result = pn_executor_value_u32(old_value);                  \
    pn_thread_set_value(thread, i->result_value_id, result);                   \
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
    PN_CHECK(PN_ARG(0, u32) == opval);                                         \
    uint32_t addr_p = PN_ARG(1, u32);                                          \
    pn_##ty value = PN_ARG(2, ty);                                             \
    pn_##ty old_value = pn_memory_read_##ty(thread->executor->memory, addr_p); \
    pn_##ty new_value = value;                                                 \
    pn_memory_write_##ty(thread->executor->memory, addr_p, new_value);         \
    PNRuntimeValue result = pn_executor_value_u32(old_value);                  \
    pn_thread_set_value(thread, i->result_value_id, result);                   \
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
      uint32_t jmpbuf_p = PN_ARG(0, u32);
      PNRuntimeValue value = pn_thread_get_value(thread, arg_ids[1]);

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
            /* Set the return value */
            PNRuntimeInstructionCall* c = location->inst;
            pn_thread_set_value(thread, c->result_value_id, value);
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
      uint32_t jmpbuf_p = PN_ARG(0, u32);
      PNJmpBuf* buf = pn_allocator_alloc(&thread->allocator, sizeof(PNJmpBuf),
                                         PN_DEFAULT_ALIGN);
      buf->id = thread->executor->next_jmpbuf_id++;
      buf->frame = *thread->current_frame;
      buf->next = thread->current_frame->jmpbuf_head;
      thread->current_frame->jmpbuf_head = buf;
      pn_memory_write_u32(thread->executor->memory, jmpbuf_p, buf->id);
      PNRuntimeValue result = pn_executor_value_u32(0);
      pn_thread_set_value(thread, i->result_value_id, result);
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

#define PN_OPCODE_INTRINSIC_STORE(ty)                                       \
  do {                                                                      \
    PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;          \
    PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);    \
    PN_CHECK(i->num_args == 3);                                             \
    PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);                    \
    uint32_t value = PN_ARG(0, ty);                                         \
    uint32_t addr_p = PN_ARG(1, u32);                                       \
    pn_memory_write_u32(thread->executor->memory, addr_p, value);           \
    thread->inst +=                                                         \
        sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId); \
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
      PN_CHECK(i->num_args == 0);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      PNRuntimeValue result = pn_executor_value_u32(thread->tls);
      pn_thread_set_value(thread, i->result_value_id, result);
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_SQRT_F32: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 1);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      float value = PN_ARG(0, f32);
      PNRuntimeValue result = pn_executor_value_f32(sqrtf(value));
      pn_thread_set_value(thread, i->result_value_id, result);
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_SQRT_F64: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 1);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      double value = PN_ARG(0, f64);
      PNRuntimeValue result = pn_executor_value_f64(sqrt(value));
      pn_thread_set_value(thread, i->result_value_id, result);
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_STACKRESTORE: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PNValueId* arg_ids = (void*)inst + sizeof(PNRuntimeInstructionCall);
      PN_CHECK(i->num_args == 1);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t value = PN_ARG(0, u32);
      /* TODO(binji): validate stack pointer */
      thread->current_frame->memory_stack_top = value;
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
      thread->inst +=
          sizeof(PNRuntimeInstructionCall) + i->num_args * sizeof(PNValueId);
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_TRAP: {
      PNRuntimeInstructionCall* i = (PNRuntimeInstructionCall*)inst;
      PN_CHECK(i->num_args == 0);
      thread->executor->exit_code = -1;
      thread->executor->exiting = PN_TRUE;
      thread->state = PN_THREAD_DEAD;
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
      PN_OPCODE_INTRINSIC_STUB(START)

#undef PN_ARG

#define PN_OPCODE_LOAD(ty)                                         \
  do {                                                             \
    PNRuntimeInstructionLoad* i = (PNRuntimeInstructionLoad*)inst; \
    PNRuntimeValue src = pn_thread_get_value(thread, i->src_id);   \
    PNRuntimeValue result = pn_executor_value_##ty(                \
        pn_memory_read_##ty(thread->executor->memory, src.u32));   \
    pn_thread_set_value(thread, i->result_value_id, result);       \
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
        thread->inst = location->inst + sizeof(PNRuntimeInstructionCall) +
                       c->num_args * sizeof(PNValueId);
        thread->function = new_function;
      } else {
        /* Returning from the top frame of a thread. This shouldn't happen in
         * most cases; the main thread should be exited by calling
         * NACL_IRT_BASIC_EXIT, and a thread should be exited by calling
         * NACL_IRT_THREAD_EXIT. In either case, there is nothing left to run
         * on this thread, so it should finish. */
        thread->state = PN_THREAD_DEAD;
        if (thread == &thread->executor->start_thread) {
          thread->executor->exit_code = 0;
          thread->executor->exiting = PN_TRUE;
        }
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
        pn_allocator_reset_to_mark(&thread->allocator,
                                   thread->current_frame->mark);
        thread->inst = location->inst + sizeof(PNRuntimeInstructionCall) +
                       c->num_args * sizeof(PNValueId);
        thread->function = new_function;
      } else {
        /* See comment in PN_OPCODE_RET. */
        thread->state = PN_THREAD_DEAD;
        if (thread == &thread->executor->start_thread) {
          thread->executor->exit_code = value.i32;
          thread->executor->exiting = PN_TRUE;
        }
#if PN_PPAPI
        else {
          /* Return value from an event being processed in a PPAPI app */
          thread->exit_value = value;
        }
#endif /* PN_PPAPI */
      }

      break;
    }

#define PN_OPCODE_STORE(ty)                                             \
  do {                                                                  \
    PNRuntimeInstructionStore* i = (PNRuntimeInstructionStore*)inst;    \
    PNRuntimeValue dest = pn_thread_get_value(thread, i->dest_id);      \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);    \
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

#define PN_OPCODE_SWITCH(ty)                                           \
  do {                                                                 \
    PNRuntimeInstructionSwitch* i = (PNRuntimeInstructionSwitch*)inst; \
    PNRuntimeSwitchCase* cases =                                       \
        (void*)inst + sizeof(PNRuntimeInstructionSwitch);              \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);   \
    void* new_inst = i->default_inst;                                  \
    uint32_t c;                                                        \
    for (c = 0; c < i->num_cases; ++c) {                               \
      PNRuntimeSwitchCase* switch_case = &cases[c];                    \
      if (value.ty == switch_case->value) {                            \
        new_inst = switch_case->inst;                                  \
        break;                                                         \
      }                                                                \
    }                                                                  \
    thread->inst += sizeof(PNRuntimeInstructionSwitch) +               \
                    i->num_cases * sizeof(PNSwitchCase);               \
    pn_thread_do_phi_assigns(thread, function, new_inst);              \
    thread->inst = new_inst;                                           \
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
      thread->inst += sizeof(PNRuntimeInstructionVselect);
      break;
    }

    default:
      PN_FATAL("Invalid opcode: %d\n", inst->opcode);
      break;
  }
}

#if PN_PPAPI
static void pn_event_finish(PNThread* thread);
static void pn_event_handle_next(PNThread* thread);
#endif /* PN_PPAPI */

PNThread* pn_executor_run_step(PNExecutor* executor, PNThread* thread) {
  uint32_t i;

#define PN_FOR_THREAD_QUANTUM \
  for (i = 0;                 \
       i < PN_INSTRUCTIONS_QUANTUM && thread->state == PN_THREAD_RUNNING; ++i)

#if PN_TRACING
  if (PN_IS_TRACE(EXECUTE)) {
    PN_FOR_THREAD_QUANTUM {
      PNFunction* function = thread->function;
      PNCallFrame* frame = thread->current_frame;
      PNRuntimeInstruction* inst = thread->inst;
      g_pn_trace_indent += 2;
      pn_runtime_instruction_trace(thread->module, function, inst);
      g_pn_trace_indent -= 2;
      pn_thread_execute_instruction(thread);
      pn_runtime_instruction_trace_intrinsics(thread, inst);
      pn_runtime_instruction_trace_values(thread, function, frame, inst);
    }
  } else if (PN_IS_TRACE(INTRINSICS)) {
    PN_FOR_THREAD_QUANTUM {
      PNRuntimeInstruction* inst = thread->inst;
      pn_thread_execute_instruction(thread);
      pn_runtime_instruction_trace_intrinsics(thread, inst);
    }
  } else
#endif /* PN_TRACING */
  {
    PN_FOR_THREAD_QUANTUM { pn_thread_execute_instruction(thread); }
  }

  if (executor->exiting) {
    return NULL;
  }

  /* Remove the dead thread from the executing linked-list. Only the
   * currently executing thread should be in this state. */
  PNThread* next_thread = thread->next;
  if (thread->state == PN_THREAD_DEAD
#if PN_PPAPI
      || thread->state == PN_THREAD_IDLE
#endif /* PN_PPAPI */
      ) {
    assert(thread != &executor->start_thread);

#if PN_PPAPI
    /* The start thread should never be dead (the program should have exited
     * above in that case). The main thread can be dead, though. This will
     * happen if the ppapi event loop has started and the currently running
     * event has been handled. At this point the next event should be
     * handled. */
    if (thread == executor->main_thread) {
      assert(executor->start_thread.state == PN_THREAD_EVENT_LOOP);
      pn_event_finish(thread);
      pn_event_handle_next(thread);
      if (thread->state == PN_THREAD_DEAD) {
        thread->state = PN_THREAD_IDLE;
      }
    } else
#endif /* PN_PPAPI */
    {
      /* Unlink from executing linked list */
      thread->prev->next = thread->next;
      thread->next->prev = thread->prev;

      /* Link into dead list, singly-linked */
      thread->next = executor->dead_threads;
      thread->prev = NULL;
      executor->dead_threads = thread;
    }
  }

  thread = next_thread;

  /* Schedule the next thread */
  while (1) {
    assert(thread->state != PN_THREAD_DEAD);

    if (thread->state == PN_THREAD_RUNNING) {
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
#if PN_PPAPI
    } else if (thread->state == PN_THREAD_IDLE) {
      break;
#endif /* PN_PPAPI */
    } else {
      PN_UNREACHABLE();
    }

    thread = thread->next;
  }

  return thread;
}

void pn_executor_run(PNExecutor* executor) {
  PNThread* thread = executor->main_thread;
  uint32_t last_thread_id = thread->id;
  while (PN_TRUE) {
    thread = pn_executor_run_step(executor, thread);
    if (!thread) {
      break;
    }
    if (thread->id != last_thread_id) {
      last_thread_id = thread->id;
      if (PN_IS_TRACE(EXECUTE) || PN_IS_TRACE(IRT) || PN_IS_TRACE(INTRINSICS)) {
        PN_PRINT("Switch thread: %d\n", thread->id);
      }
    }
  }
}

#undef PN_VALUE_DESCRIBE

#endif /* PN_EXECUTOR_H_ */
