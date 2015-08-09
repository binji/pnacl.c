/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_MEMORY_H_
#define PN_MEMORY_H_

static uint32_t pn_builtin_to_pointer(PNBuiltinId builtin_id) {
  return builtin_id << 2;
}

static uint32_t pn_function_id_to_pointer(PNFunctionId function_id) {
  return (function_id + PN_MAX_BUILTINS) << 2;
}

static PNFunctionId pn_function_pointer_to_index(uint32_t fp) {
  return fp >> 2;
}

static void pn_memory_check(PNMemory* memory, uint32_t offset, uint32_t size) {
  if (offset < PN_MEMORY_GUARD_SIZE) {
    PN_FATAL("memory access out of bounds: %u < %u\n", offset,
             PN_MEMORY_GUARD_SIZE);
  }

  if (offset + size > memory->size) {
    PN_FATAL("memory-size is too small (%u < %u).\n", memory->size,
             offset + size);
  }
}

static void pn_memory_check_pointer(PNMemory* memory, void* p, uint32_t size) {
  pn_memory_check(memory, p - memory->data, size);
}

#define PN_FORMAT_f64 "%f"
#define PN_FORMAT_f32 "%f"
#define PN_FORMAT_u8 "%u"
#define PN_FORMAT_u16 "%u"
#define PN_FORMAT_u32 "%u"
#define PN_FORMAT_u64 "%" PRIu64
#define PN_FORMAT_i8 "%d"
#define PN_FORMAT_i16 "%d"
#define PN_FORMAT_i32 "%d"
#define PN_FORMAT_i64 "%" PRId64

#define PN_DEFINE_MEMORY_READ(ty, ctype)                                \
  static ctype pn_memory_read_##ty(PNMemory* memory, uint32_t offset) { \
    pn_memory_check(memory, offset, sizeof(ctype));                     \
    ctype* m = (ctype*)(memory->data + offset);                         \
    PN_TRACE(MEMORY, "     read." #ty " [%8u] >= " PN_FORMAT_##ty "\n", \
             offset, *m);                                               \
    if (pn_is_aligned_pointer(m, sizeof(ctype))) {                      \
      return *m;                                                        \
    } else {                                                            \
      ctype ret;                                                        \
      memcpy(&ret, m, sizeof(ctype));                                   \
      return ret;                                                       \
    }                                                                   \
  }

#define PN_DEFINE_MEMORY_WRITE(ty, ctype)                               \
  static void pn_memory_write_##ty(PNMemory* memory, uint32_t offset,   \
                                   ctype value) {                       \
    pn_memory_check(memory, offset, sizeof(ctype));                     \
    ctype* m = (ctype*)(memory->data + offset);                         \
    PN_TRACE(MEMORY, "    write." #ty " [%8u] <= " PN_FORMAT_##ty "\n", \
             offset, value);                                            \
    if (pn_is_aligned_pointer(m, sizeof(ctype))) {                      \
      *m = value;                                                       \
    } else {                                                            \
      memcpy(m, &value, sizeof(ctype));                                 \
    }                                                                   \
  }

PN_DEFINE_MEMORY_READ(i8, int8_t)
PN_DEFINE_MEMORY_READ(u8, uint8_t)
PN_DEFINE_MEMORY_READ(i16, int16_t)
PN_DEFINE_MEMORY_READ(u16, uint16_t)
PN_DEFINE_MEMORY_READ(i32, int32_t)
PN_DEFINE_MEMORY_READ(u32, uint32_t)
PN_DEFINE_MEMORY_READ(i64, int64_t)
PN_DEFINE_MEMORY_READ(u64, uint64_t)
PN_DEFINE_MEMORY_READ(f32, float)
PN_DEFINE_MEMORY_READ(f64, double)

PN_DEFINE_MEMORY_WRITE(i8, int8_t)
PN_DEFINE_MEMORY_WRITE(u8, uint8_t)
PN_DEFINE_MEMORY_WRITE(i16, int16_t)
PN_DEFINE_MEMORY_WRITE(u16, uint16_t)
PN_DEFINE_MEMORY_WRITE(i32, int32_t)
PN_DEFINE_MEMORY_WRITE(u32, uint32_t)
PN_DEFINE_MEMORY_WRITE(i64, int64_t)
PN_DEFINE_MEMORY_WRITE(u64, uint64_t)
PN_DEFINE_MEMORY_WRITE(f32, float)
PN_DEFINE_MEMORY_WRITE(f64, double)

#undef PN_DEFINE_MEMORY_READ
#undef PN_DEFINE_MEMORY_WRITE

static uint32_t pn_memory_check_cstr(PNMemory* memory, uint32_t p) {
  uint32_t end = p;
  while (pn_memory_read_u8(memory, end) != 0) {
    end++;
  }
  return end - p;
}

static void pn_memory_zerofill(PNMemory* memory,
                               uint32_t offset,
                               uint32_t num_bytes) {
  pn_memory_check(memory, offset, num_bytes);
  memset(memory->data + offset, 0, num_bytes);
}

static int pn_string_list_count(char** p) {
  int result = 0;
  if (p) {
    for (; *p; ++p) {
      result++;
    }
  }
  return result;
}

static void pn_memory_init(PNMemory* memory, uint32_t size) {
  memset(memory, 0, sizeof(PNMemory));
  memory->size = size;
  memory->data = pn_malloc(memory->size);
}

static void pn_memory_reset(PNMemory* memory) {
  PNMemory copy = *memory;
  memset(memory, 0, sizeof(PNMemory));
  memory->data = copy.data;
  memory->size = copy.size;
}

static void pn_memory_init_startinfo(PNMemory* memory,
                                     char** argv,
                                     char** envp) {
  memory->startinfo_start = pn_align_up(memory->globalvar_end, 4);
  void* memory_startinfo = memory->data + memory->startinfo_start;
  /*
   * From nacl_start.h in the native_client repo:
   *
   * The true entry point for untrusted code is called with the normal C ABI,
   * taking one argument.  This is a pointer to stack space containing these
   * words:
   *   [0]             cleanup function pointer (always NULL in actual start)
   *   [1]             envc, count of envp[] pointers
   *   [2]             argc, count of argv[] pointers
   *   [3]             argv[0..argc] pointers, argv[argc] being NULL
   *   [3+argc]        envp[0..envc] pointers, envp[envc] being NULL
   *   [3+argc+envc]   auxv[] pairs
   */

  uint32_t* memory_startinfo32 = (uint32_t*)memory_startinfo;

  int argc = pn_string_list_count(argv);
  int envc = pn_string_list_count(envp);

  uint32_t auxv_length = 3;
  void* data_offset =
      memory_startinfo32 + 3 + (argc + 1) + (envc + 1) + auxv_length;

  pn_memory_check_pointer(memory, memory_startinfo,
                          data_offset - memory_startinfo);

  memory_startinfo32[0] = 0;
  memory_startinfo32[1] = envc;
  memory_startinfo32[2] = argc;

  PN_TRACE(EXECUTE, "startinfo = %" PRIuPTR "\n",
           memory_startinfo - memory->data);
  PN_TRACE(EXECUTE, "envc = %" PRIuPTR " (%d)\n",
           memory_startinfo + 4 - memory->data, envc);
  PN_TRACE(EXECUTE, "argc = %" PRIuPTR " (%d)\n",
           memory_startinfo + 8 - memory->data, argc);

  /* argv */
  int i = 0;
  uint32_t* memory_argv = memory_startinfo32 + 3;
  PN_TRACE(EXECUTE, "argv = %" PRIuPTR "\n", (void*)memory_argv - memory->data);
  for (i = 0; i < argc; ++i) {
    char* arg = argv[i];
    size_t len = strlen(arg) + 1;
    pn_memory_check_pointer(memory, data_offset, len);
    memcpy(data_offset, arg, len);
    memory_argv[i] = data_offset - memory->data;
    data_offset += len;
  }
  memory_argv[argc] = 0;

  /* envp */
  uint32_t* memory_envp = memory_argv + argc + 1;
  PN_TRACE(EXECUTE, "envp = %" PRIuPTR "\n", (void*)memory_envp - memory->data);
  for (i = 0; i < envc; ++i) {
    char* env = envp[i];
    size_t len = strlen(env) + 1;
    pn_memory_check_pointer(memory, data_offset, len);
    memcpy(data_offset, env, len);
    memory_envp[i] = data_offset - memory->data;
    data_offset += len;
  }
  memory_envp[envc] = 0;

  /*
   * The expected auxv structure is key/value pairs.
   * From elf_auxv.h in the native_client repo:
   *
   *   name       value  description
   *   AT_NULL    0      Terminating item in an auxv array
   *   AT_ENTRY   9      Entry point of the executable
   *   AT_SYSINFO 32     System entry call point
   *
   * In reality, the AT_SYSINFO value is the only one we care about, and it
   * is the address of the __nacl_irt_query function:
   *
   * typedef size_t (*TYPE_nacl_irt_query)(const char *interface_ident,
   *                                       void *table, size_t tablesize);
   */

  uint32_t* memory_auxv = memory_envp + envc + 1;
  PN_TRACE(EXECUTE, "auxv = %" PRIuPTR "\n", (void*)memory_auxv - memory->data);
  memory_auxv[0] = 32; /* AT_SYSINFO */
  memory_auxv[1] = pn_builtin_to_pointer(PN_BUILTIN_NACL_IRT_QUERY);
  memory_auxv[2] = 0; /* AT_NULL */

  memory->startinfo_end = data_offset - memory->data;
  memory->heap_start = pn_align_up(memory->startinfo_end, PN_PAGESIZE);
  memory->stack_end = memory->size;
}

#endif /* PN_MEMORY_H_ */
