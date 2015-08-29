/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_PPAPI_H_
#define PN_PPAPI_H_

#if PN_PPAPI

void pn_memory_init_ppapi(PNMemory* memory) {
  uint32_t offset = pn_align_up(memory->ppapi_start, sizeof(uint32_t));

  /* PPB_Core;1.0 */
#define PN_WRITE_BUILTIN(name)                                   \
  pn_memory_write_u32(memory, offset,                            \
                      pn_builtin_to_pointer(PN_BUILTIN_##name)); \
  offset += sizeof(uint32_t)

#define PN_WRITE_INTERFACE_NAME(name)         \
  pn_memory_write_cstr(memory, offset, name); \
  offset += strlen(name) + 1

  memory->ppapi_ppb_interfaces[PN_PPB_INTERFACE_CORE_1_0] = offset;
  PN_WRITE_BUILTIN(PPB_CORE_ADD_REF_RESOURCE);
  PN_WRITE_BUILTIN(PPB_CORE_RELEASE_RESOURCE);
  PN_WRITE_BUILTIN(PPB_CORE_GET_TIME);
  PN_WRITE_BUILTIN(PPB_CORE_GET_TIME_TICKS);
  PN_WRITE_BUILTIN(PPB_CORE_CALL_ON_MAIN_THREAD);
  PN_WRITE_BUILTIN(PPB_CORE_IS_MAIN_THREAD);

#define PN_PPP_INTERFACE(e, s)                                      \
  memory->ppapi_ppp_interface_names[PN_PPP_INTERFACE_##e] = offset; \
  PN_WRITE_INTERFACE_NAME(s);

PN_FOREACH_PPP_INTERFACES(PN_PPP_INTERFACE)

#undef PN_PPP_INTERFACE

  memory->ppapi_end = offset;
  memory->heap_start = pn_align_up(memory->ppapi_end, PN_PAGESIZE);
  memory->stack_end = memory->size;

#undef PN_WRITE_BUILTIN
}

#define PN_BUILTIN_ARG(name, n, ty)                                  \
  PNRuntimeValue value##n = pn_thread_get_value(thread, arg_ids[n]); \
  pn_##ty name = value##n.ty;                                        \
  (void) name /* no semicolon */

static PNRuntimeValue pn_builtin_PPB_GET_INTERFACE(PNThread* thread,
                                                   PNFunction* function,
                                                   uint32_t num_args,
                                                   PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(iface_name_p, 0, u32);

  PNMemory* memory = executor->memory;
  pn_memory_check(memory, iface_name_p, 1);

  uint32_t name_len = pn_memory_check_cstr(executor->memory, iface_name_p);
  PN_CHECK(name_len > 0);

  const char* iface_name = memory->data + iface_name_p;
  PN_TRACE(PPAPI, "    PPB_GET_INTERFACE(%u (%s))\n", iface_name_p, iface_name);

#define PN_PPB_INTERFACE(e, s)                                         \
  if (strcmp(iface_name, s) == 0) {                                    \
    return pn_executor_value_u32(                                      \
        executor->memory->ppapi_ppb_interfaces[PN_PPB_INTERFACE_##e]); \
  } else

  PN_FOREACH_PPB_INTERFACES(PN_PPB_INTERFACE)
  {
    PN_TRACE(PPAPI, "Unknown pepper interface name: \"%s\".\n", iface_name);
    return pn_executor_value_u32(0);
  }

#undef PN_PPB_INTERFACE
}

static PNRuntimeValue pn_builtin_PPB_CORE_ADD_REF_RESOURCE(PNThread* thread,
                                                           PNFunction* function,
                                                           uint32_t num_args,
                                                           PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(resource, 0, i32);
  (void)executor;
  PN_TRACE(PPAPI, "    PPB_CORE_ADD_REF_RESOURCE(%d)\n", resource);
  return pn_executor_value_u32(0); /* ignored */
}

static PNRuntimeValue pn_builtin_PPB_CORE_RELEASE_RESOURCE(PNThread* thread,
                                                           PNFunction* function,
                                                           uint32_t num_args,
                                                           PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(resource, 0, i32);
  (void)executor;
  PN_TRACE(PPAPI, "    PPB_CORE_RELEASE_RESOURCE(%d)\n", resource);
  return pn_executor_value_u32(0); /* ignored */
}

static PNRuntimeValue pn_builtin_PPB_CORE_GET_TIME(PNThread* thread,
                                                   PNFunction* function,
                                                   uint32_t num_args,
                                                   PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 0);
  (void)executor;
  PN_TRACE(PPAPI, "    PPB_CORE_GET_TIME()\n");
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_PPB_CORE_GET_TIME_TICKS(PNThread* thread,
                                                         PNFunction* function,
                                                         uint32_t num_args,
                                                         PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 0);
  (void)executor;
  PN_TRACE(PPAPI, "    PPB_CORE_GET_TIME_TICKS()\n");
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_PPB_CORE_CALL_ON_MAIN_THREAD(
    PNThread* thread,
    PNFunction* function,
    uint32_t num_args,
    PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 3);
  PN_BUILTIN_ARG(delay_in_ms, 0, i32);
  PN_BUILTIN_ARG(callback_p, 1, u32);
  PN_BUILTIN_ARG(result, 2, i32);
  (void)executor;
  PN_TRACE(PPAPI, "    PPB_CORE_CALL_ON_MAIN_THREAD(%dms, %u, %d)\n",
           delay_in_ms, callback_p, result);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_PPB_CORE_IS_MAIN_THREAD(PNThread* thread,
                                                         PNFunction* function,
                                                         uint32_t num_args,
                                                         PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 0);
  (void)executor;
  PN_TRACE(PPAPI, "    PPB_CORE_IS_MAIN_THREAD()\n");
  return pn_executor_value_u32(0);
}

#undef PN_BUILTIN_ARG

static PNEvent* pn_event_allocate(PNPpapi* ppapi) {
  if (ppapi->free_events) {
    PNEvent* result = ppapi->free_events;
    ppapi->free_events = result->next;
    return result;
  }

  return pn_allocator_alloc(&ppapi->allocator, sizeof(PNEvent),
                            PN_DEFAULT_ALIGN);
}

static void pn_event_enqueue(PNPpapi* ppapi, PNEvent* event) {
  event->next = &ppapi->sentinel_event;
  event->prev = ppapi->sentinel_event.prev;
  ppapi->sentinel_event.prev->next = event;
  ppapi->sentinel_event.prev = event;
}

static PNEvent* pn_event_dequeue(PNPpapi* ppapi) {
  PNEvent* result = ppapi->sentinel_event.next;
  if (result == &ppapi->sentinel_event) {
    return NULL;
  }

  result->prev->next = result->next;
  result->next->prev = result->prev;
  result->prev = NULL;
  result->next = NULL;
  return result;
}

static void pn_event_free(PNPpapi* ppapi, PNEvent* event) {
  event->next = ppapi->free_events;
  ppapi->free_events = event;
}

static uint32_t pn_ppapi_get_interface(PNThread* thread,
                                       PNPppInterfaceId interface) {
  PNExecutor* executor = thread->executor;
  uint32_t result = executor->memory->ppapi_ppp_interfaces[interface];
  if (result == PN_INVALID_INTERFACE_TABLE) {
    PNEvent* event = pn_event_allocate(&executor->ppapi);
    event->type = PN_EVENT_TYPE_GET_INTERFACE;
    event->interface = interface;
    event->out_iface_p = &executor->memory->ppapi_ppp_interfaces[interface];
    pn_event_enqueue(&executor->ppapi, event);
  }

  return result;
}

static void pn_event_handle_next(PNThread* thread) {
  PNExecutor* executor = thread->executor;
  PNPpapi* ppapi = &executor->ppapi;
  PNEvent* event = pn_event_dequeue(&executor->ppapi);
  ppapi->current_event = NULL;

  if (event == NULL) {
    return;
  }

  switch (event->type) {
    case PN_EVENT_TYPE_GET_INTERFACE: {
      PN_TRACE(PPAPI, "    PN_EVENT_TYPE_GET_INTERFACE(%d, %p)\n",
               event->interface, event->out_iface_p);
      PNPppInterfaceId interface = event->interface;
      pn_thread_push_function_pointer(thread, ppapi->get_interface_func);
      uint32_t iface_name =
          executor->memory->ppapi_ppp_interface_names[interface];
      pn_thread_set_param_value(thread, 0, pn_executor_value_u32(iface_name));
      thread->state = PN_THREAD_RUNNING;
      ppapi->current_event = event;
      break;
    }

    case PN_EVENT_TYPE_DID_CREATE: {
      PN_TRACE(PPAPI, "    PN_EVENT_TYPE_DID_CREATE()\n");
      uint32_t instance_iface =
          pn_ppapi_get_interface(thread, PN_PPP_INTERFACE_INSTANCE_1_1);
      if (instance_iface == PN_INVALID_INTERFACE_TABLE) {
        pn_event_enqueue(ppapi, event);
        break;
      }

      if (instance_iface == 0) {
        instance_iface =
            pn_ppapi_get_interface(thread, PN_PPP_INTERFACE_INSTANCE_1_0);
        if (instance_iface == PN_INVALID_INTERFACE_TABLE) {
          pn_event_enqueue(ppapi, event);
          break;
        } else if (instance_iface == 0) {
          PN_FATAL("no version of PPP_INSTANCE found.\n");
        }
      }

      /* Found one. The DidCreate function pointer is always at offset 0. */
      uint32_t did_create_func =
          pn_memory_read_u32(executor->memory, instance_iface + 0);
      PNFunction* new_function =
          pn_thread_push_function_pointer(thread, did_create_func);
      PN_CHECK(new_function->num_args == 4);
      /* 0: PNInstance instance */
      pn_thread_set_param_value(thread, 0,
                                pn_executor_value_u32(PN_INSTANCE_ID));
      /* 1: uint32_t argc */
      pn_thread_set_param_value(thread, 1, pn_executor_value_u32(0));
      /* 2: const char* argn[] */
      pn_thread_set_param_value(thread, 2, pn_executor_value_u32(0));
      /* 3: const char* argv[] */
      pn_thread_set_param_value(thread, 3, pn_executor_value_u32(0));
      thread->state = PN_THREAD_RUNNING;
      ppapi->current_event = event;
      break;
    }

    case PN_EVENT_TYPE_DID_CHANGE_VIEW: {
      PN_TRACE(PPAPI, "    PN_EVENT_TYPE_DID_CHANGE_VIEW()\n");
      break;
    }

    default:
      PN_FATAL("unknown event: %d\n", event->type);
      break;
  }
}

static void pn_event_finish(PNThread* thread) {
  PNPpapi* ppapi = &thread->executor->ppapi;
  PNEvent* event = ppapi->current_event;
  if (!event) {
    return;
  }

  switch (event->type) {
    case PN_EVENT_TYPE_GET_INTERFACE: {
      PN_TRACE(PPAPI, "    PN_EVENT_TYPE_GET_INTERFACE finished => %u\n",
               thread->exit_value.u32);
      *event->out_iface_p = thread->exit_value.u32;
      break;
    }
    default: break;
  }

  pn_event_free(ppapi, event);
}

#endif /* PN_PPAPI */

#endif /* PN_PPAPI_H_ */
