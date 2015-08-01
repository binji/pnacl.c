/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef PN_TIMERS
#define PN_TIMERS 1
#endif

#ifndef PN_TRACING
#define PN_TRACING 1
#endif

#ifndef PN_CALCULATE_LIVENESS
#define PN_CALCULATE_LIVENESS 0
#endif

#define PN_UNALIGNED_MEMORY_ACCESS 1
#define PN_DEFAULT_ALIGN 8

#define PN_MIN_CHUNKSIZE (64 * 1024)
#define PN_MAX_BLOCK_ABBREV_OP 10
#define PN_MAX_BLOCK_ABBREV 100
#define PN_DEFAULT_MEMORY_SIZE (1024 * 1024)
#define PN_MEMORY_GUARD_SIZE 1024
#define PN_PAGESHIFT 12
#define PN_PAGESIZE (1 << PN_PAGESHIFT)
#define PN_INSTRUCTIONS_QUANTUM 100

#define PN_FALSE 0
#define PN_TRUE 1

#define PN_INVALID_VALUE_ID ((PNValueId)~0)
#define PN_INVALID_BB_ID ((PNBasicBlockId)~0)
#define PN_INVALID_FUNCTION_ID ((PNFunctionId)~0)
#define PN_INVALID_TYPE_ID ((PNTypeId)~0)
#define PN_INVALID_FUNCTION_ID ((PNFunctionId)~0)

#define PN_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#if PN_TRACING
#define PN_TRACE_PRINT_INDENT() PN_PRINT("%*s", g_pn_trace_indent, "")
#define PN_TRACE_PRINT_INDENTX(x) PN_PRINT("%*s", g_pn_trace_indent + x, "")
#define PN_TRACE(flag, ...)  \
  if (g_pn_trace_##flag) {   \
    PN_TRACE_PRINT_INDENT(); \
    PN_PRINT(__VA_ARGS__);   \
  } else                     \
  (void)0
#define PN_IS_TRACE(flag) g_pn_trace_##flag
#define PN_TRACE_INDENT(flag, c) \
  if (g_pn_trace_##flag)         \
    g_pn_trace_indent += c;      \
  else                           \
  (void)0
#define PN_TRACE_DEDENT(flag, c) \
  if (g_pn_trace_##flag)         \
    g_pn_trace_indent -= c;      \
  else                           \
  (void)0
#else
#define PN_TRACE_PRINT_INDENT() (void)0
#define PN_TRACE(flag, ...) (void)0
#define PN_IS_TRACE(flag) PN_FALSE
#define PN_TRACE_INDENT(flag, c) (void)0
#define PN_TRACE_DEDENT(flag, c) (void)0
#endif /* PN_TRACING */
#define PN_WARN(...)       \
  if (g_pn_verbose > 0)    \
    PN_PRINT(__VA_ARGS__); \
  else                     \
  (void)0
#define PN_PRINT(...) PN_ERROR(__VA_ARGS__)
#define PN_ERROR(...) fprintf(stderr, __VA_ARGS__)
#define PN_FATAL(...)      \
  do {                     \
    PN_ERROR(__VA_ARGS__); \
    exit(1);               \
  } while (0)
#define PN_UNREACHABLE() PN_FATAL("unreachable\n")
#define PN_CHECK(x)                         \
  if (!(x)) {                               \
    PN_FATAL("PN_CHECK failed: %s.\n", #x); \
  } else                                    \
  (void)0
#define PN_STATIC_ASSERT__(x, line) int __pn_static_assert_##line[x ? 1 : -1]
#define PN_STATIC_ASSERT_(x, line) PN_STATIC_ASSERT__(x, line)
#define PN_STATIC_ASSERT(x) PN_STATIC_ASSERT_(x, __LINE__)

/* Some crazy system where this isn't true? */
PN_STATIC_ASSERT(sizeof(float) == sizeof(uint32_t));
PN_STATIC_ASSERT(sizeof(double) == sizeof(uint64_t));

#define PN_NANOSECONDS_IN_A_MICROSECOND 1000

#if PN_TIMERS

#define PN_NANOSECONDS_IN_A_SECOND 1000000000

#define PN_BEGIN_TIME(name)          \
  struct timespec start_time_##name; \
  clock_gettime(CLOCK_MONOTONIC, &start_time_##name) /* no semicolon */

#define PN_END_TIME(name)                                                 \
  do {                                                                    \
    struct timespec end_time_##name;                                      \
    clock_gettime(CLOCK_MONOTONIC, &end_time_##name);                     \
    struct timespec time_delta_##name;                                    \
    pn_timespec_subtract(&time_delta_##name, &end_time_##name,            \
                         &start_time_##name);                             \
    struct timespec* timer_##name = &g_pn_timer_times[PN_TIMER_##name];   \
    struct timespec new_timer_##name;                                     \
    pn_timespec_add(&new_timer_##name, timer_##name, &time_delta_##name); \
    *timer_##name = new_timer_##name;                                     \
  } while (0) /* no semicolon */

#define PN_FOREACH_TIMER(V)       \
  V(TOTAL)                        \
  V(FILE_READ)                    \
  V(BLOCKINFO_BLOCK_READ)         \
  V(MODULE_BLOCK_READ)            \
  V(CONSTANTS_BLOCK_READ)         \
  V(FUNCTION_BLOCK_READ)          \
  V(VALUE_SYMTAB_BLOCK_READ)      \
  V(TYPE_BLOCK_READ)              \
  V(GLOBALVAR_BLOCK_READ)         \
  V(CALCULATE_RESULT_VALUE_TYPES) \
  V(CALCULATE_OPCODES)            \
  V(CALCULATE_USES)               \
  V(CALCULATE_PRED_BBS)           \
  V(CALCULATE_PHI_ASSIGNS)        \
  V(CALCULATE_LIVENESS)           \
  V(FUNCTION_TRACE)               \
  V(EXECUTE)

#define PN_TIMERS_ENUM(name) PN_TIMER_##name,
enum { PN_FOREACH_TIMER(PN_TIMERS_ENUM) PN_NUM_TIMERS };
#undef PN_TIMERS_ENUM

static struct timespec g_pn_timer_times[PN_NUM_TIMERS];

#else

#define PN_BEGIN_TIME(name) (void)0
#define PN_END_TIME(name) (void)0

#endif /* PN_TIMERS */

typedef uint8_t PNBool;
typedef uint16_t PNTypeId;
typedef uint32_t PNValueId;
typedef uint32_t PNFunctionId;
typedef uint16_t PNConstantId;
typedef uint32_t PNGlobalVarId;
typedef uint16_t PNInstructionId;
typedef uint16_t PNBasicBlockId;
typedef uint16_t PNAlignment;
typedef uint32_t PNJmpBufId;

typedef uint8_t pn_u8;
typedef int8_t pn_i8;
typedef uint16_t pn_u16;
typedef int16_t pn_i16;
typedef uint32_t pn_u32;
typedef int32_t pn_i32;
typedef uint64_t pn_u64;
typedef int64_t pn_i64;
typedef float pn_f32;
typedef double pn_f64;

static int g_pn_verbose;
static const char* g_pn_filename;
static char** g_pn_argv;
static char** g_pn_environ;
static uint32_t g_pn_memory_size = PN_DEFAULT_MEMORY_SIZE;
static PNBool g_pn_print_named_functions;
static PNBool g_pn_print_stats;
static PNBool g_pn_print_opcode_counts;
static PNBool g_pn_run;
#if PN_TRACING
static const char* g_pn_trace_function_filter;
static int g_pn_trace_indent;
#endif /* PN_TRACING */

#if PN_TIMERS
static PNBool g_pn_print_time;
#endif /* PN_TIMERS */

#if PN_TRACING
#define PN_FOREACH_TRACE(V)                   \
  V(FLAGS, "flags")                           \
  V(ABBREV, "abbrev")                         \
  V(BLOCKINFO_BLOCK, "blockinfo-block")       \
  V(TYPE_BLOCK, "type-block")                 \
  V(GLOBALVAR_BLOCK, "globalvar-block")       \
  V(VALUE_SYMTAB_BLOCK, "value-symtab-block") \
  V(CONSTANTS_BLOCK, "constants-block")       \
  V(FUNCTION_BLOCK, "function-block")         \
  V(MODULE_BLOCK, "module-block")             \
  V(BASIC_BLOCKS, "basic-blocks")             \
  V(INSTRUCTIONS, "instructions")             \
  V(EXECUTE, "execute")                       \
  V(IRT, "irt")                               \
  V(INTRINSICS, "intrinsics")                 \
  V(MEMORY, "memory")

#define PN_TRACE_ENUM(name, flag) PN_TRACE_##name,
enum { PN_FOREACH_TRACE(PN_TRACE_ENUM) PN_NUM_TRACE };
#undef PN_TRACE_ENUM

#define PN_TRACE_DEFINE(name, flag) static PNBool g_pn_trace_##name;
PN_FOREACH_TRACE(PN_TRACE_DEFINE)
#undef PN_TRACE_DEFINE
#endif /* PN_TRACING */

typedef enum PNEntry {
  PN_ENTRY_END_BLOCK = 0,
  PN_ENTRY_SUBBLOCK = 1,
  PN_ENTRY_DEFINE_ABBREV = 2,
  PN_ENTRY_UNABBREV_RECORD = 3,
} PNEntry;

typedef enum PNEncoding {
  PN_ENCODING_LITERAL = 0,
  PN_ENCODING_FIXED = 1,
  PN_ENCODING_VBR = 2,
  PN_ENCODING_ARRAY = 3,
  PN_ENCODING_CHAR6 = 4,
  PN_ENCODING_BLOB = 5,
} PNEncoding;

typedef enum PNBlockId {
  PN_BLOCKID_BLOCKINFO = 0,
  PN_BLOCKID_MODULE = 8,
  PN_BLOCKID_CONSTANTS = 11,
  PN_BLOCKID_FUNCTION = 12,
  PN_BLOCKID_VALUE_SYMTAB = 14,
  PN_BLOCKID_TYPE = 17,
  PN_BLOCKID_GLOBALVAR = 19,
  PN_MAX_BLOCK_IDS,
} PNBlockId;

typedef enum PNBlockInfoCode {
  PN_BLOCKINFO_CODE_SETBID = 1,
  PN_BLOCKINFO_CODE_BLOCKNAME = 2,
  PN_BLOCKINFO_CODE_SETRECORDNAME = 3,
} PNBlockInfoCode;

typedef enum PNModuleCode {
  PN_MODULE_CODE_VERSION = 1,
  PN_MODULE_CODE_FUNCTION = 8,
} PNModuleCode;

typedef enum PNTypeCode {
  PN_TYPE_CODE_NUMENTRY = 1,
  PN_TYPE_CODE_VOID = 2,
  PN_TYPE_CODE_FLOAT = 3,
  PN_TYPE_CODE_DOUBLE = 4,
  PN_TYPE_CODE_INTEGER = 7,
  PN_TYPE_CODE_FUNCTION = 21,
} PNTypeCode;

typedef enum PNGlobalVarCode {
  PN_GLOBALVAR_CODE_VAR = 0,
  PN_GLOBALVAR_CODE_COMPOUND = 1,
  PN_GLOBALVAR_CODE_ZEROFILL = 2,
  PN_GLOBALVAR_CODE_DATA = 3,
  PN_GLOBALVAR_CODE_RELOC = 4,
  PN_GLOBALVAR_CODE_COUNT = 5,
} PNGlobalVarCode;

typedef enum PNValueSymtabCode {
  PN_VALUESYMBTAB_CODE_ENTRY = 1,
  PN_VALUESYMBTAB_CODE_BBENTRY = 2,
} PNValueSymtabCode;

typedef enum PNFunctionCode {
  PN_FUNCTION_CODE_DECLAREBLOCKS = 1,
  PN_FUNCTION_CODE_INST_BINOP = 2,
  PN_FUNCTION_CODE_INST_CAST = 3,
  PN_FUNCTION_CODE_INST_RET = 10,
  PN_FUNCTION_CODE_INST_BR = 11,
  PN_FUNCTION_CODE_INST_SWITCH = 12,
  PN_FUNCTION_CODE_INST_UNREACHABLE = 15,
  PN_FUNCTION_CODE_INST_PHI = 16,
  PN_FUNCTION_CODE_INST_ALLOCA = 19,
  PN_FUNCTION_CODE_INST_LOAD = 20,
  PN_FUNCTION_CODE_INST_STORE = 24,
  PN_FUNCTION_CODE_INST_CMP2 = 28,
  PN_FUNCTION_CODE_INST_VSELECT = 29,
  PN_FUNCTION_CODE_INST_CALL = 34,
  PN_FUNCTION_CODE_INST_FORWARDTYPEREF = 43,
  PN_FUNCTION_CODE_INST_CALL_INDIRECT = 44,
} PNFunctionCode;

typedef enum PNConstantCode {
  PN_CONSTANTS_CODE_SETTYPE = 1,
  PN_CONSTANTS_CODE_UNDEF = 3,
  PN_CONSTANTS_CODE_INTEGER = 4,
  PN_CONSTANTS_CODE_FLOAT = 6,
} PNConstantCode;

typedef enum PNBinOp {
  PN_BINOP_ADD = 0,
  PN_BINOP_SUB = 1,
  PN_BINOP_MUL = 2,
  PN_BINOP_UDIV = 3,
  PN_BINOP_SDIV = 4,
  PN_BINOP_UREM = 5,
  PN_BINOP_SREM = 6,
  PN_BINOP_SHL = 7,
  PN_BINOP_LSHR = 8,
  PN_BINOP_ASHR = 9,
  PN_BINOP_AND = 10,
  PN_BINOP_OR = 11,
  PN_BINOP_XOR = 12,
} PNBinOp;

typedef enum PNCmp2 {
  PN_FCMP_FALSE = 0,
  PN_FCMP_OEQ = 1,
  PN_FCMP_OGT = 2,
  PN_FCMP_OGE = 3,
  PN_FCMP_OLT = 4,
  PN_FCMP_OLE = 5,
  PN_FCMP_ONE = 6,
  PN_FCMP_ORD = 7,
  PN_FCMP_UNO = 8,
  PN_FCMP_UEQ = 9,
  PN_FCMP_UGT = 10,
  PN_FCMP_UGE = 11,
  PN_FCMP_ULT = 12,
  PN_FCMP_ULE = 13,
  PN_FCMP_UNE = 14,
  PN_FCMP_TRUE = 15,
  PN_ICMP_EQ = 32,
  PN_ICMP_NE = 33,
  PN_ICMP_UGT = 34,
  PN_ICMP_UGE = 35,
  PN_ICMP_ULT = 36,
  PN_ICMP_ULE = 37,
  PN_ICMP_SGT = 38,
  PN_ICMP_SGE = 39,
  PN_ICMP_SLT = 40,
  PN_ICMP_SLE = 41,
} PNCmp2;

typedef enum PNCast {
  PN_CAST_TRUNC = 0,
  PN_CAST_ZEXT = 1,
  PN_CAST_SEXT = 2,
  PN_CAST_FPTOUI = 3,
  PN_CAST_FPTOSI = 4,
  PN_CAST_UITOFP = 5,
  PN_CAST_SITOFP = 6,
  PN_CAST_FPTRUNC = 7,
  PN_CAST_FPEXT = 8,
  PN_CAST_BITCAST = 11,
} PNCast;

typedef enum PNBasicType {
  PN_BASIC_TYPE_VOID,
  PN_BASIC_TYPE_INT1,
  PN_BASIC_TYPE_INT8,
  PN_BASIC_TYPE_INT16,
  PN_BASIC_TYPE_INT32,
  PN_BASIC_TYPE_INT64,
  PN_BASIC_TYPE_FLOAT,
  PN_BASIC_TYPE_DOUBLE,
} PNBasicType;

#define PN_FOREACH_BUILTIN(V)   \
  V(NACL_IRT_QUERY)             \
  V(NACL_IRT_BASIC_EXIT)        \
  V(NACL_IRT_BASIC_GETTOD)      \
  V(NACL_IRT_BASIC_CLOCK)       \
  V(NACL_IRT_BASIC_NANOSLEEP)   \
  V(NACL_IRT_BASIC_SCHED_YIELD) \
  V(NACL_IRT_BASIC_SYSCONF)     \
  V(NACL_IRT_FDIO_CLOSE)        \
  V(NACL_IRT_FDIO_DUP)          \
  V(NACL_IRT_FDIO_DUP2)         \
  V(NACL_IRT_FDIO_READ)         \
  V(NACL_IRT_FDIO_WRITE)        \
  V(NACL_IRT_FDIO_SEEK)         \
  V(NACL_IRT_FDIO_FSTAT)        \
  V(NACL_IRT_FDIO_GETDENTS)     \
  V(NACL_IRT_FDIO_FCHDIR)       \
  V(NACL_IRT_FDIO_FCHMOD)       \
  V(NACL_IRT_FDIO_FSYNC)        \
  V(NACL_IRT_FDIO_FDATASYNC)    \
  V(NACL_IRT_FDIO_FTRUNCATE)    \
  V(NACL_IRT_FDIO_ISATTY)       \
  V(NACL_IRT_FILENAME_OPEN)     \
  V(NACL_IRT_FILENAME_STAT)     \
  V(NACL_IRT_FILENAME_MKDIR)    \
  V(NACL_IRT_FILENAME_RMDIR)    \
  V(NACL_IRT_FILENAME_CHDIR)    \
  V(NACL_IRT_FILENAME_GETCWD)   \
  V(NACL_IRT_FILENAME_UNLINK)   \
  V(NACL_IRT_FILENAME_TRUNCATE) \
  V(NACL_IRT_FILENAME_LSTAT)    \
  V(NACL_IRT_FILENAME_LINK)     \
  V(NACL_IRT_FILENAME_RENAME)   \
  V(NACL_IRT_FILENAME_SYMLINK)  \
  V(NACL_IRT_FILENAME_CHMOD)    \
  V(NACL_IRT_FILENAME_ACCESS)   \
  V(NACL_IRT_FILENAME_READLINK) \
  V(NACL_IRT_FILENAME_UTIMES)   \
  V(NACL_IRT_MEMORY_MMAP)       \
  V(NACL_IRT_MEMORY_MUNMAP)     \
  V(NACL_IRT_MEMORY_MPROTECT)   \
  V(NACL_IRT_TLS_INIT)          \
  V(NACL_IRT_TLS_GET)           \
  V(NACL_IRT_THREAD_CREATE)     \
  V(NACL_IRT_THREAD_EXIT)       \
  V(NACL_IRT_THREAD_NICE)       \
  V(NACL_IRT_FUTEX_WAIT_ABS)    \
  V(NACL_IRT_FUTEX_WAKE)

typedef enum PNBuiltinId {
  PN_BUILTIN_NULL,
#define PN_BUILTIN(e) PN_BUILTIN_##e,
  PN_FOREACH_BUILTIN(PN_BUILTIN)
#undef PN_BUILTIN
  PN_MAX_BUILTINS
} PNBuiltinId;

#define PN_FOREACH_INTRINSIC(V)                                   \
  V(LLVM_BSWAP_I16, "llvm.bswap.i16")                             \
  V(LLVM_BSWAP_I32, "llvm.bswap.i32")                             \
  V(LLVM_BSWAP_I64, "llvm.bswap.i64")                             \
  V(LLVM_CTLZ_I32, "llvm.ctlz.i32")                               \
  V(LLVM_CTTZ_I32, "llvm.cttz.i32")                               \
  V(LLVM_FABS_F32, "llvm.fabs.f32")                               \
  V(LLVM_FABS_F64, "llvm.fabs.f64")                               \
  V(LLVM_MEMCPY, "llvm.memcpy.p0i8.p0i8.i32")                     \
  V(LLVM_MEMMOVE, "llvm.memmove.p0i8.p0i8.i32")                   \
  V(LLVM_MEMSET, "llvm.memset.p0i8.i32")                          \
  V(LLVM_NACL_ATOMIC_CMPXCHG_I8, "llvm.nacl.atomic.cmpxchg.i8")   \
  V(LLVM_NACL_ATOMIC_CMPXCHG_I16, "llvm.nacl.atomic.cmpxchg.i16") \
  V(LLVM_NACL_ATOMIC_CMPXCHG_I32, "llvm.nacl.atomic.cmpxchg.i32") \
  V(LLVM_NACL_ATOMIC_CMPXCHG_I64, "llvm.nacl.atomic.cmpxchg.i64") \
  V(LLVM_NACL_ATOMIC_LOAD_I8, "llvm.nacl.atomic.load.i8")         \
  V(LLVM_NACL_ATOMIC_LOAD_I16, "llvm.nacl.atomic.load.i16")       \
  V(LLVM_NACL_ATOMIC_LOAD_I32, "llvm.nacl.atomic.load.i32")       \
  V(LLVM_NACL_ATOMIC_LOAD_I64, "llvm.nacl.atomic.load.i64")       \
  V(LLVM_NACL_ATOMIC_RMW_I8, "llvm.nacl.atomic.rmw.i8")           \
  V(LLVM_NACL_ATOMIC_RMW_I16, "llvm.nacl.atomic.rmw.i16")         \
  V(LLVM_NACL_ATOMIC_RMW_I32, "llvm.nacl.atomic.rmw.i32")         \
  V(LLVM_NACL_ATOMIC_RMW_I64, "llvm.nacl.atomic.rmw.i64")         \
  V(LLVM_NACL_ATOMIC_STORE_I8, "llvm.nacl.atomic.store.i8")       \
  V(LLVM_NACL_ATOMIC_STORE_I16, "llvm.nacl.atomic.store.i16")     \
  V(LLVM_NACL_ATOMIC_STORE_I32, "llvm.nacl.atomic.store.i32")     \
  V(LLVM_NACL_ATOMIC_STORE_I64, "llvm.nacl.atomic.store.i64")     \
  V(LLVM_NACL_LONGJMP, "llvm.nacl.longjmp")                       \
  V(LLVM_NACL_READ_TP, "llvm.nacl.read.tp")                       \
  V(LLVM_NACL_SETJMP, "llvm.nacl.setjmp")                         \
  V(LLVM_SQRT_F32, "llvm.sqrt.f32")                               \
  V(LLVM_SQRT_F64, "llvm.sqrt.f64")                               \
  V(LLVM_STACKRESTORE, "llvm.stackrestore")                       \
  V(LLVM_STACKSAVE, "llvm.stacksave")                             \
  V(LLVM_TRAP, "llvm.trap")                                       \
  V(START, "_start")

#define PN_FOREACH_OPCODE(V)                 \
  V(ALLOCA_INT32)                            \
  V(BINOP_ADD_DOUBLE)                        \
  V(BINOP_ADD_FLOAT)                         \
  V(BINOP_ADD_INT8)                          \
  V(BINOP_ADD_INT16)                         \
  V(BINOP_ADD_INT32)                         \
  V(BINOP_ADD_INT64)                         \
  V(BINOP_AND_INT1)                          \
  V(BINOP_AND_INT8)                          \
  V(BINOP_AND_INT16)                         \
  V(BINOP_AND_INT32)                         \
  V(BINOP_AND_INT64)                         \
  V(BINOP_ASHR_INT8)                         \
  V(BINOP_ASHR_INT16)                        \
  V(BINOP_ASHR_INT32)                        \
  V(BINOP_ASHR_INT64)                        \
  V(BINOP_LSHR_INT8)                         \
  V(BINOP_LSHR_INT16)                        \
  V(BINOP_LSHR_INT32)                        \
  V(BINOP_LSHR_INT64)                        \
  V(BINOP_MUL_DOUBLE)                        \
  V(BINOP_MUL_FLOAT)                         \
  V(BINOP_MUL_INT8)                          \
  V(BINOP_MUL_INT16)                         \
  V(BINOP_MUL_INT32)                         \
  V(BINOP_MUL_INT64)                         \
  V(BINOP_OR_INT1)                           \
  V(BINOP_OR_INT8)                           \
  V(BINOP_OR_INT16)                          \
  V(BINOP_OR_INT32)                          \
  V(BINOP_OR_INT64)                          \
  V(BINOP_SDIV_DOUBLE)                       \
  V(BINOP_SDIV_FLOAT)                        \
  V(BINOP_SDIV_INT32)                        \
  V(BINOP_SDIV_INT64)                        \
  V(BINOP_SHL_INT8)                          \
  V(BINOP_SHL_INT16)                         \
  V(BINOP_SHL_INT32)                         \
  V(BINOP_SHL_INT64)                         \
  V(BINOP_SREM_INT32)                        \
  V(BINOP_SREM_INT64)                        \
  V(BINOP_SUB_DOUBLE)                        \
  V(BINOP_SUB_FLOAT)                         \
  V(BINOP_SUB_INT8)                          \
  V(BINOP_SUB_INT16)                         \
  V(BINOP_SUB_INT32)                         \
  V(BINOP_SUB_INT64)                         \
  V(BINOP_UDIV_INT8)                         \
  V(BINOP_UDIV_INT16)                        \
  V(BINOP_UDIV_INT32)                        \
  V(BINOP_UDIV_INT64)                        \
  V(BINOP_UREM_INT8)                         \
  V(BINOP_UREM_INT16)                        \
  V(BINOP_UREM_INT32)                        \
  V(BINOP_UREM_INT64)                        \
  V(BINOP_XOR_INT1)                          \
  V(BINOP_XOR_INT8)                          \
  V(BINOP_XOR_INT16)                         \
  V(BINOP_XOR_INT32)                         \
  V(BINOP_XOR_INT64)                         \
  V(BR)                                      \
  V(BR_INT1)                                 \
  V(CALL)                                    \
  V(CALL_INDIRECT)                           \
  V(CAST_BITCAST_DOUBLE_INT64)               \
  V(CAST_BITCAST_FLOAT_INT32)                \
  V(CAST_BITCAST_INT32_FLOAT)                \
  V(CAST_BITCAST_INT64_DOUBLE)               \
  V(CAST_FPEXT_FLOAT_DOUBLE)                 \
  V(CAST_FPTOSI_DOUBLE_INT8)                 \
  V(CAST_FPTOSI_DOUBLE_INT16)                \
  V(CAST_FPTOSI_DOUBLE_INT32)                \
  V(CAST_FPTOSI_DOUBLE_INT64)                \
  V(CAST_FPTOSI_FLOAT_INT8)                  \
  V(CAST_FPTOSI_FLOAT_INT16)                 \
  V(CAST_FPTOSI_FLOAT_INT32)                 \
  V(CAST_FPTOSI_FLOAT_INT64)                 \
  V(CAST_FPTOUI_DOUBLE_INT8)                 \
  V(CAST_FPTOUI_DOUBLE_INT16)                \
  V(CAST_FPTOUI_DOUBLE_INT32)                \
  V(CAST_FPTOUI_DOUBLE_INT64)                \
  V(CAST_FPTOUI_FLOAT_INT8)                  \
  V(CAST_FPTOUI_FLOAT_INT16)                 \
  V(CAST_FPTOUI_FLOAT_INT32)                 \
  V(CAST_FPTOUI_FLOAT_INT64)                 \
  V(CAST_FPTRUNC_DOUBLE_FLOAT)               \
  V(CAST_SEXT_INT1_INT8)                     \
  V(CAST_SEXT_INT1_INT16)                    \
  V(CAST_SEXT_INT1_INT32)                    \
  V(CAST_SEXT_INT1_INT64)                    \
  V(CAST_SEXT_INT8_INT16)                    \
  V(CAST_SEXT_INT8_INT32)                    \
  V(CAST_SEXT_INT8_INT64)                    \
  V(CAST_SEXT_INT16_INT32)                   \
  V(CAST_SEXT_INT16_INT64)                   \
  V(CAST_SEXT_INT32_INT64)                   \
  V(CAST_SITOFP_INT8_DOUBLE)                 \
  V(CAST_SITOFP_INT8_FLOAT)                  \
  V(CAST_SITOFP_INT16_DOUBLE)                \
  V(CAST_SITOFP_INT16_FLOAT)                 \
  V(CAST_SITOFP_INT32_DOUBLE)                \
  V(CAST_SITOFP_INT32_FLOAT)                 \
  V(CAST_SITOFP_INT64_DOUBLE)                \
  V(CAST_SITOFP_INT64_FLOAT)                 \
  V(CAST_TRUNC_INT8_INT1)                    \
  V(CAST_TRUNC_INT16_INT1)                   \
  V(CAST_TRUNC_INT16_INT8)                   \
  V(CAST_TRUNC_INT32_INT1)                   \
  V(CAST_TRUNC_INT32_INT8)                   \
  V(CAST_TRUNC_INT32_INT16)                  \
  V(CAST_TRUNC_INT64_INT8)                   \
  V(CAST_TRUNC_INT64_INT16)                  \
  V(CAST_TRUNC_INT64_INT32)                  \
  V(CAST_UITOFP_INT8_DOUBLE)                 \
  V(CAST_UITOFP_INT8_FLOAT)                  \
  V(CAST_UITOFP_INT16_DOUBLE)                \
  V(CAST_UITOFP_INT16_FLOAT)                 \
  V(CAST_UITOFP_INT32_DOUBLE)                \
  V(CAST_UITOFP_INT32_FLOAT)                 \
  V(CAST_UITOFP_INT64_DOUBLE)                \
  V(CAST_UITOFP_INT64_FLOAT)                 \
  V(CAST_ZEXT_INT1_INT8)                     \
  V(CAST_ZEXT_INT1_INT16)                    \
  V(CAST_ZEXT_INT1_INT32)                    \
  V(CAST_ZEXT_INT1_INT64)                    \
  V(CAST_ZEXT_INT8_INT16)                    \
  V(CAST_ZEXT_INT8_INT32)                    \
  V(CAST_ZEXT_INT8_INT64)                    \
  V(CAST_ZEXT_INT16_INT32)                   \
  V(CAST_ZEXT_INT16_INT64)                   \
  V(CAST_ZEXT_INT32_INT64)                   \
  V(FCMP_OEQ_DOUBLE)                         \
  V(FCMP_OEQ_FLOAT)                          \
  V(FCMP_OGE_DOUBLE)                         \
  V(FCMP_OGE_FLOAT)                          \
  V(FCMP_OGT_DOUBLE)                         \
  V(FCMP_OGT_FLOAT)                          \
  V(FCMP_OLE_DOUBLE)                         \
  V(FCMP_OLE_FLOAT)                          \
  V(FCMP_OLT_DOUBLE)                         \
  V(FCMP_OLT_FLOAT)                          \
  V(FCMP_ONE_DOUBLE)                         \
  V(FCMP_ONE_FLOAT)                          \
  V(FCMP_ORD_DOUBLE)                         \
  V(FCMP_ORD_FLOAT)                          \
  V(FCMP_UEQ_DOUBLE)                         \
  V(FCMP_UEQ_FLOAT)                          \
  V(FCMP_UGE_DOUBLE)                         \
  V(FCMP_UGE_FLOAT)                          \
  V(FCMP_UGT_DOUBLE)                         \
  V(FCMP_UGT_FLOAT)                          \
  V(FCMP_ULE_DOUBLE)                         \
  V(FCMP_ULE_FLOAT)                          \
  V(FCMP_ULT_DOUBLE)                         \
  V(FCMP_ULT_FLOAT)                          \
  V(FCMP_UNE_DOUBLE)                         \
  V(FCMP_UNE_FLOAT)                          \
  V(FCMP_UNO_DOUBLE)                         \
  V(FCMP_UNO_FLOAT)                          \
  V(FORWARDTYPEREF)                          \
  V(ICMP_EQ_INT8)                            \
  V(ICMP_EQ_INT16)                           \
  V(ICMP_EQ_INT32)                           \
  V(ICMP_EQ_INT64)                           \
  V(ICMP_NE_INT8)                            \
  V(ICMP_NE_INT16)                           \
  V(ICMP_NE_INT32)                           \
  V(ICMP_NE_INT64)                           \
  V(ICMP_SGE_INT8)                           \
  V(ICMP_SGE_INT16)                          \
  V(ICMP_SGE_INT32)                          \
  V(ICMP_SGE_INT64)                          \
  V(ICMP_SGT_INT8)                           \
  V(ICMP_SGT_INT16)                          \
  V(ICMP_SGT_INT32)                          \
  V(ICMP_SGT_INT64)                          \
  V(ICMP_SLE_INT8)                           \
  V(ICMP_SLE_INT16)                          \
  V(ICMP_SLE_INT32)                          \
  V(ICMP_SLE_INT64)                          \
  V(ICMP_SLT_INT8)                           \
  V(ICMP_SLT_INT16)                          \
  V(ICMP_SLT_INT32)                          \
  V(ICMP_SLT_INT64)                          \
  V(ICMP_UGE_INT8)                           \
  V(ICMP_UGE_INT16)                          \
  V(ICMP_UGE_INT32)                          \
  V(ICMP_UGE_INT64)                          \
  V(ICMP_UGT_INT8)                           \
  V(ICMP_UGT_INT16)                          \
  V(ICMP_UGT_INT32)                          \
  V(ICMP_UGT_INT64)                          \
  V(ICMP_ULE_INT8)                           \
  V(ICMP_ULE_INT16)                          \
  V(ICMP_ULE_INT32)                          \
  V(ICMP_ULE_INT64)                          \
  V(ICMP_ULT_INT8)                           \
  V(ICMP_ULT_INT16)                          \
  V(ICMP_ULT_INT32)                          \
  V(ICMP_ULT_INT64)                          \
  V(INTRINSIC_LLVM_NACL_ATOMIC_ADD_I8)       \
  V(INTRINSIC_LLVM_NACL_ATOMIC_ADD_I16)      \
  V(INTRINSIC_LLVM_NACL_ATOMIC_ADD_I32)      \
  V(INTRINSIC_LLVM_NACL_ATOMIC_ADD_I64)      \
  V(INTRINSIC_LLVM_NACL_ATOMIC_AND_I8)       \
  V(INTRINSIC_LLVM_NACL_ATOMIC_AND_I16)      \
  V(INTRINSIC_LLVM_NACL_ATOMIC_AND_I32)      \
  V(INTRINSIC_LLVM_NACL_ATOMIC_AND_I64)      \
  V(INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I8)  \
  V(INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I16) \
  V(INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I32) \
  V(INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I64) \
  V(INTRINSIC_LLVM_NACL_ATOMIC_OR_I8)        \
  V(INTRINSIC_LLVM_NACL_ATOMIC_OR_I16)       \
  V(INTRINSIC_LLVM_NACL_ATOMIC_OR_I32)       \
  V(INTRINSIC_LLVM_NACL_ATOMIC_OR_I64)       \
  V(INTRINSIC_LLVM_NACL_ATOMIC_SUB_I8)       \
  V(INTRINSIC_LLVM_NACL_ATOMIC_SUB_I16)      \
  V(INTRINSIC_LLVM_NACL_ATOMIC_SUB_I32)      \
  V(INTRINSIC_LLVM_NACL_ATOMIC_SUB_I64)      \
  V(INTRINSIC_LLVM_NACL_ATOMIC_XOR_I8)       \
  V(INTRINSIC_LLVM_NACL_ATOMIC_XOR_I16)      \
  V(INTRINSIC_LLVM_NACL_ATOMIC_XOR_I32)      \
  V(INTRINSIC_LLVM_NACL_ATOMIC_XOR_I64)      \
  V(LOAD_DOUBLE)                             \
  V(LOAD_FLOAT)                              \
  V(LOAD_INT8)                               \
  V(LOAD_INT16)                              \
  V(LOAD_INT32)                              \
  V(LOAD_INT64)                              \
  V(PHI)                                     \
  V(RET)                                     \
  V(RET_VALUE)                               \
  V(STORE_DOUBLE)                            \
  V(STORE_FLOAT)                             \
  V(STORE_INT8)                              \
  V(STORE_INT16)                             \
  V(STORE_INT32)                             \
  V(STORE_INT64)                             \
  V(SWITCH_INT1)                             \
  V(SWITCH_INT8)                             \
  V(SWITCH_INT16)                            \
  V(SWITCH_INT32)                            \
  V(SWITCH_INT64)                            \
  V(UNREACHABLE)                             \
  V(VSELECT)

typedef enum PNOpcode {
#define PN_OPCODE(e) PN_OPCODE_##e,
  PN_FOREACH_OPCODE(PN_OPCODE)
#undef PN_OPCODE

#define PN_INTRINSIC_OPCODE(e, name) PN_OPCODE_INTRINSIC_##e,
  PN_FOREACH_INTRINSIC(PN_INTRINSIC_OPCODE)
#undef PN_INTRINSIC_OPCODE

  PN_MAX_OPCODE,
} PNOpcode;

static const char* g_pn_opcode_names[] = {
#define PN_OPCODE(e) #e,
  PN_FOREACH_OPCODE(PN_OPCODE)
#undef PN_OPCODE

#define PN_INTRINSIC_OPCODE(e, name) "INTRINSIC_" #e,
  PN_FOREACH_INTRINSIC(PN_INTRINSIC_OPCODE)
#undef PN_INTRINSIC_OPCODE
};

/* These asserts allow us to ensure that for all atomic ops, the types are
 * consecutive and the types are ordered i8, i16, i32, i64. We can then safely
 * do basic arithmetic on the enum value. */
#define PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(opcode)                   \
  PN_STATIC_ASSERT(                                                 \
      PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I8 <          \
          PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I16 &&    \
      PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I16 <         \
          PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I32 &&    \
      PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I32 <         \
          PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I64 &&    \
      PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I64 -         \
              PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I8 == \
          3) /* no semicolon */
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(RMW);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(ADD);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(SUB);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(AND);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(OR);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(XOR);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(EXCHANGE);

#undef PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES

typedef enum PNIntrinsicId {
  PN_INTRINSIC_NULL,
#define PN_INTRINSIC_DEFINE(e, name) PN_INTRINSIC_##e,
  PN_FOREACH_INTRINSIC(PN_INTRINSIC_DEFINE)
#undef PN_INTRINSIC_DEFINE
  PN_MAX_INTRINSICS,
} PNIntrinsicId;

#define PN_FOREACH_ERRNO(V)                 \
  V(EPERM, 1, "Operation not permitted")    \
  V(ENOENT, 2, "No such file or directory") \
  V(ESRCH, 3, "No such process")            \
  V(EINTR, 4, "Interrupted system call")    \
  V(EIO, 5, "I/O error")                    \
  V(ENXIO, 6, "No such device or address")  \
  V(E2BIG, 7, "Argument list too long")     \
  V(ENOEXEC, 8, "Exec format error")        \
  V(EBADF, 9, "Bad file number")            \
  V(ECHILD, 10, "No child processes")       \
  V(EAGAIN, 11, "Try again")                \
  V(ENOMEM, 12, "Out of memory")            \
  V(EACCES, 13, "Permission denied")        \
  V(EFAULT, 14, "Bad address")              \
  V(EBUSY, 16, "Device or resource busy")   \
  V(EEXIST, 17, "File exists")              \
  V(EXDEV, 18, "Cross-device link")         \
  V(ENODEV, 19, "No such device")           \
  V(ENOTDIR, 20, "Not a directory")         \
  V(EISDIR, 21, "Is a directory")           \
  V(EINVAL, 22, "Invalid argument")         \
  V(ENFILE, 23, "File table overflow")      \
  V(EMFILE, 24, "Too many open files")      \
  V(ENOTTY, 25, "Not a typewriter")         \
  V(EFBIG, 27, "File too large")            \
  V(ENOSPC, 28, "No space left on device")  \
  V(ESPIPE, 29, "Illegal seek")             \
  V(EROFS, 30, "Read-only file system")     \
  V(EMLINK, 31, "Too many links")           \
  V(EPIPE, 32, "Broken pipe")               \
  V(ENAMETOOLONG, 36, "File name too long") \
  V(ENOSYS, 38, "Function not implemented") \
  V(EDQUOT, 122, "Quota exceeded")          \
  V(ETIMEDOUT, 110, "Connection timed out")

typedef enum PNErrno {
#define PN_ERRNO(name, id, str) PN_##name = id,
  PN_FOREACH_ERRNO(PN_ERRNO)
#undef PN_ERRNO
} PNErrno;

typedef enum PNThreadState {
  PN_THREAD_RUNNING,
  PN_THREAD_BLOCKED,
  PN_THREAD_DEAD,
} PNThreadState;

typedef enum PNFutexState {
  PN_FUTEX_NONE,
  PN_FUTEX_WOKEN,
  PN_FUTEX_TIMEDOUT,
} PNFutexState;

typedef struct PNAllocatorChunk {
  struct PNAllocatorChunk* next;
  void* current;
  void* end;
} PNAllocatorChunk;

typedef struct PNAllocator {
  const char* name;
  PNAllocatorChunk* chunk_head;
  /* Last allocation. This is the only one that can be realloc'd */
  void* last_alloc;
  size_t min_chunk_size;
  size_t total_used;
  size_t internal_fragmentation;
} PNAllocator;

typedef struct PNAllocatorMark {
  void* current;
  void* last_alloc;
  size_t total_used;
  size_t internal_fragmentation;
} PNAllocatorMark;

typedef struct PNBitSet {
  uint32_t num_words;
  uint32_t* words;
} PNBitSet;

typedef struct PNBitStream {
  void* data;
  uint32_t data_len;
  uint32_t curword;
  int curword_bits;
  uint32_t bit_offset;
} PNBitStream;

typedef struct PNBlockAbbrevOp {
  PNEncoding encoding;
  union {
    /* PN_ENCODING_LITERAL */
    uint32_t value;
    /* PN_ENCODING_FIXED */
    /* PN_ENCODING_VBR */
    int num_bits;
  };
} PNBlockAbbrevOp;

typedef struct PNBlockAbbrev {
  uint32_t num_ops;
  PNBlockAbbrevOp ops[PN_MAX_BLOCK_ABBREV_OP];
} PNBlockAbbrev;

typedef struct PNBlockAbbrevs {
  uint32_t num_abbrevs;
  PNBlockAbbrev abbrevs[PN_MAX_BLOCK_ABBREV];
} PNBlockAbbrevs;

typedef struct PNRecordReader {
  struct PNBitStream* bs;
  struct PNBlockAbbrevs* abbrevs;
  uint32_t entry;
  uint32_t op_index;
  uint32_t num_values;
  uint32_t value_index;
} PNRecordReader;

typedef struct PNSwitchCase {
  int64_t value;
  PNBasicBlockId bb_id;
} PNSwitchCase;

typedef struct PNPhiIncoming {
  PNBasicBlockId bb_id;
  PNValueId value_id;
} PNPhiIncoming;

typedef struct PNInstruction {
  PNFunctionCode code;
  PNOpcode opcode;
} PNInstruction;

typedef struct PNInstructionBinop {
  PNInstruction base;
  PNValueId result_value_id;
  PNValueId value0_id;
  PNValueId value1_id;
  PNBinOp binop_opcode;
  int32_t flags;
} PNInstructionBinop;

typedef struct PNInstructionCast {
  PNInstruction base;
  PNValueId result_value_id;
  PNValueId value_id;
  PNCast cast_opcode;
  PNTypeId type_id;
} PNInstructionCast;

typedef struct PNInstructionRet {
  PNInstruction base;
  PNValueId value_id; /* Or PN_INVALID_VALUE_ID */
} PNInstructionRet;

typedef struct PNInstructionBr {
  PNInstruction base;
  PNBasicBlockId true_bb_id;
  PNBasicBlockId false_bb_id; /* Or PN_INVALID_BB_ID */
  PNValueId value_id;         /* Or PN_INVALID_VALUE_ID */
} PNInstructionBr;

typedef struct PNInstructionSwitch {
  PNInstruction base;
  PNValueId value_id;
  PNBasicBlockId default_bb_id;
  uint32_t num_cases;
  PNSwitchCase* cases;
  PNTypeId type_id;
} PNInstructionSwitch;

typedef struct PNInstructionUnreachable {
  PNInstruction base;
} PNInstructionUnreachable;

typedef struct PNInstructionPhi {
  PNInstruction base;
  PNValueId result_value_id;
  uint32_t num_incoming;
  PNPhiIncoming* incoming;
  PNTypeId type_id;
} PNInstructionPhi;

typedef struct PNInstructionAlloca {
  PNInstruction base;
  PNValueId result_value_id;
  PNValueId size_id;
  PNAlignment alignment;
} PNInstructionAlloca;

typedef struct PNInstructionLoad {
  PNInstruction base;
  PNValueId result_value_id;
  PNValueId src_id;
  PNAlignment alignment;
  PNTypeId type_id;
} PNInstructionLoad;

typedef struct PNInstructionStore {
  PNInstruction base;
  PNValueId dest_id;
  PNValueId value_id;
  PNAlignment alignment;
} PNInstructionStore;

typedef struct PNInstructionCmp2 {
  PNInstruction base;
  PNValueId result_value_id;
  PNValueId value0_id;
  PNValueId value1_id;
  PNCmp2 cmp2_opcode;
} PNInstructionCmp2;

typedef struct PNInstructionVselect {
  PNInstruction base;
  PNValueId result_value_id;
  PNValueId cond_id;
  PNValueId true_value_id;
  PNValueId false_value_id;
} PNInstructionVselect;

typedef struct PNInstructionForwardtyperef {
  PNInstruction base;
  PNValueId value_id;
  PNValueId type_id;
} PNInstructionForwardtyperef;

typedef struct PNInstructionCall {
  PNInstruction base;
  PNValueId result_value_id;
  uint32_t calling_convention;
  PNValueId callee_id;
  uint32_t num_args;
  PNValueId* arg_ids;
  PNTypeId return_type_id;
  PNBool is_indirect;
  PNBool is_tail_call;
} PNInstructionCall;

typedef struct PNPhiUse {
  PNValueId dest_value_id;
  PNPhiIncoming incoming;
} PNPhiUse;

typedef struct PNPhiAssign {
  PNBasicBlockId bb_id;
  PNValueId source_value_id;
  PNValueId dest_value_id;
} PNPhiAssign;

typedef struct PNBasicBlock {
  uint32_t num_instructions;
  PNInstruction** instructions;
  uint32_t num_pred_bbs;
  PNBasicBlockId* pred_bb_ids;
  uint32_t num_succ_bbs;
  PNBasicBlockId* succ_bb_ids;
  uint32_t num_uses;
  PNValueId* uses;
  uint32_t num_phi_uses;
  PNPhiUse* phi_uses;
  uint32_t num_phi_assigns;
  PNPhiAssign* phi_assigns;
  PNValueId first_def_id; /* Or PN_INVALID_VALUE_ID */
  PNValueId last_def_id;  /* Or PN_INVALID_VALUE_ID */
  uint32_t num_livein;
  PNValueId* livein;
  uint32_t num_liveout;
  PNValueId* liveout;
} PNBasicBlock;

typedef union PNRuntimeValue {
  int8_t i8;
  uint8_t u8;
  int16_t i16;
  uint16_t u16;
  int32_t i32;
  uint32_t u32;
  int64_t i64;
  uint64_t u64;
  float f32;
  double f64;
} PNRuntimeValue;

typedef struct PNConstant {
  PNConstantCode code;
  PNTypeId type_id;
  PNBasicType basic_type;
  PNRuntimeValue value;
} PNConstant;

typedef enum PNValueCode {
  PN_VALUE_CODE_FUNCTION,
  PN_VALUE_CODE_GLOBAL_VAR,
  PN_VALUE_CODE_CONSTANT,
  PN_VALUE_CODE_FUNCTION_ARG,
  PN_VALUE_CODE_LOCAL_VAR,
} PNValueCode;

typedef struct PNValue {
  PNValueCode code;
  /* Index into the array for the given value code.
   *   PN_VALUE_CODE_FUNCTION -> PNModule.functions
   *   PN_VALUE_CODE_GLOBAL_VAR -> PNModule.global_vars
   *   PN_VALUE_CODE_CONSTANT -> PNFunction.constants
   *   PN_VALUE_CODE_FUNCTION_ARG -> function argument index
   *   PN_VALUE_CODE_LOCAL_VAR -> PNFunction instruction index
   */
  uint32_t index;
  PNTypeId type_id;
} PNValue;

typedef struct PNFunction {
  char* name;
  PNTypeId type_id;
  PNIntrinsicId intrinsic_id;
  uint32_t num_args;
  uint32_t calling_convention;
  PNBool is_proto;
  uint32_t linkage;
  uint32_t num_constants;
  PNConstant* constants;
  uint32_t num_bbs;
  PNBasicBlock* bbs;
  uint32_t num_values;
  PNValue* values;
} PNFunction;

typedef struct PNType {
  PNTypeCode code;
  PNBasicType basic_type;
  union {
    /* PN_TYPE_CODE_INTEGER */
    uint32_t width;
    /* PN_TYPE_CODE_FUNCTION */
    struct {
      PNBool is_varargs;
      PNTypeId return_type;
      uint32_t num_args;
      PNTypeId* arg_types;
    };
  };
} PNType;

typedef struct PNGlobalVar {
  uint32_t num_initializers;
  PNAlignment alignment;
  uint32_t offset;
  PNBool is_constant;
} PNGlobalVar;

typedef struct PNMemory {
  void* data;
  uint32_t size;
  uint32_t globalvar_start;
  uint32_t globalvar_end;
  uint32_t startinfo_start;
  uint32_t startinfo_end;
  uint32_t heap_start;
  uint32_t stack_end;
} PNMemory;

typedef struct PNModule {
  uint32_t version;
  uint32_t num_functions;
  PNFunction* functions;
  uint32_t num_types;
  PNType* types;
  uint32_t num_global_vars;
  PNGlobalVar* global_vars;
  uint32_t num_values;
  PNValue* values;
  PNFunctionId known_functions[PN_MAX_INTRINSICS];

  PNAllocator allocator;
  PNAllocator value_allocator;
  PNAllocator instruction_allocator;
  PNAllocator temp_allocator;

  /* Stored here so global variable data can be written directly. */
  PNMemory* memory;
} PNModule;

#if PN_CALCULATE_LIVENESS
typedef struct PNLivenessState {
  PNBitSet* livein;
  PNBitSet* liveout;
} PNLivenessState;
#endif /* PN_CALCULATE_LIVENESS */

typedef struct PNBlockInfoContext {
  uint32_t num_abbrevs;
  PNBlockAbbrevs block_abbrev_map[PN_MAX_BLOCK_IDS];
  PNBool use_relative_ids;
} PNBlockInfoContext;

typedef struct PNLocation {
  PNFunctionId function_id;
  PNBasicBlockId bb_id;
  PNInstructionId instruction_id;
} PNLocation;

typedef struct PNCallFrame {
  PNLocation location;
  PNAllocatorMark mark;
  PNRuntimeValue* function_values;
  struct PNCallFrame* parent;
  struct PNJmpBuf* jmpbuf_head;
  uint32_t memory_stack_top; /* Grows down */
} PNCallFrame;

typedef struct PNJmpBuf {
  PNJmpBufId id;
  PNCallFrame frame;
  struct PNJmpBuf* next;
} PNJmpBuf;

typedef struct PNThread {
  PNAllocator allocator;
  struct PNExecutor* executor;
  PNCallFrame* current_call_frame;
  struct PNThread* next;
  struct PNThread* prev;
  PNThreadState state;
  PNFutexState futex_state;
  uint32_t tls;
  uint32_t id;

  uint32_t wait_addr;
  PNBool has_timeout;
  uint64_t timeout_sec;
  uint32_t timeout_usec;
} PNThread;

typedef struct PNExecutor {
  PNModule* module;
  PNMemory* memory;
  PNRuntimeValue* module_values;
  PNThread main_thread;
  PNThread* dead_threads;
  PNCallFrame sentinel_frame;
  PNBitSet mapped_pages;
  PNAllocator allocator;
  PNJmpBufId next_jmpbuf_id;
  uint32_t next_thread_id;
  uint32_t heap_end; /* Grows up */
  int32_t exit_code;
  PNBool exiting;
} PNExecutor;

#if PN_TIMERS
void pn_timespec_check(struct timespec* a) {
  assert(a->tv_sec >= 0);
  assert(a->tv_nsec >= 0 && a->tv_nsec < PN_NANOSECONDS_IN_A_SECOND);
}

void pn_timespec_subtract(struct timespec* result,
                          struct timespec* a,
                          struct timespec* b) {
  pn_timespec_check(a);
  pn_timespec_check(b);
  result->tv_sec = a->tv_sec - b->tv_sec;
  result->tv_nsec = a->tv_nsec - b->tv_nsec;
  if (result->tv_nsec < 0) {
    result->tv_sec--;
    result->tv_nsec += PN_NANOSECONDS_IN_A_SECOND;
  }
  pn_timespec_check(result);
}

void pn_timespec_add(struct timespec* result,
                     struct timespec* a,
                     struct timespec* b) {
  pn_timespec_check(a);
  pn_timespec_check(b);
  result->tv_sec = a->tv_sec + b->tv_sec;
  result->tv_nsec = a->tv_nsec + b->tv_nsec;
  if (result->tv_nsec >= PN_NANOSECONDS_IN_A_SECOND) {
    result->tv_sec++;
    result->tv_nsec -= PN_NANOSECONDS_IN_A_SECOND;
  }
  pn_timespec_check(result);
}

double pn_timespec_to_double(struct timespec* t) {
  return (double)t->tv_sec + (double)t->tv_nsec / PN_NANOSECONDS_IN_A_SECOND;
}

#endif /* PN_TIMERS */

static void* pn_malloc(size_t size) {
  void* ret = malloc(size);
  if (!ret) {
    PN_FATAL("Out of memory.\n");
  }
  return ret;
}

static void* pn_realloc(void* p, size_t size) {
  void* ret = realloc(p, size);
  if (!ret) {
    PN_FATAL("Out of memory.\n");
  }
  return ret;
}

static void* pn_calloc(size_t nmemb, size_t size) {
  void* ret = calloc(nmemb, size);
  if (!ret) {
    PN_FATAL("Out of memory.\n");
  }
  return ret;
}

static char* pn_strdup(char* s) {
  char* ret = strdup(s);
  if (!ret) {
    PN_FATAL("Out of memory.\n");
  }
  return ret;
}

static void pn_free(void* p) {
  free(p);
}

#undef malloc
#define malloc DONT_CALL_THIS_FUNCTION
#undef realloc
#define realloc DONT_CALL_THIS_FUNCTION
#undef calloc
#define calloc DONT_CALL_THIS_FUNCTION
#undef strdup
#define strdup DONT_CALL_THIS_FUNCTION
#undef free
#define free DONT_CALL_THIS_FUNCTION
#undef printf
#define printf DONT_CALL_THIS_FUNCTION

static size_t pn_max(size_t x, size_t y) {
  return x > y ? x : y;
}

static PNBool pn_is_power_of_two(size_t value) {
  return (value > 0) && ((value) & (value - 1)) == 0;
}

static size_t pn_next_power_of_two(size_t value) {
  size_t ret = 1;
  while (ret < value) {
    ret <<= 1;
  }

  return ret;
}

static inline size_t pn_align_down(size_t size, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return size & ~(align - 1);
}

static inline void* pn_align_down_pointer(void* p, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return (void*)((intptr_t)p & ~((intptr_t)align - 1));
}

static inline size_t pn_align_up(size_t size, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return (size + align - 1) & ~(align - 1);
}

static inline void* pn_align_up_pointer(void* p, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return (void*)(((intptr_t)p + align - 1) & ~((intptr_t)align - 1));
}

static inline PNBool pn_is_aligned(size_t size, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return (size & (align - 1)) == 0;
}

static inline PNBool pn_is_aligned_pointer(void* p, uint32_t align) {
  assert(pn_is_power_of_two(align));
  return ((intptr_t)p & (intptr_t)(align - 1)) == 0;
}

static inline uint32_t pn_ctz(uint32_t x) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_ctz(x);
#else
  /* See https://en.wikipedia.org/wiki/Find_first_set */
  if (x == 0) return 32;
  uint32_t n = 0;
  if ((x & 0xffff) == 0) { n += 16; x >>= 16; }
  if ((x & 0x00ff) == 0) { n += 8;  x >>= 8; }
  if ((x & 0x000f) == 0) { n += 4;  x >>= 4; }
  if ((x & 0x0003) == 0) { n += 2;  x >>= 2; }
  if ((x & 0x0001) == 0) { n += 1; }
  return n;
#endif
}

static inline uint32_t pn_clz(uint32_t x) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_clz(x);
#else
  /* See https://en.wikipedia.org/wiki/Find_first_set */
  if (x == 0) return 32;
  uint32_t n = 0;
  if ((x & 0xffff0000) == 0) { n += 16; x <<= 16; }
  if ((x & 0xff000000) == 0) { n += 8;  x <<= 8; }
  if ((x & 0xf0000000) == 0) { n += 4;  x <<= 4; }
  if ((x & 0xc0000000) == 0) { n += 2;  x <<= 2; }
  if ((x & 0x80000000) == 0) { n += 1; }
  return n;
#endif
}

static inline uint32_t pn_popcount(uint32_t x) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_popcount(x);
#else
  /* See
   * http://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
   */
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  return (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
#endif
}

static void pn_allocator_init(PNAllocator* allocator,
                              size_t min_chunk_size,
                              const char* name) {
  assert(pn_is_power_of_two(min_chunk_size));
  assert(min_chunk_size > sizeof(PNAllocatorChunk));

  allocator->name = name;
  allocator->chunk_head = NULL;
  allocator->last_alloc = NULL;
  allocator->min_chunk_size = min_chunk_size;
  allocator->total_used = 0;
  allocator->internal_fragmentation = 0;
}

static PNAllocatorChunk* pn_allocator_new_chunk(PNAllocator* allocator,
                                                size_t initial_alloc_size,
                                                uint32_t align) {
  PN_CHECK(initial_alloc_size < 0x80000000);
  if (allocator->chunk_head) {
    allocator->internal_fragmentation +=
        (allocator->chunk_head->end - allocator->chunk_head->current);
  }

  size_t chunk_size = pn_next_power_of_two(
      pn_max(initial_alloc_size + sizeof(PNAllocatorChunk) + align - 1,
             allocator->min_chunk_size));
  PNAllocatorChunk* chunk = pn_malloc(chunk_size);
  assert(pn_is_aligned_pointer(chunk, sizeof(void*)));

  chunk->current =
      pn_align_up_pointer((void*)chunk + sizeof(PNAllocatorChunk), align);
  chunk->end = (void*)chunk + chunk_size;
  chunk->next = allocator->chunk_head;
  allocator->chunk_head = chunk;
  return chunk;
}

static void* pn_allocator_alloc(PNAllocator* allocator,
                                size_t size,
                                uint32_t align) {
  PNAllocatorChunk* chunk = allocator->chunk_head;
  void* ret;
  void* new_current;

  if (chunk) {
    ret = pn_align_up_pointer(chunk->current, align);
    new_current = ret + size;
    PN_CHECK(new_current >= ret);
  }

  if (!chunk || new_current > chunk->end) {
    chunk = pn_allocator_new_chunk(allocator, size, align);
    ret = pn_align_up_pointer(chunk->current, align);
    new_current = ret + size;
    PN_CHECK(new_current >= ret);
    assert(new_current <= chunk->end);
  }

  chunk->current = new_current;
  allocator->last_alloc = ret;
  allocator->total_used += size;
  return ret;
}

static void* pn_allocator_allocz(PNAllocator* allocator,
                                 size_t size,
                                 uint32_t align) {
  void* p = pn_allocator_alloc(allocator, size, align);
  memset(p, 0, size);
  return p;
}

static void* pn_allocator_realloc_add(PNAllocator* allocator,
                                      void** p,
                                      size_t add_size,
                                      uint32_t align) {
  if (!*p) {
    *p = pn_allocator_alloc(allocator, add_size, align);
    return *p;
  }

  if (*p != allocator->last_alloc) {
    PN_FATAL(
        "Attempting to realloc, but it was not the last allocation:\n"
        "p = %p, last_alloc = %p\n",
        *p, allocator->last_alloc);
  }

  PNAllocatorChunk* chunk = allocator->chunk_head;
  assert(chunk);
  void* ret = chunk->current;
  void* new_current = chunk->current + add_size;
  PN_CHECK(new_current > chunk->current);

  if (new_current > chunk->end) {
    /* Doesn't fit, alloc a new chunk */
    size_t old_size = chunk->current - *p;
    size_t new_size = old_size + add_size;
    chunk = pn_allocator_new_chunk(allocator, new_size, align);

    assert(chunk->current + new_size <= chunk->end);
    memcpy(chunk->current, *p, old_size);
    *p = chunk->current;
    ret = chunk->current + old_size;
    new_current = ret + add_size;
  }

  chunk->current = new_current;
  allocator->last_alloc = *p;
  allocator->total_used += add_size;
  return ret;
}

static size_t pn_allocator_last_alloc_size(PNAllocator* allocator) {
  PNAllocatorChunk* chunk = allocator->chunk_head;
  assert(chunk);
  return chunk->current - allocator->last_alloc;
}

static PNAllocatorMark pn_allocator_mark(PNAllocator* allocator) {
  PNAllocatorMark mark;
  mark.current = allocator->chunk_head ? allocator->chunk_head->current : 0;
  mark.last_alloc = allocator->last_alloc;
  mark.total_used = allocator->total_used;
  mark.internal_fragmentation = allocator->internal_fragmentation;
  return mark;
}

static void pn_allocator_reset_to_mark(PNAllocator* allocator,
                                       PNAllocatorMark mark) {
  /* Free chunks until last_alloc is found */
  PNAllocatorChunk* chunk = allocator->chunk_head;
  while (chunk) {
    if (mark.last_alloc >= (void*)chunk &&
        mark.last_alloc < (void*)chunk->end) {
      break;
    }

    PNAllocatorChunk* next = chunk->next;
    pn_free(chunk);
    chunk = next;
  }

  if (chunk) {
    chunk->current = mark.current;
  }
  allocator->chunk_head = chunk;
  allocator->last_alloc = mark.last_alloc;
  allocator->total_used = mark.total_used;
  allocator->internal_fragmentation = mark.internal_fragmentation;
}

static void pn_bitset_init(PNAllocator* allocator,
                           PNBitSet* bitset,
                           int32_t size) {
  bitset->num_words = (size + 31) >> 5;
  bitset->words = pn_allocator_allocz(
      allocator, sizeof(uint32_t) * bitset->num_words, sizeof(uint32_t));
}

static void pn_bitset_set(PNBitSet* bitset, uint32_t bit, PNBool set) {
  uint32_t word = bit >> 5;
  uint32_t mask = 1 << (bit & 31);
  assert(word < bitset->num_words);

  if (set) {
    bitset->words[word] |= mask;
  } else {
    bitset->words[word] &= ~mask;
  }
}

static PNBool pn_bitset_is_set(PNBitSet* bitset, uint32_t bit) {
  uint32_t word = bit >> 5;
  uint32_t mask = 1 << (bit & 31);
  assert(word < bitset->num_words);

  return (bitset->words[word] & mask) != 0;
}

static uint32_t pn_bitset_num_bits_set(PNBitSet* bitset) {
  uint32_t i;
  uint32_t result = 0;
  for (i = 0; i < bitset->num_words; ++i) {
    result += pn_popcount(bitset->words[i]);
  }
  return result;
}

static uint32_t pn_decode_char6(uint32_t value) {
  const char data[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._";
  if (value >= PN_ARRAY_SIZE(data)) {
    PN_FATAL("Invalid char6 value: %u\n", value);
  }

  return data[value];
}

static int32_t pn_decode_sign_rotated_value(uint32_t value) {
  if ((value & 1) == 0) {
    return value >> 1;
  }
  if (value != 1) {
    return -(value >> 1);
  } else {
    return INT_MIN;
  }
}

static int64_t pn_decode_sign_rotated_value_int64(uint64_t value) {
  if ((value & 1) == 0) {
    return value >> 1;
  }
  if (value != 1) {
    return -(value >> 1);
  } else {
    return INT64_MIN;
  }
}

static uint32_t pn_builtin_to_pointer(PNBuiltinId builtin_id) {
  return builtin_id << 2;
}

static uint32_t pn_function_id_to_pointer(PNFunctionId function_id) {
  return (function_id + PN_MAX_BUILTINS) << 2;
}

static PNFunctionId pn_function_pointer_to_index(uint32_t fp) {
  return fp >> 2;
}

static void pn_bitstream_init(PNBitStream* bs, void* data, uint32_t data_len) {
  bs->data = data;
  bs->data_len = data_len;
  bs->curword = 0;
  bs->curword_bits = 0;
  bs->bit_offset = 0;
}

static uint32_t pn_bitstream_read_frac_bits(PNBitStream* bs, int num_bits) {
  PN_CHECK(num_bits <= bs->curword_bits);
  uint32_t result;
  if (num_bits == 32) {
    result = bs->curword;
    bs->curword = 0;
  } else {
    result = bs->curword & ((1 << num_bits) - 1);
    bs->curword >>= num_bits;
  }
  bs->curword_bits -= num_bits;
  bs->bit_offset += num_bits;
  return result;
}

static void pn_bitstream_fill_curword(PNBitStream* bs) {
  uint32_t byte_offset = bs->bit_offset >> 3;
  if (byte_offset + 4 < bs->data_len) {
    bs->curword_bits = 32;
    bs->curword = *(uint32_t*)(bs->data + byte_offset);
  } else {
    PN_CHECK(byte_offset <= bs->data_len);
    bs->curword_bits = (bs->data_len - byte_offset) * 8;
    if (bs->curword_bits) {
      bs->curword = *(uint32_t*)(bs->data + byte_offset);
    }
  }
  assert(bs->curword_bits <= 32);
}

static uint32_t pn_bitstream_read(PNBitStream* bs, int num_bits) {
  PN_CHECK(num_bits <= 32);
  if (num_bits <= bs->curword_bits) {
    return pn_bitstream_read_frac_bits(bs, num_bits);
  }

  uint32_t result = bs->curword;
  int bits_read = bs->curword_bits;
  int bits_left = num_bits - bs->curword_bits;
  bs->bit_offset += bits_read;
  pn_bitstream_fill_curword(bs);
  result |= pn_bitstream_read_frac_bits(bs, bits_left) << bits_read;
  return result;
}

static uint32_t pn_bitstream_read_vbr(PNBitStream* bs, int num_bits) {
  uint64_t piece = pn_bitstream_read(bs, num_bits);
  uint64_t hi_mask = 1 << (num_bits - 1);
  if ((piece & hi_mask) == 0) {
    return piece;
  }

  uint64_t lo_mask = hi_mask - 1;
  uint64_t result = 0;
  int shift = 0;
  while (1) {
    PN_CHECK(shift < 64);
    result |= (piece & lo_mask) << shift;
    if ((piece & hi_mask) == 0) {
      /* The value should be < 2**32, or should be sign-extended so the top
       * 32-bits are all 1 */
      PN_CHECK(result <= UINT32_MAX ||
               ((result & 0x80000000) && ((result >> 32) == UINT32_MAX)));
      return result;
    }
    shift += num_bits - 1;
    piece = pn_bitstream_read(bs, num_bits);
  }
}

static uint64_t pn_bitstream_read_vbr_uint64(PNBitStream* bs, int num_bits) {
  uint32_t piece = pn_bitstream_read(bs, num_bits);
  uint32_t hi_mask = 1 << (num_bits - 1);
  if ((piece & hi_mask) == 0) {
    return piece;
  }

  uint32_t lo_mask = hi_mask - 1;
  uint64_t result = 0;
  int shift = 0;
  while (1) {
    PN_CHECK(shift < 64);
    result |= (uint64_t)(piece & lo_mask) << shift;
    if ((piece & hi_mask) == 0) {
      return result;
    }
    shift += num_bits - 1;
    piece = pn_bitstream_read(bs, num_bits);
  }
}

static void pn_bitstream_seek_bit(PNBitStream* bs, uint32_t bit_offset) {
  bs->bit_offset = pn_align_down(bit_offset, 32);
  pn_bitstream_fill_curword(bs);

  bit_offset &= 31;
  if (bit_offset) {
    /* Offset is not aligned, read the unaliged bits */
    pn_bitstream_read_frac_bits(bs, bit_offset);
  }
}

static void pn_bitstream_skip_bytes(PNBitStream* bs, int num_bytes) {
  pn_bitstream_seek_bit(bs, bs->bit_offset + num_bytes * 8);
}

static void pn_bitstream_align_32(PNBitStream* bs) {
  pn_bitstream_seek_bit(bs, pn_align_up(bs->bit_offset, 32));
}

static PNBool pn_bitstream_at_end(PNBitStream* bs) {
  uint32_t byte_offset = bs->bit_offset >> 3;
  return byte_offset == bs->data_len;
}

static void pn_context_fix_value_ids(PNBlockInfoContext* context,
                                     PNValueId rel_id,
                                     uint32_t count,
                                     ...) {
  if (!context->use_relative_ids)
    return;

  va_list args;
  va_start(args, count);
  uint32_t i;
  for (i = 0; i < count; ++i) {
    PNValueId* value_id = va_arg(args, PNValueId*);
    *value_id = rel_id - *value_id;
  }
  va_end(args);
}

static PNType* pn_module_get_type(PNModule* module, PNTypeId type_id) {
  if (type_id < 0 || type_id >= module->num_types) {
    PN_FATAL("accessing invalid type %d (max %d)\n", type_id,
             module->num_types);
  }

  return &module->types[type_id];
}

static PNTypeId pn_module_find_integer_type(PNModule* module, int width) {
  uint32_t n;
  for (n = 0; n < module->num_types; ++n) {
    PNType* type = &module->types[n];
    if (type->code == PN_TYPE_CODE_INTEGER) {
      if (type->width == width) {
        return n;
      }
    }
  }

  return PN_INVALID_TYPE_ID;
}

static PNTypeId pn_module_find_pointer_type(PNModule* module) {
  return pn_module_find_integer_type(module, 32);
}

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

static PNFunction* pn_module_get_function(PNModule* module,
                                          PNFunctionId function_id) {
  if (function_id >= module->num_functions) {
    PN_FATAL("accessing invalid function %d (max %d)\n", function_id,
             module->num_functions);
  }

  return &module->functions[function_id];
}

static PNConstant* pn_function_get_constant(PNFunction* function,
                                            PNConstantId constant_id) {
  if (constant_id >= function->num_constants) {
    PN_FATAL("accessing invalid constant %d (max %d)\n", constant_id,
             function->num_constants);
  }

  return &function->constants[constant_id];
}

static PNConstant* pn_function_append_constant(PNModule* module,
                                               PNFunction* function,
                                               PNConstantId* out_constant_id) {
  *out_constant_id = function->num_constants++;
  return pn_allocator_realloc_add(&module->allocator,
                                  (void**)&function->constants,
                                  sizeof(PNConstant), PN_DEFAULT_ALIGN);
}

static PNGlobalVar* pn_module_get_global_var(PNModule* module,
                                             PNGlobalVarId global_var_id) {
  if (global_var_id >= module->num_global_vars) {
    PN_FATAL("accessing invalid global_var %d (max %d)\n", global_var_id,
             module->num_global_vars);
  }

  return &module->global_vars[global_var_id];
}

static PNValue* pn_module_get_value(PNModule* module, PNValueId value_id) {
  if (value_id >= module->num_values) {
    PN_FATAL("accessing invalid value %d (max %d)\n", value_id,
             module->num_values);
  }

  return &module->values[value_id];
}

static PNValue* pn_module_append_value(PNModule* module,
                                       PNValueId* out_value_id) {
  *out_value_id = module->num_values++;
  PNValue* ret = pn_allocator_realloc_add(&module->value_allocator,
                                          (void**)&module->values,
                                          sizeof(PNValue), PN_DEFAULT_ALIGN);
  return ret;
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

#if PN_UNALIGNED_MEMORY_ACCESS

#define DEFINE_MEMORY_READ(ty, ctype)                                   \
  static ctype pn_memory_read_##ty(PNMemory* memory, uint32_t offset) { \
    pn_memory_check(memory, offset, sizeof(ctype));                     \
    ctype* m = (ctype*)(memory->data + offset);                         \
    PN_TRACE(MEMORY, "     read." #ty " [%8u] >= " PN_FORMAT_##ty "\n", \
             offset, *m);                                               \
    return *m;                                                          \
  }

#define DEFINE_MEMORY_WRITE(ty, ctype)                                  \
  static void pn_memory_write_##ty(PNMemory* memory, uint32_t offset,   \
                                   ctype value) {                       \
    pn_memory_check(memory, offset, sizeof(value));                     \
    ctype* m = (ctype*)(memory->data + offset);                         \
    *m = value;                                                         \
    PN_TRACE(MEMORY, "    write." #ty " [%8u] <= " PN_FORMAT_##ty "\n", \
             offset, value);                                            \
  }

#else

#define DEFINE_MEMORY_READ(ty, ctype)                                   \
  static ctype pn_memory_read_##ty(PNMemory* memory, uint32_t offset) { \
    pn_memory_check(memory, offset, sizeof(ctype));                     \
    ctype* m = (ctype*)(memory->data + offset);                         \
    ctype ret;                                                          \
    memcpy(&ret, m, sizeof(ty));                                        \
    return ret;                                                         \
  }

#define DEFINE_MEMORY_WRITE(ty, ctype)                                \
  static void pn_memory_write_##ty(PNMemory* memory, uint32_t offset, \
                                   ctype value) {                     \
    pn_memory_check(memory, offset, sizeof(value));                   \
    ctype* m = (ctype*)(memory->data + offset);                       \
    memcpy(memory32, &value, sizeof(value));                          \
  }

#endif

DEFINE_MEMORY_READ(i8, int8_t)
DEFINE_MEMORY_READ(u8, uint8_t)
DEFINE_MEMORY_READ(i16, int16_t)
DEFINE_MEMORY_READ(u16, uint16_t)
DEFINE_MEMORY_READ(i32, int32_t)
DEFINE_MEMORY_READ(u32, uint32_t)
DEFINE_MEMORY_READ(i64, int64_t)
DEFINE_MEMORY_READ(u64, uint64_t)
DEFINE_MEMORY_READ(f32, float)
DEFINE_MEMORY_READ(f64, double)

DEFINE_MEMORY_WRITE(i8, int8_t)
DEFINE_MEMORY_WRITE(u8, uint8_t)
DEFINE_MEMORY_WRITE(i16, int16_t)
DEFINE_MEMORY_WRITE(u16, uint16_t)
DEFINE_MEMORY_WRITE(i32, int32_t)
DEFINE_MEMORY_WRITE(u32, uint32_t)
DEFINE_MEMORY_WRITE(i64, int64_t)
DEFINE_MEMORY_WRITE(u64, uint64_t)
DEFINE_MEMORY_WRITE(f32, float)
DEFINE_MEMORY_WRITE(f64, double)

#undef DEFINE_MEMORY_READ
#undef DEFINE_MEMORY_WRITE

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

static uint32_t pn_function_num_values(PNModule* module, PNFunction* function) {
  return module->num_values + function->num_values;
}

static PNValue* pn_function_get_value(PNModule* module,
                                      PNFunction* function,
                                      PNValueId value_id) {
  if (value_id < module->num_values) {
    return &module->values[value_id];
  }

  value_id -= module->num_values;

  if (value_id < function->num_values) {
    return &function->values[value_id];
  }

  PN_FATAL("accessing invalid value %d (max %d)\n",
           value_id + module->num_values,
           module->num_values + function->num_values);
}

static PNValue* pn_function_append_value(PNModule* module,
                                         PNFunction* function,
                                         PNValueId* out_value_id) {
  uint32_t index = function->num_values++;
  *out_value_id = module->num_values + index;
  PNValue* ret = pn_allocator_realloc_add(&module->value_allocator,
                                          (void**)&function->values,
                                          sizeof(PNValue), PN_DEFAULT_ALIGN);
  return ret;
}

static void* pn_basic_block_append_instruction(
    PNModule* module,
    PNBasicBlock* bb,
    uint32_t instruction_size,
    PNInstructionId* out_instruction_id) {
  *out_instruction_id = bb->num_instructions;
  pn_allocator_realloc_add(&module->allocator, (void**)&bb->instructions,
                           sizeof(PNInstruction*), sizeof(PNInstruction*));
  void* p = pn_allocator_allocz(&module->instruction_allocator,
                                instruction_size, PN_DEFAULT_ALIGN);
  bb->instructions[*out_instruction_id] = p;
  bb->num_instructions++;
  return p;
}

#define PN_BASIC_BLOCK_APPEND_INSTRUCTION(type, module, bb, id) \
  (type*) pn_basic_block_append_instruction(module, bb, sizeof(type), id)

static void pn_basic_block_list_append(PNModule* module,
                                       PNBasicBlockId** bb_list,
                                       uint32_t* num_els,
                                       PNBasicBlockId bb_id) {
  pn_allocator_realloc_add(&module->allocator, (void**)bb_list,
                           sizeof(PNBasicBlockId), sizeof(PNBasicBlockId));
  (*bb_list)[(*num_els)++] = bb_id;
}

#if PN_TRACING
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
  return pn_type_describe_all(module, type_id, NULL, PN_FALSE, PN_FALSE);
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
    for (b = first_bit; b <= last_bit; ++b) {
      if (word & (1 << b)) {
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

static void pn_function_calculate_pred_bbs(PNModule* module,
                                           PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_PRED_BBS);
  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;
    for (m = 0; m < bb->num_succ_bbs; ++m) {
      PNBasicBlockId succ_bb_id = bb->succ_bb_ids[m];
      assert(succ_bb_id < function->num_bbs);
      PNBasicBlock* succ_bb = &function->bbs[succ_bb_id];
      succ_bb->num_pred_bbs++;
    }
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    bb->pred_bb_ids = pn_allocator_alloc(
        &module->allocator, sizeof(PNBasicBlockId) * bb->num_pred_bbs,
        sizeof(PNBasicBlockId));
    bb->num_pred_bbs = 0;
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;
    for (m = 0; m < bb->num_succ_bbs; ++m) {
      PNBasicBlockId succ_bb_id = bb->succ_bb_ids[m];
      PNBasicBlock* succ_bb = &function->bbs[succ_bb_id];
      succ_bb->pred_bb_ids[succ_bb->num_pred_bbs++] = n;
    }
  }
  PN_END_TIME(CALCULATE_PRED_BBS);
}

#if PN_CALCULATE_LIVENESS
static void pn_basic_block_calculate_liveness_per_value(
    PNModule* module,
    PNFunction* function,
    PNLivenessState* state,
    PNBasicBlockId initial_bb_id,
    PNValueId rel_id) {
  PNValueId value_id = module->num_values + rel_id;

  /* Allocate enough space for any predecessor chain. num_bbs is always an
   * upper bound */
  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);
  PNBasicBlockId* bb_id_stack = pn_allocator_alloc(
      &module->temp_allocator, sizeof(PNBasicBlockId) * function->num_bbs,
      sizeof(PNBasicBlockId));
  bb_id_stack[0] = initial_bb_id;
  uint32_t stack_top = 1;

  while (stack_top > 0) {
    PNBasicBlockId bb_id = bb_id_stack[--stack_top];
    PNBasicBlock* bb = &function->bbs[bb_id];
    if (value_id >= bb->first_def_id && value_id <= bb->last_def_id) {
      /* Value killed at definition. */
      continue;
    }

    if (pn_bitset_is_set(&state->livein[bb_id], rel_id)) {
      /* Already processed. */
      continue;
    }

    pn_bitset_set(&state->livein[bb_id], rel_id, PN_TRUE);

    uint32_t n;
    for (n = 0; n < bb->num_pred_bbs; ++n) {
      PNBasicBlockId pred_bb_id = bb->pred_bb_ids[n];
      pn_bitset_set(&state->liveout[pred_bb_id], rel_id, PN_TRUE);

      assert(stack_top < function->num_bbs);
      bb_id_stack[stack_top++] = pred_bb_id;
    }
  }

  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
}

static void pn_basic_block_calculate_liveness(PNModule* module,
                                              PNFunction* function,
                                              PNLivenessState* state,
                                              PNBasicBlockId bb_id) {
  PNBasicBlock* bb = &function->bbs[bb_id];
  uint32_t n;
  for (n = 0; n < bb->num_phi_uses; ++n) {
    PNPhiIncoming* incoming = &bb->phi_uses[n].incoming;
    if (incoming->value_id <
        module->num_values + function->num_args + function->num_constants) {
      break;
    }

    PNValueId rel_id = incoming->value_id - module->num_values;
    PNBasicBlockId pred_bb_id = incoming->bb_id;
    //    pn_bitset_set(&state->liveout[pred_bb_id], rel_id, PN_TRUE);
    pn_basic_block_calculate_liveness_per_value(module, function, state,
                                                pred_bb_id, rel_id);
  }

  for (n = 0; n < bb->num_uses; ++n) {
    PNValueId value_id = bb->uses[n];
    PNValueId rel_id = value_id - module->num_values;
    pn_basic_block_calculate_liveness_per_value(module, function, state, bb_id,
                                                rel_id);
  }
}

static void pn_function_calculate_liveness(PNModule* module,
                                           PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_LIVENESS);
  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);

  PNLivenessState state;
  state.livein = pn_allocator_alloc(&module->temp_allocator,
                                    sizeof(PNBitSet) * function->num_bbs,
                                    PN_DEFAULT_ALIGN);
  state.liveout = pn_allocator_alloc(&module->temp_allocator,
                                     sizeof(PNBitSet) * function->num_bbs,
                                     PN_DEFAULT_ALIGN);

  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    pn_bitset_init(&module->temp_allocator, &state.livein[n],
                   function->num_values);
    pn_bitset_init(&module->temp_allocator, &state.liveout[n],
                   function->num_values);
  }

  for (n = function->num_bbs; n > 0; --n) {
    PNBasicBlockId bb_id = n - 1;
    pn_basic_block_calculate_liveness(module, function, &state, bb_id);
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;

    uint32_t livein_bits_set = pn_bitset_num_bits_set(&state.livein[n]);
    if (livein_bits_set) {
      bb->num_livein = 0;
      bb->livein = pn_allocator_alloc(&module->allocator,
                                      sizeof(PNValueId) * livein_bits_set,
                                      sizeof(PNValueId));

      for (m = 0; m < function->num_values; ++m) {
        if (pn_bitset_is_set(&state.livein[n], m)) {
          bb->livein[bb->num_livein++] = module->num_values + m;
        }
      }
    }

    uint32_t liveout_bits_set = pn_bitset_num_bits_set(&state.liveout[n]);
    if (liveout_bits_set) {
      bb->num_liveout = 0;
      bb->liveout = pn_allocator_alloc(&module->allocator,
                                       sizeof(PNValueId) * liveout_bits_set,
                                       sizeof(PNValueId));

      for (m = 0; m < function->num_values; ++m) {
        if (pn_bitset_is_set(&state.liveout[n], m)) {
          bb->liveout[bb->num_liveout++] = module->num_values + m;
        }
      }
    }
  }

  pn_allocator_reset_to_mark(&module->temp_allocator, mark);
  PN_END_TIME(CALCULATE_LIVENESS);
}
#endif /* PN_CALCULATE_LIVENESS */

static void pn_function_calculate_phi_assigns(PNModule* module,
                                              PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_PHI_ASSIGNS);
  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;
    for (m = 0; m < bb->num_phi_uses; ++m) {
      PNPhiUse* use = &bb->phi_uses[m];
      assert(use->incoming.bb_id < function->num_bbs);

      PNBasicBlock* incoming_bb = &function->bbs[use->incoming.bb_id];
      incoming_bb->num_phi_assigns++;
    }
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];

    bb->phi_assigns = pn_allocator_alloc(
        &module->allocator, sizeof(PNPhiAssign) * bb->num_phi_assigns,
        PN_DEFAULT_ALIGN);
    bb->num_phi_assigns = 0;
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;
    for (m = 0; m < bb->num_phi_uses; ++m) {
      PNPhiUse* use = &bb->phi_uses[m];
      PNBasicBlock* incoming_bb = &function->bbs[use->incoming.bb_id];
      PNPhiAssign* assign =
          &incoming_bb->phi_assigns[incoming_bb->num_phi_assigns++];

      assign->bb_id = n;
      assign->dest_value_id = use->dest_value_id;
      assign->source_value_id = use->incoming.value_id;
    }
  }
  PN_END_TIME(CALCULATE_PHI_ASSIGNS);
}

static void pn_record_reader_init(PNRecordReader* reader,
                                  PNBitStream* bs,
                                  PNBlockAbbrevs* abbrevs,
                                  uint32_t entry) {
  reader->bs = bs;
  reader->abbrevs = abbrevs;
  reader->entry = entry;
  reader->op_index = 0;
  reader->num_values = -1;
  reader->value_index = 0;
}

static PNBool pn_record_read_abbrev(PNRecordReader* reader,
                                    uint32_t* out_value) {
  PN_CHECK(reader->entry - 4 < reader->abbrevs->num_abbrevs);
  PNBlockAbbrev* abbrev = &reader->abbrevs->abbrevs[reader->entry - 4];
  if (reader->op_index >= abbrev->num_ops) {
    return PN_FALSE;
  }

  PNBlockAbbrevOp* op = &abbrev->ops[reader->op_index];

  switch (op->encoding) {
    case PN_ENCODING_LITERAL:
      *out_value = op->value;
      reader->op_index++;
      reader->value_index = 0;
      return PN_TRUE;

    case PN_ENCODING_FIXED:
      *out_value = pn_bitstream_read(reader->bs, op->num_bits);
      reader->op_index++;
      reader->value_index = 0;
      return PN_TRUE;

    case PN_ENCODING_VBR:
      *out_value = pn_bitstream_read_vbr(reader->bs, op->num_bits);
      reader->op_index++;
      reader->value_index = 0;
      return PN_TRUE;

    case PN_ENCODING_ARRAY: {
      if (reader->value_index == 0) {
        /* First read is the number of elements */
        reader->num_values = pn_bitstream_read_vbr(reader->bs, 6);
        PN_CHECK(reader->num_values > 0);
      }

      PNBlockAbbrevOp* elt_op = &abbrev->ops[reader->op_index + 1];
      switch (elt_op->encoding) {
        case PN_ENCODING_LITERAL:
          *out_value = elt_op->value;
          break;

        case PN_ENCODING_FIXED:
          *out_value = pn_bitstream_read(reader->bs, elt_op->num_bits);
          break;

        case PN_ENCODING_VBR:
          *out_value = pn_bitstream_read_vbr(reader->bs, elt_op->num_bits);
          break;

        case PN_ENCODING_CHAR6:
          *out_value = pn_decode_char6(pn_bitstream_read(reader->bs, 6));
          break;

        default:
          PN_FATAL("bad encoding for array element: %d\n", elt_op->encoding);
      }

      if (++reader->value_index == reader->num_values) {
        /* Array encoding uses the following op as the element op. Skip both */
        reader->op_index += 2;
      }
      return PN_TRUE;
    }

    case PN_ENCODING_CHAR6:
      *out_value = pn_decode_char6(pn_bitstream_read(reader->bs, 6));
      reader->op_index++;
      reader->value_index = 0;
      return PN_TRUE;

    case PN_ENCODING_BLOB:
      if (reader->value_index == 0) {
        /* First read is the number of elements */
        reader->num_values = pn_bitstream_read(reader->bs, 6);
        pn_bitstream_align_32(reader->bs);
      }

      /* TODO(binji): optimize? this is aligned, so it should be easy to extract
       * all the data in a buffer.*/
      *out_value = pn_bitstream_read(reader->bs, 8);
      if (reader->value_index++ == reader->num_values) {
        reader->op_index++;
      }
      return PN_TRUE;

    default:
      PN_FATAL("bad encoding: %d\n", op->encoding);
  }
}

static PNBool pn_record_read_abbrev_uint64(PNRecordReader* reader,
                                           uint64_t* out_value) {
  assert(reader->entry - 4 < reader->abbrevs->num_abbrevs);
  PNBlockAbbrev* abbrev = &reader->abbrevs->abbrevs[reader->entry - 4];
  if (reader->op_index >= abbrev->num_ops) {
    return PN_FALSE;
  }

  PNBlockAbbrevOp* op = &abbrev->ops[reader->op_index];

  switch (op->encoding) {
    case PN_ENCODING_LITERAL:
      *out_value = op->value;
      reader->op_index++;
      reader->value_index = 0;
      return PN_TRUE;

    case PN_ENCODING_FIXED:
      *out_value = pn_bitstream_read(reader->bs, op->num_bits);
      reader->op_index++;
      reader->value_index = 0;
      return PN_TRUE;

    case PN_ENCODING_VBR:
      *out_value = pn_bitstream_read_vbr_uint64(reader->bs, op->num_bits);
      reader->op_index++;
      reader->value_index = 0;
      return PN_TRUE;

    case PN_ENCODING_ARRAY: {
      if (reader->value_index == 0) {
        /* First read is the number of elements */
        reader->num_values = pn_bitstream_read_vbr(reader->bs, 6);
      }

      PNBlockAbbrevOp* elt_op = &abbrev->ops[reader->op_index + 1];
      switch (elt_op->encoding) {
        case PN_ENCODING_LITERAL:
          *out_value = elt_op->value;
          break;

        case PN_ENCODING_FIXED:
          *out_value = pn_bitstream_read(reader->bs, elt_op->num_bits);
          break;

        case PN_ENCODING_VBR:
          *out_value =
              pn_bitstream_read_vbr_uint64(reader->bs, elt_op->num_bits);
          break;

        case PN_ENCODING_CHAR6:
          *out_value = pn_decode_char6(pn_bitstream_read(reader->bs, 6));
          break;

        default:
          PN_FATAL("bad encoding for array element: %d\n", elt_op->encoding);
      }

      if (++reader->value_index == reader->num_values) {
        /* Array encoding uses the following op as the element op. Skip both */
        reader->op_index += 2;
      }
      return PN_TRUE;
    }

    case PN_ENCODING_CHAR6:
      *out_value = pn_decode_char6(pn_bitstream_read(reader->bs, 6));
      reader->op_index++;
      reader->value_index = 0;
      return PN_TRUE;

    case PN_ENCODING_BLOB:
      if (reader->value_index == 0) {
        /* First read is the number of elements */
        reader->num_values = pn_bitstream_read(reader->bs, 6);
        pn_bitstream_align_32(reader->bs);
      }

      /* TODO(binji): optimize? this is aligned, so it should be easy to extract
       * all the data in a buffer.*/
      *out_value = pn_bitstream_read(reader->bs, 8);
      if (reader->value_index++ == reader->num_values) {
        reader->op_index++;
      }
      return PN_TRUE;

    default:
      PN_FATAL("bad encoding: %d\n", op->encoding);
  }
}

static PNBool pn_record_read_code(PNRecordReader* reader, uint32_t* out_code) {
  if (reader->entry == PN_ENTRY_UNABBREV_RECORD) {
    *out_code = pn_bitstream_read_vbr(reader->bs, 6);
    reader->num_values = pn_bitstream_read_vbr(reader->bs, 6);
    return PN_TRUE;
  } else {
    return pn_record_read_abbrev(reader, out_code);
  }
}

static PNBool pn_record_try_read_uint32(PNRecordReader* reader,
                                        uint32_t* out_value) {
  if (reader->entry == PN_ENTRY_UNABBREV_RECORD) {
    /* num_values must be set, see if we're over the limit */
    if (reader->value_index >= reader->num_values) {
      return PN_FALSE;
    }
    *out_value = pn_bitstream_read_vbr(reader->bs, 6);
    reader->value_index++;
    return PN_TRUE;
  } else {
    /* Reading an abbreviation */
    return pn_record_read_abbrev(reader, out_value);
  }
}

static PNBool pn_record_try_read_uint64(PNRecordReader* reader,
                                        uint64_t* out_value) {
  if (reader->entry == PN_ENTRY_UNABBREV_RECORD) {
    /* num_values must be set, see if we're over the limit */
    if (reader->value_index >= reader->num_values) {
      return PN_FALSE;
    }
    *out_value = pn_bitstream_read_vbr_uint64(reader->bs, 6);
    reader->value_index++;
    return PN_TRUE;
  } else {
    /* Reading an abbreviation */
    return pn_record_read_abbrev_uint64(reader, out_value);
  }
}

static PNBool pn_record_try_read_uint16(PNRecordReader* reader,
                                        uint16_t* out_value) {
  uint32_t value;
  PNBool ret = pn_record_try_read_uint32(reader, &value);
  if (ret) {
    if (value > UINT16_MAX) {
      PN_FATAL("value too large for u16; (%u)\n", value);
    }

    *out_value = value;
  }
  return ret;
}

static PNBool pn_record_try_read_int32(PNRecordReader* reader,
                                       int32_t* out_value) {
  uint32_t value;
  PNBool ret = pn_record_try_read_uint32(reader, &value);
  if (ret) {
    *out_value = value;
  }
  return ret;
}

static int32_t pn_record_read_int32(PNRecordReader* reader, const char* name) {
  int32_t value;
  if (!pn_record_try_read_int32(reader, &value)) {
    PN_FATAL("unable to read %s.\n", name);
  }

  return value;
}

static int32_t pn_record_read_decoded_int32(PNRecordReader* reader,
                                            const char* name) {
  /* We must decode int32 via int64 types because INT32_MIN is
   * encoded as a 64-bit value (0x100000001) */
  uint64_t value;
  if (!pn_record_try_read_uint64(reader, &value)) {
    PN_FATAL("unable to read %s.\n", name);
  }

  int64_t ret = pn_decode_sign_rotated_value_int64(value);
  if (ret < INT32_MIN || ret > INT32_MAX) {
    PN_FATAL("value %" PRId64 " out of int32 range.\n", ret);
  }

  return ret;
}

static uint32_t pn_record_read_uint32(PNRecordReader* reader,
                                      const char* name) {
  uint32_t value;
  if (!pn_record_try_read_uint32(reader, &value)) {
    PN_FATAL("unable to read %s.\n", name);
  }

  return value;
}

static uint64_t pn_record_read_uint64(PNRecordReader* reader,
                                      const char* name) {
  uint64_t value;
  if (!pn_record_try_read_uint64(reader, &value)) {
    PN_FATAL("unable to read %s.\n", name);
  }

  return value;
}

static int64_t pn_record_read_decoded_int64(PNRecordReader* reader,
                                            const char* name) {
  uint64_t value;
  if (!pn_record_try_read_uint64(reader, &value)) {
    PN_FATAL("unable to read %s.\n", name);
  }

  return pn_decode_sign_rotated_value_int64(value);
}

static float pn_record_read_float(PNRecordReader* reader, const char* name) {
  uint32_t value;
  if (!pn_record_try_read_uint32(reader, &value)) {
    PN_FATAL("unable to read %s.\n", name);
  }

  float float_value;
  memcpy(&float_value, &value, sizeof(float));

  return float_value;
}

static double pn_record_read_double(PNRecordReader* reader, const char* name) {
  uint64_t value;
  if (!pn_record_try_read_uint64(reader, &value)) {
    PN_FATAL("unable to read %s.\n", name);
  }

  double double_value;
  memcpy(&double_value, &value, sizeof(double));

  return double_value;
}

static void pn_record_reader_finish(PNRecordReader* reader) {
  int count = 0;
  uint32_t dummy;
  while (pn_record_try_read_uint32(reader, &dummy)) {
    ++count;
  }
  if (count) {
    PN_WARN("pn_record_reader_finish skipped %d values.\n", count);
  }
}

static void pn_block_info_context_get_abbrev(PNBlockInfoContext* context,
                                             PNBlockId block_id,
                                             PNBlockAbbrevs* abbrevs) {
  PN_CHECK(block_id < PN_MAX_BLOCK_IDS);
  PNBlockAbbrevs* context_abbrevs = &context->block_abbrev_map[block_id];
  assert(abbrevs->num_abbrevs + context_abbrevs->num_abbrevs <=
         PN_MAX_BLOCK_ABBREV);
  PNBlockAbbrev* src_abbrev = &context_abbrevs->abbrevs[0];
  PNBlockAbbrev* dest_abbrev = &abbrevs->abbrevs[abbrevs->num_abbrevs];
  uint32_t i;
  for (i = 0; i < context_abbrevs->num_abbrevs; ++i) {
    *dest_abbrev++ = *src_abbrev++;
    abbrevs->num_abbrevs++;
  }
}

static uint32_t pn_block_info_context_append_abbrev(PNBlockInfoContext* context,
                                                    PNBlockId block_id,
                                                    PNBlockAbbrev* abbrev) {
  PN_CHECK(block_id < PN_MAX_BLOCK_IDS);
  PNBlockAbbrevs* abbrevs = &context->block_abbrev_map[block_id];
  assert(abbrevs->num_abbrevs < PN_MAX_BLOCK_ABBREV);
  uint32_t abbrev_id = abbrevs->num_abbrevs++;
  PNBlockAbbrev* dest_abbrev = &abbrevs->abbrevs[abbrev_id];
  *dest_abbrev = *abbrev;
  return abbrev_id;
}

static PNBlockAbbrev* pn_block_abbrev_read(PNBitStream* bs,
                                           PNBlockAbbrevs* abbrevs) {
  assert(abbrevs->num_abbrevs < PN_MAX_BLOCK_ABBREV);
  PNBlockAbbrev* abbrev = &abbrevs->abbrevs[abbrevs->num_abbrevs++];

  uint32_t num_ops = pn_bitstream_read_vbr(bs, 5);
  PN_CHECK(num_ops < PN_MAX_BLOCK_ABBREV_OP);
  while (abbrev->num_ops < num_ops) {
    PNBlockAbbrevOp* op = &abbrev->ops[abbrev->num_ops++];

    PNBool is_literal = pn_bitstream_read(bs, 1);
    if (is_literal) {
      op->encoding = PN_ENCODING_LITERAL;
      op->value = pn_bitstream_read_vbr(bs, 8);
    } else {
      op->encoding = pn_bitstream_read(bs, 3);
      switch (op->encoding) {
        case PN_ENCODING_FIXED:
          op->num_bits = pn_bitstream_read_vbr(bs, 5);
          break;

        case PN_ENCODING_VBR:
          op->num_bits = pn_bitstream_read_vbr(bs, 5);
          break;

        case PN_ENCODING_ARRAY: {
          PNBlockAbbrevOp* elt_op = &abbrev->ops[abbrev->num_ops++];

          PNBool is_literal = pn_bitstream_read(bs, 1);
          if (is_literal) {
            elt_op->encoding = PN_ENCODING_LITERAL;
            elt_op->value = pn_bitstream_read_vbr(bs, 8);
          } else {
            elt_op->encoding = pn_bitstream_read(bs, 3);
            switch (elt_op->encoding) {
              case PN_ENCODING_FIXED:
                elt_op->num_bits = pn_bitstream_read_vbr(bs, 5);
                break;

              case PN_ENCODING_VBR:
                elt_op->num_bits = pn_bitstream_read_vbr(bs, 5);
                break;

              case PN_ENCODING_CHAR6:
                break;

              default:
                PN_FATAL("bad encoding for array element: %d\n",
                         elt_op->encoding);
            }
          }
          break;
        }

        case PN_ENCODING_CHAR6:
        case PN_ENCODING_BLOB:
          /* Nothing */
          break;

        default:
          PN_FATAL("bad encoding: %d\n", op->encoding);
      }
    }
  }

  return abbrev;
}

#if PN_TRACING
static void pn_abbrev_trace(PNBlockAbbrev* abbrev,
                            uint32_t abbrev_id,
                            PNBool global) {
  if (!PN_IS_TRACE(ABBREV)) {
    return;
  }
  PN_TRACE_PRINT_INDENT();
  PN_PRINT("%ca%d = abbrev <", global ? '@' : '%', abbrev_id);
  uint32_t i;
  for (i = 0; i < abbrev->num_ops; ++i) {
    PNBlockAbbrevOp* op = &abbrev->ops[i];
    switch (op->encoding) {
      case PN_ENCODING_LITERAL: PN_PRINT("%d", op->value); break;
      case PN_ENCODING_FIXED: PN_PRINT("fixed(%d)", op->num_bits); break;
      case PN_ENCODING_VBR: PN_PRINT("vbr(%d)", op->num_bits); break;
      case PN_ENCODING_ARRAY: {
        PNBlockAbbrevOp* elt = &abbrev->ops[i + 1];
        PN_PRINT("array(");
        switch (elt->encoding) {
          case PN_ENCODING_FIXED: PN_PRINT("fixed(%d)", elt->num_bits); break;
          case PN_ENCODING_VBR: PN_PRINT("vbr(%d)", elt->num_bits); break;
          case PN_ENCODING_CHAR6: PN_PRINT("char6"); break;
          default: PN_UNREACHABLE(); break;
        }
        PN_PRINT(")");
        ++i;
        break;
      }

      case PN_ENCODING_CHAR6: PN_PRINT("char6"); break;
      case PN_ENCODING_BLOB: PN_PRINT("blob"); break;
      default: PN_UNREACHABLE(); break;
    }
    if (i != abbrev->num_ops - 1) {
      PN_PRINT(", ");
    }
  }
  PN_PRINT(">;\n");
}
#else
static void pn_abbrev_trace(PNBlockAbbrev* abbrev,
                            uint32_t abbrev_id,
                            PNBool global) {}
#endif /* PN_TRACING */

static void pn_blockinfo_block_read(PNBlockInfoContext* context,
                                    PNBitStream* bs) {
  PN_BEGIN_TIME(BLOCKINFO_BLOCK_READ);
  PN_TRACE(BLOCKINFO_BLOCK, "abbreviations {  // BlockID = %d\n",
           PN_BLOCKID_BLOCKINFO);
  PN_TRACE_INDENT(BLOCKINFO_BLOCK, 2);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  (void)pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  PNBlockId block_id = -1;

  /* Indent 2 more, that we we can always dedent 2 on SETBID */
  PN_TRACE_INDENT(BLOCKINFO_BLOCK, 2);

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_TRACE_DEDENT(BLOCKINFO_BLOCK, 4);
        PN_TRACE(BLOCKINFO_BLOCK, "}\n");
        pn_bitstream_align_32(bs);
        PN_END_TIME(BLOCKINFO_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in blockinfo_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        PNBlockAbbrev* abbrev = pn_block_abbrev_read(bs, &abbrevs);
        uint32_t abbrev_id =
            pn_block_info_context_append_abbrev(context, block_id, abbrev);
        pn_abbrev_trace(abbrev, abbrev_id, PN_TRUE);
        break;
      }

      default: {
        /* Abbrev or UNABBREV_RECORD */
        uint32_t code;
        PNRecordReader reader;
        pn_record_reader_init(&reader, bs, &abbrevs, entry);
        pn_record_read_code(&reader, &code);

        switch (code) {
          case PN_BLOCKINFO_CODE_SETBID:
            PN_TRACE_DEDENT(BLOCKINFO_BLOCK, 2);
            block_id = pn_record_read_int32(&reader, "block id");
            const char* name = NULL;
            switch (block_id) {
              case PN_BLOCKID_BLOCKINFO: name = "abbreviations"; break;
              case PN_BLOCKID_MODULE: name = "module"; break;
              case PN_BLOCKID_CONSTANTS: name = "constants"; break;
              case PN_BLOCKID_FUNCTION: name = "function"; break;
              case PN_BLOCKID_VALUE_SYMTAB: name = "valuesymtab"; break;
              case PN_BLOCKID_TYPE: name = "type"; break;
              case PN_BLOCKID_GLOBALVAR: name = "globals"; break;
              default: PN_UNREACHABLE(); break;
            }
            PN_TRACE(BLOCKINFO_BLOCK, "%s:\n", name);
            PN_TRACE_INDENT(BLOCKINFO_BLOCK, 2);
            break;

          case PN_BLOCKINFO_CODE_BLOCKNAME:
            PN_TRACE(BLOCKINFO_BLOCK, "block name\n");
            break;

          case PN_BLOCKINFO_CODE_SETRECORDNAME:
            PN_TRACE(BLOCKINFO_BLOCK, "block record name\n");
            break;

          default:
            PN_FATAL("bad record code: %d.\n", code);
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
  PN_FATAL("Unexpected end of stream.\n");
}

static void pn_type_block_read(PNModule* module,
                               PNBlockInfoContext* context,
                               PNBitStream* bs) {
  PN_BEGIN_TIME(TYPE_BLOCK_READ);
  PN_TRACE(TYPE_BLOCK, "types {  // BlockID = %d\n", PN_BLOCKID_TYPE);
  PN_TRACE_INDENT(TYPE_BLOCK, 2);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num_words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_TYPE, &abbrevs);

  PNTypeId current_type_id = 0;

  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_CHECK(current_type_id == module->num_types);
        PN_TRACE_DEDENT(TYPE_BLOCK, 2);
        PN_TRACE(TYPE_BLOCK, "}\n");
        pn_bitstream_align_32(bs);
        pn_allocator_reset_to_mark(&module->temp_allocator, mark);
        PN_END_TIME(TYPE_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in type_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        PNBlockAbbrev* abbrev = pn_block_abbrev_read(bs, &abbrevs);
        uint32_t abbrev_id = abbrev - abbrevs.abbrevs;
        pn_abbrev_trace(abbrev, abbrev_id, PN_FALSE);
        break;
      }

      default: {
        /* Abbrev or UNABBREV_RECORD */
        uint32_t code;
        PNRecordReader reader;
        pn_record_reader_init(&reader, bs, &abbrevs, entry);
        pn_record_read_code(&reader, &code);

        switch (code) {
          case PN_TYPE_CODE_NUMENTRY: {
            module->num_types = pn_record_read_uint32(&reader, "num types");
            module->types = pn_allocator_allocz(
                &module->allocator, module->num_types * sizeof(PNType),
                PN_DEFAULT_ALIGN);
            PN_TRACE(TYPE_BLOCK, "count %d;\n", module->num_types);
            break;
          }

          case PN_TYPE_CODE_VOID: {
            PN_CHECK(current_type_id < module->num_types);
            PNTypeId type_id = current_type_id++;
            PNType* type = &module->types[type_id];
            type->code = PN_TYPE_CODE_VOID;
            type->basic_type = PN_BASIC_TYPE_VOID;
            break;
          }

          case PN_TYPE_CODE_FLOAT: {
            PN_CHECK(current_type_id < module->num_types);
            PNTypeId type_id = current_type_id++;
            PNType* type = &module->types[type_id];
            type->code = PN_TYPE_CODE_FLOAT;
            type->basic_type = PN_BASIC_TYPE_FLOAT;
            break;
          }

          case PN_TYPE_CODE_DOUBLE: {
            PN_CHECK(current_type_id < module->num_types);
            PNTypeId type_id = current_type_id++;
            PNType* type = &module->types[type_id];
            type->code = PN_TYPE_CODE_DOUBLE;
            type->basic_type = PN_BASIC_TYPE_DOUBLE;
            break;
          }

          case PN_TYPE_CODE_INTEGER: {
            PN_CHECK(current_type_id < module->num_types);
            PNTypeId type_id = current_type_id++;
            PNType* type = &module->types[type_id];
            type->code = PN_TYPE_CODE_INTEGER;
            type->width = pn_record_read_int32(&reader, "width");
            switch (type->width) {
              case 1:
                type->basic_type = PN_BASIC_TYPE_INT1;
                break;
              case 8:
                type->basic_type = PN_BASIC_TYPE_INT8;
                break;
              case 16:
                type->basic_type = PN_BASIC_TYPE_INT16;
                break;
              case 32:
                type->basic_type = PN_BASIC_TYPE_INT32;
                break;
              case 64:
                type->basic_type = PN_BASIC_TYPE_INT64;
                break;
              default:
                PN_FATAL("Bad integer width: %d\n", type->width);
            }
            break;
          }

          case PN_TYPE_CODE_FUNCTION: {
            PN_CHECK(current_type_id < module->num_types);
            PNTypeId type_id = current_type_id++;
            PNType* type = &module->types[type_id];
            type->code = PN_TYPE_CODE_FUNCTION;
            type->basic_type = PN_BASIC_TYPE_INT32;
            type->is_varargs = pn_record_read_int32(&reader, "is_varargs");
            type->return_type = pn_record_read_int32(&reader, "return_type");
            type->num_args = 0;
            type->arg_types = NULL;
            PNTypeId arg_type_id;
            while (pn_record_try_read_uint16(&reader, &arg_type_id)) {
              PNTypeId* new_arg_type_id = pn_allocator_realloc_add(
                  &module->allocator, (void**)&type->arg_types,
                  sizeof(PNTypeId), sizeof(PNTypeId));
              *new_arg_type_id = arg_type_id;
              type->num_args++;
            }
            break;
          }

          default:
            PN_FATAL("bad record code: %d.\n", code);
        }

        if (code != PN_TYPE_CODE_NUMENTRY) {
          PN_TRACE(TYPE_BLOCK, "@t%d = %s;\n", current_type_id - 1,
                   pn_type_describe(module, current_type_id - 1));
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
  PN_FATAL("Unexpected end of stream.\n");
}

static void pn_globalvar_write_reloc(PNModule* module,
                                     PNValueId value_id,
                                     uint32_t offset,
                                     uint32_t addend) {
  PN_CHECK(value_id < module->num_values);
  PNValue* value = pn_module_get_value(module, value_id);
  uint32_t reloc_value;
  switch (value->code) {
    case PN_VALUE_CODE_GLOBAL_VAR: {
      PNGlobalVar* var = pn_module_get_global_var(module, value->index);
      reloc_value = var->offset + addend;
      break;
    }

    case PN_VALUE_CODE_FUNCTION:
      PN_CHECK(addend == 0);
      /* Use the function index as the function "address". */
      reloc_value = pn_function_id_to_pointer(value->index);
      break;

    default:
      PN_FATAL("Unexpected globalvar reloc. code: %d\n", value->code);
      break;
  }

#if 0
  PN_TRACE(GLOBALVAR_BLOCK,
           "  writing reloc value. offset:%u value:%d (0x%x)\n", offset,
           reloc_value, reloc_value);
#endif
  pn_memory_write_u32(module->memory, offset, reloc_value);
}

static void pn_globalvar_block_read(PNModule* module,
                                    PNBlockInfoContext* context,
                                    PNBitStream* bs) {
  PN_BEGIN_TIME(GLOBALVAR_BLOCK_READ);
  PN_TRACE(GLOBALVAR_BLOCK, "globals {  // BlockID = %d\n",
           PN_BLOCKID_GLOBALVAR);
  PN_TRACE_INDENT(GLOBALVAR_BLOCK, 2);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_GLOBALVAR, &abbrevs);

  PNGlobalVar* global_var = NULL;

  uint32_t num_global_vars = 0;
  uint32_t initializer_id = 0;

  PNMemory* memory = module->memory;
  uint8_t* data8 = memory->data;
  uint32_t data_offset = PN_MEMORY_GUARD_SIZE;

  memory->globalvar_start = data_offset;

  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);

  typedef struct PNRelocInfo {
    uint32_t offset;
    uint32_t index;
    uint32_t addend;
  } PNRelocInfo;
  PNRelocInfo* reloc_infos = NULL;
  uint32_t num_reloc_infos = 0;

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK: {
        if (global_var) {
          /* Dedent if there was a previous variable */
          PN_TRACE_DEDENT(GLOBALVAR_BLOCK, 2);
          /* Additional dedent if there was a previous compound initializer */
          if (global_var->num_initializers > 1) {
            PN_TRACE(GLOBALVAR_BLOCK, "}\n");
            PN_TRACE_DEDENT(GLOBALVAR_BLOCK, 2);
          }
        }

        PN_TRACE_DEDENT(GLOBALVAR_BLOCK, 2);
        PN_TRACE(GLOBALVAR_BLOCK, "}\n");
        pn_bitstream_align_32(bs);

        uint32_t i;
        for (i = 0; i < num_reloc_infos; ++i) {
          pn_globalvar_write_reloc(module, reloc_infos[i].index,
                                   reloc_infos[i].offset,
                                   reloc_infos[i].addend);
        }

        memory->globalvar_end = data_offset;

        pn_allocator_reset_to_mark(&module->temp_allocator, mark);
        PN_END_TIME(GLOBALVAR_BLOCK_READ);
        return;
      }

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in globalvar_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        PNBlockAbbrev* abbrev = pn_block_abbrev_read(bs, &abbrevs);
        uint32_t abbrev_id = abbrev - abbrevs.abbrevs;
        pn_abbrev_trace(abbrev, abbrev_id, PN_FALSE);
        break;
      }

      default: {
        /* Abbrev or UNABBREV_RECORD */
        uint32_t code;
        PNRecordReader reader;
        pn_record_reader_init(&reader, bs, &abbrevs, entry);
        pn_record_read_code(&reader, &code);

        switch (code) {
          case PN_GLOBALVAR_CODE_VAR: {
            PNGlobalVarId global_var_id = module->num_global_vars++;
            PN_CHECK(global_var_id < num_global_vars);

            if (global_var) {
              /* Dedent if there was a previous variable */
              PN_TRACE_DEDENT(GLOBALVAR_BLOCK, 2);
              /* Additional dedent if there was a previous compound initializer
               */
              if (global_var->num_initializers > 1) {
                PN_TRACE(GLOBALVAR_BLOCK, "}\n");
                PN_TRACE_DEDENT(GLOBALVAR_BLOCK, 2);
              }
            }

            global_var = &module->global_vars[global_var_id];
            global_var->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;
            global_var->is_constant =
                pn_record_read_int32(&reader, "is_constant") != 0;
            global_var->num_initializers = 1;

            PN_CHECK(pn_is_power_of_two(global_var->alignment));
            data_offset = pn_align_up(data_offset, global_var->alignment);
            global_var->offset = data_offset;
            initializer_id = 0;

            PNValueId value_id;
            PNValue* value = pn_module_append_value(module, &value_id);
            value->code = PN_VALUE_CODE_GLOBAL_VAR;
            value->type_id = pn_module_find_pointer_type(module);
            value->index = global_var_id;

            PN_TRACE(GLOBALVAR_BLOCK, "%s %s, align %d,\n",
                     global_var->is_constant ? "const" : "var",
                     pn_value_describe_temp(module, NULL, value_id),
                     global_var->alignment);
            PN_TRACE_INDENT(GLOBALVAR_BLOCK, 2);
            break;
          }

          case PN_GLOBALVAR_CODE_COMPOUND: {
            PN_CHECK(global_var);
            global_var->num_initializers =
                pn_record_read_int32(&reader, "num_initializers");
            PN_TRACE(GLOBALVAR_BLOCK, "initializers %d {\n",
                     global_var->num_initializers);
            PN_TRACE_INDENT(GLOBALVAR_BLOCK, 2);
            break;
          }

          case PN_GLOBALVAR_CODE_ZEROFILL: {
            PN_CHECK(global_var);
            PN_CHECK(initializer_id < global_var->num_initializers);
            initializer_id++;
            uint32_t num_bytes = pn_record_read_uint32(&reader, "num_bytes");

            pn_memory_zerofill(memory, data_offset, num_bytes);
            data_offset += num_bytes;

            PN_TRACE(GLOBALVAR_BLOCK, "zerofill %d;\n", num_bytes);
            break;
          }

          case PN_GLOBALVAR_CODE_DATA: {
            PN_CHECK(global_var);
            PN_CHECK(initializer_id < global_var->num_initializers);
            initializer_id++;
            uint32_t num_bytes = 0;
            uint32_t value;
            PN_TRACE(GLOBALVAR_BLOCK, "{");

            /* TODO(binji): optimize. Check if this data is aligned with type
             * abbreviation type PN_ENCODING_BLOB. If so, we can just memcpy. */
            while (pn_record_try_read_uint32(&reader, &value)) {
              if (value >= 256) {
                PN_FATAL("globalvar data out of range: %d\n", value);
              }

              if (data_offset + 1 > memory->size) {
                PN_FATAL("memory-size is too small (%u < %u).\n", memory->size,
                         data_offset + 1);
              }
              if (PN_IS_TRACE(GLOBALVAR_BLOCK)) {
                if (num_bytes) {
                  PN_PRINT(", ");
                  if (num_bytes % 14 == 0) {
                    PN_PRINT("\n");
                    PN_TRACE_PRINT_INDENT();
                    PN_PRINT(" ");
                  }
                }
                PN_PRINT("%3d", value);
              }

              data8[data_offset++] = value;
              num_bytes++;
            }

            if (PN_IS_TRACE(GLOBALVAR_BLOCK)) {
              PN_PRINT("}\n");
            }
            break;
          }

          case PN_GLOBALVAR_CODE_RELOC: {
            PN_CHECK(global_var);
            PN_CHECK(initializer_id < global_var->num_initializers);
            initializer_id++;
            uint32_t index = pn_record_read_uint32(&reader, "reloc index");
            uint32_t addend = 0;
            /* Optional */
            pn_record_try_read_uint32(&reader, &addend);

            if (addend) {
              PN_TRACE(GLOBALVAR_BLOCK, "reloc %s + %d;\n",
                       pn_value_describe_temp(module, NULL, index), addend);
            } else {
              PN_TRACE(GLOBALVAR_BLOCK, "reloc %s;\n",
                       pn_value_describe_temp(module, NULL, index));
            }

            if (index < module->num_values) {
              pn_globalvar_write_reloc(module, index, data_offset, addend);
            } else {
              /* Unknown value, this will need to be fixed up later */
              pn_allocator_realloc_add(&module->temp_allocator,
                                       (void**)&reloc_infos,
                                       sizeof(PNRelocInfo), PN_DEFAULT_ALIGN);
              PNRelocInfo* reloc_info = &reloc_infos[num_reloc_infos++];
              reloc_info->offset = data_offset;
              reloc_info->index = index;
              reloc_info->addend = addend;
            }

            data_offset += 4;
            break;
          }

          case PN_GLOBALVAR_CODE_COUNT: {
            num_global_vars =
                pn_record_read_uint32(&reader, "global var count");
            module->global_vars = pn_allocator_alloc(
                &module->allocator, num_global_vars * sizeof(PNGlobalVar),
                PN_DEFAULT_ALIGN);

            PN_TRACE(GLOBALVAR_BLOCK, "count %d;\n", num_global_vars);
            break;
          }

          default:
            PN_FATAL("bad record code: %d.\n", code);
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
  PN_FATAL("Unexpected end of stream.\n");
}

static void pn_value_symtab_block_read(PNModule* module,
                                       PNBlockInfoContext* context,
                                       PNBitStream* bs) {
  PN_BEGIN_TIME(VALUE_SYMTAB_BLOCK_READ);
  PN_TRACE(VALUE_SYMTAB_BLOCK, "valuesymtab {  // BlockID = %d\n",
           PN_BLOCKID_VALUE_SYMTAB);
  PN_TRACE_INDENT(VALUE_SYMTAB_BLOCK, 2);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_VALUE_SYMTAB, &abbrevs);

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_TRACE_DEDENT(VALUE_SYMTAB_BLOCK, 2);
        PN_TRACE(VALUE_SYMTAB_BLOCK, "}\n");
        pn_bitstream_align_32(bs);
        PN_END_TIME(VALUE_SYMTAB_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in valuesymtab_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        PNBlockAbbrev* abbrev = pn_block_abbrev_read(bs, &abbrevs);
        uint32_t abbrev_id = abbrev - abbrevs.abbrevs;
        pn_abbrev_trace(abbrev, abbrev_id, PN_FALSE);
        break;
      }

      default: {
        /* Abbrev or UNABBREV_RECORD */
        uint32_t code;
        PNRecordReader reader;
        pn_record_reader_init(&reader, bs, &abbrevs, entry);
        pn_record_read_code(&reader, &code);

        switch (code) {
          case PN_VALUESYMBTAB_CODE_ENTRY: {
            PNValueId value_id = pn_record_read_int32(&reader, "value_id");
            char* name = NULL;
            char* p;
            int32_t c;

            while (pn_record_try_read_int32(&reader, &c)) {
              p = pn_allocator_realloc_add(&module->allocator, (void**)&name, 1,
                                           1);
              *p = c;
            }

            /* NULL-terminate the string if any characters were read. */
            if (name) {
              p = pn_allocator_realloc_add(&module->allocator, (void**)&name, 1,
                                           1);
              *p = 0;
            }

            PN_TRACE(VALUE_SYMTAB_BLOCK, "%s : \"%s\";\n",
                     pn_value_describe_temp(module, NULL, value_id), name);

            PNValue* value = pn_module_get_value(module, value_id);
            if (value->code == PN_VALUE_CODE_FUNCTION) {
              PNFunctionId function_id = value->index;
              PNFunction* function =
                  pn_module_get_function(module, function_id);
              function->name = name;

#define PN_INTRINSIC_CHECK(i_enum, i_name)                        \
  if (strcmp(name, i_name) == 0) {                                \
    module->known_functions[PN_INTRINSIC_##i_enum] = function_id; \
    function->intrinsic_id = PN_INTRINSIC_##i_enum;               \
    PN_TRACE(INTRINSICS, "intrinsic \"%s\" (%d)\n", i_name,       \
             PN_INTRINSIC_##i_enum);                              \
  } else

              PN_FOREACH_INTRINSIC(PN_INTRINSIC_CHECK)
              { /* Unknown function name */ }

#undef PN_INTRINSIC_CHECK
            }

            break;
          }

          case PN_VALUESYMBTAB_CODE_BBENTRY: break;

          default:
            PN_FATAL("bad record code: %d.\n", code);
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
  PN_FATAL("Unexpected end of stream.\n");
}

static void pn_constants_block_read(PNModule* module,
                                    PNFunction* function,
                                    PNBlockInfoContext* context,
                                    PNBitStream* bs) {
  PN_BEGIN_TIME(CONSTANTS_BLOCK_READ);
  PN_TRACE(CONSTANTS_BLOCK, "constants {  // BlockID = %d\n",
           PN_BLOCKID_CONSTANTS);
  PN_TRACE_INDENT(CONSTANTS_BLOCK, 2);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_CONSTANTS, &abbrevs);

  PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);

  /* Indent 2 more, that we we can always dedent 2 on PN_CONSTANTS_CODE_SETTYPE
   */
  PN_TRACE_INDENT(CONSTANTS_BLOCK, 2);

  PNTypeId cur_type_id = -1;
  PNBasicType cur_basic_type = PN_BASIC_TYPE_VOID;
  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_TRACE_DEDENT(CONSTANTS_BLOCK, 2);
        PN_TRACE(CONSTANTS_BLOCK, "}\n");
        PN_TRACE_DEDENT(CONSTANTS_BLOCK, 2);
        pn_bitstream_align_32(bs);
        pn_allocator_reset_to_mark(&module->temp_allocator, mark);
        PN_END_TIME(CONSTANTS_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in constants_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        PNBlockAbbrev* abbrev = pn_block_abbrev_read(bs, &abbrevs);
        uint32_t abbrev_id = abbrev - abbrevs.abbrevs;
        pn_abbrev_trace(abbrev, abbrev_id, PN_FALSE);
        break;
      }

      default: {
        /* Abbrev or UNABBREV_RECORD */
        uint32_t code;
        PNRecordReader reader;
        pn_record_reader_init(&reader, bs, &abbrevs, entry);
        pn_record_read_code(&reader, &code);

        switch (code) {
          case PN_CONSTANTS_CODE_SETTYPE:
            cur_type_id = pn_record_read_int32(&reader, "current type");
            cur_basic_type =
                pn_module_get_type(module, cur_type_id)->basic_type;
            PN_TRACE_DEDENT(CONSTANTS_BLOCK, 2);
            PN_TRACE(CONSTANTS_BLOCK, "%s:\n",
                     pn_type_describe(module, cur_type_id));
            PN_TRACE_INDENT(CONSTANTS_BLOCK, 2);
            break;

          case PN_CONSTANTS_CODE_UNDEF: {
            PNConstantId constant_id;
            PNConstant* constant =
                pn_function_append_constant(module, function, &constant_id);
            constant->code = code;
            constant->type_id = cur_type_id;
            constant->basic_type = cur_basic_type;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_CONSTANT;
            value->type_id = cur_type_id;
            value->index = constant_id;

            PN_TRACE(CONSTANTS_BLOCK, "%s = %s undef;\n",
                     pn_value_describe_temp(module, function, value_id),
                     pn_type_describe(module, cur_type_id));
            break;
          }

          case PN_CONSTANTS_CODE_INTEGER: {
            PNConstantId constant_id;
            PNConstant* constant =
                pn_function_append_constant(module, function, &constant_id);
            constant->code = code;
            constant->type_id = cur_type_id;
            constant->basic_type = cur_basic_type;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_CONSTANT;
            value->type_id = cur_type_id;
            value->index = constant_id;

            switch (cur_basic_type) {
              case PN_BASIC_TYPE_INT1:
              case PN_BASIC_TYPE_INT8:
              case PN_BASIC_TYPE_INT16:
              case PN_BASIC_TYPE_INT32: {
                int32_t data =
                    pn_record_read_decoded_int32(&reader, "integer value");

                switch (cur_basic_type) {
                  case PN_BASIC_TYPE_INT1:
                    constant->value.i8 = data & 1;
                    break;
                  case PN_BASIC_TYPE_INT8:
                    constant->value.i8 = data;
                    break;
                  case PN_BASIC_TYPE_INT16:
                    constant->value.i16 = data;
                    break;
                  case PN_BASIC_TYPE_INT32:
                    constant->value.i32 = data;
                    break;
                  default:
                    PN_UNREACHABLE();
                    break;
                }

                PN_TRACE(CONSTANTS_BLOCK, "%s = %s %d;\n",
                         pn_value_describe_temp(module, function, value_id),
                         pn_type_describe(module, cur_type_id), data);
                break;
              }

              case PN_BASIC_TYPE_INT64: {
                int64_t data =
                    pn_record_read_decoded_int64(&reader, "integer64 value");
                constant->value.i64 = data;

                PN_TRACE(CONSTANTS_BLOCK, "%s = %s %" PRId64 ";\n",
                         pn_value_describe_temp(module, function, value_id),
                         pn_type_describe(module, cur_type_id), data);
                break;
              }

              default:
                PN_UNREACHABLE();
                break;
            }

            break;
          }

          case PN_CONSTANTS_CODE_FLOAT: {
            PNConstantId constant_id;
            PNConstant* constant =
                pn_function_append_constant(module, function, &constant_id);
            constant->code = code;
            constant->type_id = cur_type_id;
            constant->basic_type = cur_basic_type;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_CONSTANT;
            value->type_id = cur_type_id;
            value->index = constant_id;

            switch (cur_basic_type) {
              case PN_BASIC_TYPE_FLOAT: {
                float data = pn_record_read_float(&reader, "float value");
                constant->value.f32 = data;
                PN_TRACE(CONSTANTS_BLOCK, "%s = %s %g;\n",
                         pn_value_describe_temp(module, function, value_id),
                         pn_type_describe(module, cur_type_id), data);
                break;
              }

              case PN_BASIC_TYPE_DOUBLE: {
                double data = pn_record_read_double(&reader, "double value");
                constant->value.f64 = data;
                PN_TRACE(CONSTANTS_BLOCK, "%s = %s %g;\n",
                         pn_value_describe_temp(module, function, value_id),
                         pn_type_describe(module, cur_type_id), data);
                break;
              }

              default:
                PN_UNREACHABLE();
                break;
            }

            break;
          }

          default:
            PN_FATAL("bad record code: %d.\n", code);
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
  PN_FATAL("Unexpected end of stream.\n");
}

static void pn_function_block_read(PNModule* module,
                                   PNBlockInfoContext* context,
                                   PNBitStream* bs,
                                   PNFunctionId function_id) {
  PN_BEGIN_TIME(FUNCTION_BLOCK_READ);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_FUNCTION, &abbrevs);

  PNFunction* function = pn_module_get_function(module, function_id);
  if (PN_IS_TRACE(FUNCTION_BLOCK)) {
    pn_function_print_header(module, function, function_id);
  }

  PNType* type = pn_module_get_type(module, function->type_id);
  assert(type->code == PN_TYPE_CODE_FUNCTION);
  assert(type->num_args == function->num_args);

  uint32_t i;
  for (i = 0; i < function->num_args; ++i) {
    PNValueId value_id;
    PNValue* value = pn_function_append_value(module, function, &value_id);
    value->code = PN_VALUE_CODE_FUNCTION_ARG;
    value->type_id = type->arg_types[i];
    value->index = i;
  }

  uint32_t num_bbs = 0;
  PNValueId first_bb_value_id = PN_INVALID_VALUE_ID;
  /* These are initialized with different values so the first instruction
   * creates the basic block */
  PNBasicBlockId prev_bb_id = -1;
  PNBasicBlockId cur_bb_id = 0;
  PNBasicBlock* cur_bb = NULL;
  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_CHECK(num_bbs == function->num_bbs);
        pn_function_calculate_result_value_types(module, function);
        pn_function_calculate_opcodes(module, function);
        pn_function_calculate_uses(module, function);
        pn_function_calculate_pred_bbs(module, function);
        pn_function_calculate_phi_assigns(module, function);
#if PN_CALCULATE_LIVENESS
        pn_function_calculate_liveness(module, function);
#endif
        pn_function_trace(module, function, function_id);

        PN_TRACE_DEDENT(FUNCTION_BLOCK, 2);
        PN_TRACE(FUNCTION_BLOCK, "}\n");
        pn_bitstream_align_32(bs);
        PN_END_TIME(FUNCTION_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK: {
        uint32_t id = pn_bitstream_read_vbr(bs, 8);
        switch (id) {
          case PN_BLOCKID_CONSTANTS:
            pn_constants_block_read(module, function, context, bs);
            break;

          case PN_BLOCKID_VALUE_SYMTAB:
            pn_value_symtab_block_read(module, context, bs);
            break;

          default:
            PN_FATAL("bad block id %d\n", id);
        }
        break;
      }

      case PN_ENTRY_DEFINE_ABBREV: {
        PNBlockAbbrev* abbrev = pn_block_abbrev_read(bs, &abbrevs);
        uint32_t abbrev_id = abbrev - abbrevs.abbrevs;
        pn_abbrev_trace(abbrev, abbrev_id, PN_FALSE);
        break;
      }

      default: {
        /* Abbrev or UNABBREV_RECORD */
        uint32_t code;
        PNRecordReader reader;
        pn_record_reader_init(&reader, bs, &abbrevs, entry);
        pn_record_read_code(&reader, &code);

        PNBool is_terminator = PN_FALSE;
        PNValueId rel_id = pn_function_num_values(module, function);

        if (code == PN_FUNCTION_CODE_DECLAREBLOCKS) {
          function->num_bbs =
              pn_record_read_uint32(&reader, "num basic blocks");
          function->bbs = pn_allocator_allocz(
              &module->allocator, sizeof(PNBasicBlock) * function->num_bbs,
              PN_DEFAULT_ALIGN);
          PN_TRACE(FUNCTION_BLOCK, "blocks %d;\n", function->num_bbs);
          break;
        }

        if (prev_bb_id != cur_bb_id) {
          PN_CHECK(cur_bb_id < function->num_bbs);
          prev_bb_id = cur_bb_id;
          cur_bb = &function->bbs[cur_bb_id];
          cur_bb->first_def_id = PN_INVALID_VALUE_ID;
          cur_bb->last_def_id = PN_INVALID_VALUE_ID;

          first_bb_value_id = pn_function_num_values(module, function);
          num_bbs++;
        }

        switch (code) {
          case PN_FUNCTION_CODE_DECLAREBLOCKS:
            /* Handled above so we only print the basic block index when listing
             * instructions */
            break;

          case PN_FUNCTION_CODE_INST_BINOP: {
            PNInstructionId inst_id;
            PNInstructionBinop* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionBinop, module, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            /* Fix later, when all values are defined. */
            value->type_id = PN_INVALID_TYPE_ID;
            value->index = inst_id;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->value0_id = pn_record_read_uint32(&reader, "value 0");
            inst->value1_id = pn_record_read_uint32(&reader, "value 1");
            inst->binop_opcode = pn_record_read_int32(&reader, "opcode");
            inst->flags = 0;

            pn_context_fix_value_ids(context, rel_id, 2, &inst->value0_id,
                                     &inst->value1_id);

            /* optional */
            pn_record_try_read_int32(&reader, &inst->flags);
            break;
          }

          case PN_FUNCTION_CODE_INST_CAST: {
            PNInstructionId inst_id;
            PNInstructionCast* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionCast, module, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = inst_id;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->value_id = pn_record_read_uint32(&reader, "value");
            inst->type_id = pn_record_read_uint32(&reader, "type_id");
            inst->cast_opcode = pn_record_read_int32(&reader, "opcode");

            value->type_id = inst->type_id;

            pn_context_fix_value_ids(context, rel_id, 1, &inst->value_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_RET: {
            PNInstructionId inst_id;
            PNInstructionRet* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionRet, module, cur_bb, &inst_id);
            inst->base.code = code;
            inst->value_id = PN_INVALID_VALUE_ID;

            uint32_t value_id;
            if (pn_record_try_read_uint32(&reader, &value_id)) {
              inst->value_id = value_id;
              pn_context_fix_value_ids(context, rel_id, 1, &inst->value_id);
            }

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_BR: {
            PNInstructionId inst_id;
            PNInstructionBr* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionBr, module, cur_bb, &inst_id);
            inst->base.code = code;
            inst->true_bb_id = pn_record_read_uint32(&reader, "true_bb");
            inst->false_bb_id = PN_INVALID_BB_ID;

            pn_basic_block_list_append(module, &cur_bb->succ_bb_ids,
                                       &cur_bb->num_succ_bbs, inst->true_bb_id);

            if (pn_record_try_read_uint16(&reader, &inst->false_bb_id)) {
              inst->value_id = pn_record_read_uint32(&reader, "value");
              pn_context_fix_value_ids(context, rel_id, 1, &inst->value_id);

              pn_basic_block_list_append(module, &cur_bb->succ_bb_ids,
                                         &cur_bb->num_succ_bbs,
                                         inst->false_bb_id);
            } else {
              inst->value_id = PN_INVALID_VALUE_ID;
            }

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_SWITCH: {
            PNInstructionId inst_id;
            PNInstructionSwitch* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionSwitch, module, cur_bb, &inst_id);

            inst->type_id = pn_record_read_uint32(&reader, "type_id");
            inst->value_id = pn_record_read_uint32(&reader, "value");
            inst->default_bb_id = pn_record_read_uint32(&reader, "default bb");

            pn_basic_block_list_append(module, &cur_bb->succ_bb_ids,
                                       &cur_bb->num_succ_bbs,
                                       inst->default_bb_id);

            pn_context_fix_value_ids(context, rel_id, 1, &inst->value_id);

            inst->base.code = code;
            int32_t num_cases = pn_record_read_int32(&reader, "num cases");
            uint32_t total_values = 0;

            int32_t c = 0;
            for (c = 0; c < num_cases; ++c) {
              int32_t num_case_values = 0;

              int32_t i;
              int32_t num_values = pn_record_read_int32(&reader, "num values");
              for (i = 0; i < num_values; ++i) {
                PNBool is_single = pn_record_read_int32(&reader, "is_single");
                int64_t low = pn_record_read_decoded_int64(&reader, "low");
                int64_t high = low;
                if (!is_single) {
                  high = pn_record_read_decoded_int64(&reader, "high");
                  PN_CHECK(low <= high);
                }

                int64_t diff = high - low + 1;
                PNSwitchCase* new_switch_cases = pn_allocator_realloc_add(
                    &module->instruction_allocator, (void**)&inst->cases,
                    diff * sizeof(PNSwitchCase), PN_DEFAULT_ALIGN);

                int64_t n;
                for (n = 0; n < diff; ++n) {
                  new_switch_cases[n].value = n + low;
                }

                num_case_values += diff;
              }

              PNBasicBlockId bb_id = pn_record_read_int32(&reader, "bb");
              for (i = total_values; i < total_values + num_case_values; ++i) {
                inst->cases[i].bb_id = bb_id;
              }

              total_values += num_case_values;

              pn_basic_block_list_append(module, &cur_bb->succ_bb_ids,
                                         &cur_bb->num_succ_bbs, bb_id);
            }

            inst->num_cases = total_values;

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_UNREACHABLE: {
            PNInstructionId inst_id;
            PNInstructionUnreachable* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionUnreachable, module, cur_bb, &inst_id);
            inst->base.code = code;
            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_PHI: {
            PNInstructionId inst_id;
            PNInstructionPhi* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionPhi, module, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = inst_id;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->type_id = pn_record_read_int32(&reader, "type_id");
            inst->num_incoming = 0;

            value->type_id = inst->type_id;

            uint32_t i;
            while (1) {
              PNBasicBlockId bb;
              PNValueId value;
              if (!pn_record_try_read_uint32(&reader, &value)) {
                break;
              }
              if (context->use_relative_ids) {
                value = value_id - (int32_t)pn_decode_sign_rotated_value(value);
              }

              if (!pn_record_try_read_uint16(&reader, &bb)) {
                PN_FATAL("unable to read phi bb index\n");
              }

              /* Dedupe incoming branches */
              PNBool found = PN_FALSE;
              for (i = 0; i < inst->num_incoming; ++i) {
                if (inst->incoming[i].bb_id == bb) {
                  if (inst->incoming[i].value_id == value) {
                    found = PN_TRUE;
                  } else {
                    /* Found, but values don't match */
                    PN_FATAL(
                        "phi duplicated with matching bb %d but different "
                        "values %d != %d\n",
                        bb, inst->incoming[i].value_id, value);
                  }
                  break;
                }
              }

              if (!found) {
                pn_allocator_realloc_add(
                    &module->instruction_allocator, (void**)&inst->incoming,
                    sizeof(PNPhiIncoming), PN_DEFAULT_ALIGN);
                PNPhiIncoming* incoming = &inst->incoming[inst->num_incoming++];
                incoming->bb_id = bb;
                incoming->value_id = value;
              }
            }
            break;
          }

          case PN_FUNCTION_CODE_INST_ALLOCA: {
            PNInstructionId inst_id;
            PNInstructionAlloca* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionAlloca, module, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->type_id = pn_module_find_pointer_type(module);
            value->index = inst_id;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->size_id = pn_record_read_uint32(&reader, "size");
            inst->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;
            PN_CHECK(pn_is_power_of_two(inst->alignment));

            pn_context_fix_value_ids(context, rel_id, 1, &inst->size_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_LOAD: {
            PNInstructionId inst_id;
            PNInstructionLoad* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionLoad, module, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = inst_id;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->src_id = pn_record_read_uint32(&reader, "src");
            inst->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;
            inst->type_id = pn_record_read_int32(&reader, "type_id");
            PN_CHECK(pn_is_power_of_two(inst->alignment));

            value->type_id = inst->type_id;

            pn_context_fix_value_ids(context, rel_id, 1, &inst->src_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_STORE: {
            PNInstructionId inst_id;
            PNInstructionStore* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionStore, module, cur_bb, &inst_id);

            inst->base.code = code;
            inst->dest_id = pn_record_read_uint32(&reader, "dest");
            inst->value_id = pn_record_read_uint32(&reader, "value");
            inst->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;
            PN_CHECK(pn_is_power_of_two(inst->alignment));

            pn_context_fix_value_ids(context, rel_id, 2, &inst->dest_id,
                                     &inst->value_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_CMP2: {
            PNInstructionId inst_id;
            PNInstructionCmp2* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionCmp2, module, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->type_id = pn_module_find_integer_type(module, 1);
            value->index = inst_id;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->value0_id = pn_record_read_uint32(&reader, "value 0");
            inst->value1_id = pn_record_read_uint32(&reader, "value 1");
            inst->cmp2_opcode = pn_record_read_int32(&reader, "opcode");

            pn_context_fix_value_ids(context, rel_id, 2, &inst->value0_id,
                                     &inst->value1_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_VSELECT: {
            PNInstructionId inst_id;
            PNInstructionVselect* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionVselect, module, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            /* Fix later, when all values are defined. */
            value->type_id = PN_INVALID_TYPE_ID;
            value->index = inst_id;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->true_value_id = pn_record_read_uint32(&reader, "true_value");
            inst->false_value_id =
                pn_record_read_uint32(&reader, "false_value");
            inst->cond_id = pn_record_read_uint32(&reader, "cond");

            pn_context_fix_value_ids(context, rel_id, 3, &inst->true_value_id,
                                     &inst->false_value_id, &inst->cond_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_FORWARDTYPEREF: {
            PNInstructionId inst_id;
            PNInstructionForwardtyperef* inst =
                PN_BASIC_BLOCK_APPEND_INSTRUCTION(PNInstructionForwardtyperef,
                                                  module, cur_bb, &inst_id);

            inst->base.code = code;
            inst->value_id = pn_record_read_int32(&reader, "value");
            inst->type_id = pn_record_read_int32(&reader, "type");
            break;
          }

          case PN_FUNCTION_CODE_INST_CALL:
          case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
            PNInstructionId inst_id;
            PNInstructionCall* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionCall, module, cur_bb, &inst_id);

            inst->base.code = code;
            inst->is_indirect = code == PN_FUNCTION_CODE_INST_CALL_INDIRECT;
            int32_t cc_info = pn_record_read_int32(&reader, "cc_info");
            inst->is_tail_call = cc_info & 1;
            inst->calling_convention = cc_info >> 1;
            inst->callee_id = pn_record_read_uint32(&reader, "callee");

            pn_context_fix_value_ids(context, rel_id, 1, &inst->callee_id);

            PNTypeId type_id;
            if (inst->is_indirect) {
              inst->return_type_id =
                  pn_record_read_int32(&reader, "return_type");
            } else {
              PNValue* function_value =
                  pn_module_get_value(module, inst->callee_id);
              assert(function_value->code == PN_VALUE_CODE_FUNCTION);
              PNFunction* called_function =
                  pn_module_get_function(module, function_value->index);
              type_id = called_function->type_id;
              PNType* function_type = pn_module_get_type(module, type_id);
              assert(function_type->code == PN_TYPE_CODE_FUNCTION);
              inst->return_type_id = function_type->return_type;
            }

            PNType* return_type =
                pn_module_get_type(module, inst->return_type_id);
            PNBool is_return_type_void = return_type->code == PN_TYPE_CODE_VOID;
            PNValueId value_id;
            if (!is_return_type_void) {
              PNValue* value =
                  pn_function_append_value(module, function, &value_id);
              value->code = PN_VALUE_CODE_LOCAL_VAR;
              value->type_id = inst->return_type_id;
              value->index = inst_id;

              inst->result_value_id = value_id;
            } else {
              value_id = pn_function_num_values(module, function);
              inst->result_value_id = PN_INVALID_VALUE_ID;
            }

            inst->num_args = 0;

            int32_t arg;
            while (pn_record_try_read_int32(&reader, &arg)) {
              if (context->use_relative_ids) {
                arg = value_id - arg;
              }

              pn_allocator_realloc_add(&module->instruction_allocator,
                                       (void**)&inst->arg_ids,
                                       sizeof(PNValueId), sizeof(PNValueId));
              inst->arg_ids[inst->num_args++] = arg;
            }
            break;
          }

          default:
            PN_FATAL("bad record code: %d.\n", code);
        }

        if (is_terminator) {
          PNValueId last_bb_value_id = pn_function_num_values(module, function);
          if (last_bb_value_id != first_bb_value_id) {
            cur_bb->first_def_id = first_bb_value_id;
            cur_bb->last_def_id = last_bb_value_id - 1;
          }

          cur_bb_id++;
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
  PN_FATAL("Unexpected end of stream.\n");
}

static void pn_module_block_read(PNModule* module,
                                 PNBlockInfoContext* context,
                                 PNBitStream* bs) {
  PN_BEGIN_TIME(MODULE_BLOCK_READ);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  PNFunctionId function_id = 0;

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        pn_bitstream_align_32(bs);
        PN_END_TIME(MODULE_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK: {
        uint32_t id = pn_bitstream_read_vbr(bs, 8);

        switch (id) {
          case PN_BLOCKID_BLOCKINFO:
            pn_blockinfo_block_read(context, bs);
            break;
          case PN_BLOCKID_TYPE:
            pn_type_block_read(module, context, bs);
            break;
          case PN_BLOCKID_GLOBALVAR:
            pn_globalvar_block_read(module, context, bs);
            break;
          case PN_BLOCKID_VALUE_SYMTAB:
            pn_value_symtab_block_read(module, context, bs);
            break;
          case PN_BLOCKID_FUNCTION: {
            while (pn_module_get_function(module, function_id)->is_proto) {
              function_id++;
            }

            pn_function_block_read(module, context, bs, function_id);
            function_id++;
            break;
          }
          default:
            PN_TRACE(MODULE_BLOCK, "*** SUBBLOCK (BAD) (%d)\n", id);
            PN_FATAL("bad block id %d\n", id);
        }

        break;
      }

      case PN_ENTRY_DEFINE_ABBREV: {
        PNBlockAbbrev* abbrev = pn_block_abbrev_read(bs, &abbrevs);
        uint32_t abbrev_id = abbrev - abbrevs.abbrevs;
        pn_abbrev_trace(abbrev, abbrev_id, PN_FALSE);
        break;
      }

      default: {
        /* Abbrev or UNABBREV_RECORD */
        uint32_t code;
        PNRecordReader reader;
        pn_record_reader_init(&reader, bs, &abbrevs, entry);
        pn_record_read_code(&reader, &code);

        switch (code) {
          case PN_MODULE_CODE_VERSION: {
            module->version = pn_record_read_int32(&reader, "module version");
            context->use_relative_ids = module->version == 1;
            PN_TRACE(MODULE_BLOCK, "version %d;\n", module->version);
            break;
          }

          case PN_MODULE_CODE_FUNCTION: {
            PNFunction* function = pn_allocator_realloc_add(
                &module->allocator, (void**)&module->functions,
                sizeof(PNFunction), PN_DEFAULT_ALIGN);
            PNFunctionId function_id = module->num_functions++;

            function->name = NULL;
            function->type_id = pn_record_read_int32(&reader, "type_id");
            function->intrinsic_id = PN_INTRINSIC_NULL;
            function->calling_convention =
                pn_record_read_int32(&reader, "calling_convention");
            function->is_proto = pn_record_read_int32(&reader, "is_proto");
            function->linkage = pn_record_read_int32(&reader, "linkage");
            function->num_constants = 0;
            function->constants = NULL;
            function->num_bbs = 0;
            function->bbs = NULL;
            function->num_values = 0;
            function->values = NULL;

            /* Cache number of arguments to function */
            PNType* function_type =
                pn_module_get_type(module, function->type_id);
            PN_CHECK(function_type->code == PN_TYPE_CODE_FUNCTION);
            function->num_args = function_type->num_args;

            PNValueId value_id;
            PNValue* value = pn_module_append_value(module, &value_id);
            value->code = PN_VALUE_CODE_FUNCTION;
            value->type_id = function->type_id;
            value->index = function_id;

            PN_TRACE(MODULE_BLOCK, "%s %s %s;\n",
                     function->is_proto ? "declare" : "define",
                     function->linkage ? "internal" : "external",
                     pn_type_describe_all(
                         module, function->type_id,
                         pn_value_describe(module, function, value_id),
                         PN_FALSE, PN_FALSE));
            break;
          }

          default:
            PN_FATAL("bad record code: %d.\n", code);
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
  PN_FATAL("Unexpected end of stream.\n");
}

/* Executor */

static int pn_string_list_count(char** p) {
  int result = 0;
  if (p) {
    for (; *p; ++p) {
      result++;
    }
  }
  return result;
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

#define DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(ty, ctype)             \
  static inline PNRuntimeValue pn_executor_value_##ty(ctype x) { \
    PNRuntimeValue ret;                                          \
    ret.ty = x;                                                  \
    return ret;                                                  \
  }

DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(i8, int8_t)
DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(u8, uint8_t)
DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(i16, int16_t)
DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(u16, uint16_t)
DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(i32, int32_t)
DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(u32, uint32_t)
DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(i64, int64_t)
DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(u64, uint64_t)
DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(f32, float)
DEFINE_EXECUTOR_VALUE_CONSTRUCTOR(f64, double)

#undef DEFINE_EXECUTOR_VALUE_CONSTRUCTOR

static PNRuntimeValue pn_thread_get_value(PNThread* thread,
                                          PNValueId value_id) {
  if (value_id >= thread->executor->module->num_values) {
    value_id -= thread->executor->module->num_values;
    return thread->current_call_frame->function_values[value_id];
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
  thread->current_call_frame->function_values[value_id] = value;
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
  PNCallFrame* prev_frame = thread->current_call_frame;

  frame->location.function_id = function_id;
  frame->location.bb_id = 0;
  frame->location.instruction_id = 0;
  frame->function_values = pn_allocator_alloc(
      &thread->allocator, sizeof(PNRuntimeValue) * function->num_values,
      sizeof(PNRuntimeValue));
  frame->memory_stack_top = prev_frame->memory_stack_top;
  frame->parent = prev_frame;
  frame->jmpbuf_head = NULL;
  frame->mark = pn_allocator_mark(&thread->allocator);

  thread->current_call_frame = frame;

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
                                     PNBasicBlock* bb,
                                     PNBasicBlockId dest_bb_id) {
  PNAllocatorMark mark = pn_allocator_mark(&thread->allocator);
  PNRuntimeValue* temp = pn_allocator_alloc(
      &thread->allocator, sizeof(PNRuntimeValue) * bb->num_phi_assigns,
      sizeof(PNRuntimeValue));

  /* First pass, read values to temp */
  uint32_t i;
  for (i = 0; i < bb->num_phi_assigns; ++i) {
    temp[i] = pn_thread_get_value(thread, bb->phi_assigns[i].source_value_id);
  }

  /* Second pass, write values from temp */
  for (i = 0; i < bb->num_phi_assigns; ++i) {
    PNPhiAssign* assign = &bb->phi_assigns[i];
    if (assign->bb_id == dest_bb_id) {
      pn_thread_set_value(thread, assign->dest_value_id, temp[i]);
      PN_TRACE(EXECUTE, "    %%%d <= %%%d\n", assign->dest_value_id,
               assign->source_value_id);
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
  executor->sentinel_frame.location.bb_id = PN_INVALID_BB_ID;
  executor->sentinel_frame.location.instruction_id = 0;
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
  thread->current_call_frame = &executor->sentinel_frame;
  thread->tls = 0;
  thread->id = executor->next_thread_id++;

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
        PN_PRINT("%s%%%d = %u%s", prefix, value_id, value.u8, postfix);
        break;
      case PN_BASIC_TYPE_INT8:
        PN_PRINT("%s%%%d = %u%s", prefix, value_id, value.u8, postfix);
        break;
      case PN_BASIC_TYPE_INT16:
        PN_PRINT("%s%%%d = %u%s", prefix, value_id, value.u16, postfix);
        break;
      case PN_BASIC_TYPE_INT32:
        PN_PRINT("%s%%%d = %u%s", prefix, value_id, value.u32, postfix);
        break;
      case PN_BASIC_TYPE_INT64:
        PN_PRINT("%s%%%d = %" PRIu64 "%s", prefix, value_id, value.u64,
                 postfix);
        break;
      case PN_BASIC_TYPE_FLOAT:
        PN_PRINT("%s%%%d = %f%s", prefix, value_id, value.f32, postfix);
        break;
      case PN_BASIC_TYPE_DOUBLE:
        PN_PRINT("%s%%%d = %f%s", prefix, value_id, value.f64, postfix);
        break;
      default:
        PN_UNREACHABLE();
        break;
    }
  }
}

static void pn_thread_backtrace(PNThread* thread) {
  PNModule* module = thread->executor->module;
  PNCallFrame* frame = thread->current_call_frame;
  int n = 0;
  while (frame != &thread->executor->sentinel_frame) {
    PNLocation* location = &frame->location;
    PNFunction* function = &module->functions[location->function_id];
    if (function->name && function->name[0]) {
      PN_PRINT("%d. %s(%d) %d %d\n", n, function->name, location->function_id,
               location->bb_id, location->instruction_id);
    } else {
      PN_PRINT("%d. %d %d %d\n", n, location->function_id, location->bb_id,
               location->instruction_id);
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

#define PN_BUILTIN_ARG(name, n, ty)                                  \
  PNRuntimeValue value##n = pn_thread_get_value(thread, arg_ids[n]); \
  pn_##ty name = value##n.ty;                                        \
  (void) name /* no semicolon */

static PNErrno pn_from_errno(int e) {
  switch (e) {
#define PN_ERRNO(name, id, str) case name: return PN_##name;
  PN_FOREACH_ERRNO(PN_ERRNO)
#undef PN_ERRNO
    default:
      PN_FATAL("Unknown errno: %d\n", e);
      return PN_ENOSYS;
  }
}

static PNRuntimeValue pn_builtin_NACL_IRT_QUERY(PNThread* thread,
                                                PNFunction* function,
                                                uint32_t num_args,
                                                PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 3);
  PN_BUILTIN_ARG(name_p, 0, u32);
  PN_BUILTIN_ARG(table, 1, u32);
  PN_BUILTIN_ARG(table_size, 2, u32);

  PNMemory* memory = executor->memory;
  pn_memory_check(memory, name_p, 1);

  uint32_t name_len = pn_memory_check_cstr(executor->memory, name_p);
  PN_CHECK(name_len > 0);

#define PN_WRITE_BUILTIN(offset, name)                           \
  pn_memory_write_u32(memory, table + offset * sizeof(uint32_t), \
                      pn_builtin_to_pointer(PN_BUILTIN_##name));

  const char* iface_name = memory->data + name_p;
  PN_TRACE(IRT, "    NACL_IRT_QUERY(%u (%s), %u, %u)\n", name_p, iface_name,
           table, table_size);

  if (strcmp(iface_name, "nacl-irt-basic-0.1") == 0) {
    PN_CHECK(table_size == 24);
    PN_WRITE_BUILTIN(0, NACL_IRT_BASIC_EXIT);
    PN_WRITE_BUILTIN(1, NACL_IRT_BASIC_GETTOD);
    PN_WRITE_BUILTIN(2, NACL_IRT_BASIC_CLOCK);
    PN_WRITE_BUILTIN(3, NACL_IRT_BASIC_NANOSLEEP);
    PN_WRITE_BUILTIN(4, NACL_IRT_BASIC_SCHED_YIELD);
    PN_WRITE_BUILTIN(5, NACL_IRT_BASIC_SYSCONF);
    return pn_executor_value_u32(24);
  } else if (strcmp(iface_name, "nacl-irt-dev-fdio-0.3") == 0) {
    PN_CHECK(table_size == 56);
    PN_WRITE_BUILTIN(0, NACL_IRT_FDIO_CLOSE);
    PN_WRITE_BUILTIN(1, NACL_IRT_FDIO_DUP);
    PN_WRITE_BUILTIN(2, NACL_IRT_FDIO_DUP2);
    PN_WRITE_BUILTIN(3, NACL_IRT_FDIO_READ);
    PN_WRITE_BUILTIN(4, NACL_IRT_FDIO_WRITE);
    PN_WRITE_BUILTIN(5, NACL_IRT_FDIO_SEEK);
    PN_WRITE_BUILTIN(6, NACL_IRT_FDIO_FSTAT);
    PN_WRITE_BUILTIN(7, NACL_IRT_FDIO_GETDENTS);
    PN_WRITE_BUILTIN(8, NACL_IRT_FDIO_FCHDIR);
    PN_WRITE_BUILTIN(9, NACL_IRT_FDIO_FCHMOD);
    PN_WRITE_BUILTIN(10, NACL_IRT_FDIO_FSYNC);
    PN_WRITE_BUILTIN(11, NACL_IRT_FDIO_FDATASYNC);
    PN_WRITE_BUILTIN(12, NACL_IRT_FDIO_FTRUNCATE);
    PN_WRITE_BUILTIN(13, NACL_IRT_FDIO_ISATTY);
    return pn_executor_value_u32(56);
  } else if (strcmp(iface_name, "nacl-irt-fdio-0.1") == 0) {
    PN_CHECK(table_size == 32);
    PN_WRITE_BUILTIN(0, NACL_IRT_FDIO_CLOSE);
    PN_WRITE_BUILTIN(1, NACL_IRT_FDIO_DUP);
    PN_WRITE_BUILTIN(2, NACL_IRT_FDIO_DUP2);
    PN_WRITE_BUILTIN(3, NACL_IRT_FDIO_READ);
    PN_WRITE_BUILTIN(4, NACL_IRT_FDIO_WRITE);
    PN_WRITE_BUILTIN(5, NACL_IRT_FDIO_SEEK);
    PN_WRITE_BUILTIN(6, NACL_IRT_FDIO_FSTAT);
    PN_WRITE_BUILTIN(7, NACL_IRT_FDIO_GETDENTS);
    return pn_executor_value_u32(32);
  } else if (strcmp(iface_name, "nacl-irt-dev-filename-0.3") == 0) {
    PN_CHECK(table_size == 64);
    PN_WRITE_BUILTIN(0, NACL_IRT_FILENAME_OPEN);
    PN_WRITE_BUILTIN(1, NACL_IRT_FILENAME_STAT);
    PN_WRITE_BUILTIN(2, NACL_IRT_FILENAME_MKDIR);
    PN_WRITE_BUILTIN(3, NACL_IRT_FILENAME_RMDIR);
    PN_WRITE_BUILTIN(4, NACL_IRT_FILENAME_CHDIR);
    PN_WRITE_BUILTIN(5, NACL_IRT_FILENAME_GETCWD);
    PN_WRITE_BUILTIN(6, NACL_IRT_FILENAME_UNLINK);
    PN_WRITE_BUILTIN(7, NACL_IRT_FILENAME_TRUNCATE);
    PN_WRITE_BUILTIN(8, NACL_IRT_FILENAME_LSTAT);
    PN_WRITE_BUILTIN(9, NACL_IRT_FILENAME_LINK);
    PN_WRITE_BUILTIN(10, NACL_IRT_FILENAME_RENAME);
    PN_WRITE_BUILTIN(11, NACL_IRT_FILENAME_SYMLINK);
    PN_WRITE_BUILTIN(12, NACL_IRT_FILENAME_CHMOD);
    PN_WRITE_BUILTIN(13, NACL_IRT_FILENAME_ACCESS);
    PN_WRITE_BUILTIN(14, NACL_IRT_FILENAME_READLINK);
    PN_WRITE_BUILTIN(15, NACL_IRT_FILENAME_UTIMES);
    return pn_executor_value_u32(64);
  } else if (strcmp(iface_name, "nacl-irt-memory-0.3") == 0) {
    PN_CHECK(table_size == 12);
    PN_WRITE_BUILTIN(0, NACL_IRT_MEMORY_MMAP);
    PN_WRITE_BUILTIN(1, NACL_IRT_MEMORY_MUNMAP);
    PN_WRITE_BUILTIN(2, NACL_IRT_MEMORY_MPROTECT);
    return pn_executor_value_u32(12);
  } else if (strcmp(iface_name, "nacl-irt-tls-0.1") == 0) {
    PN_CHECK(table_size == 8);
    PN_WRITE_BUILTIN(0, NACL_IRT_TLS_INIT);
    PN_WRITE_BUILTIN(1, NACL_IRT_TLS_GET);
    return pn_executor_value_u32(8);
  } else if (strcmp(iface_name, "nacl-irt-thread-0.1") == 0) {
    PN_CHECK(table_size == 12);
    PN_WRITE_BUILTIN(0, NACL_IRT_THREAD_CREATE);
    PN_WRITE_BUILTIN(1, NACL_IRT_THREAD_EXIT);
    PN_WRITE_BUILTIN(2, NACL_IRT_THREAD_NICE);
    return pn_executor_value_u32(12);
  } else if (strcmp(iface_name, "nacl-irt-futex-0.1") == 0) {
    PN_CHECK(table_size == 8);
    PN_WRITE_BUILTIN(0, NACL_IRT_FUTEX_WAIT_ABS);
    PN_WRITE_BUILTIN(1, NACL_IRT_FUTEX_WAKE);
    return pn_executor_value_u32(8);
  } else {
    PN_TRACE(IRT, "Unknown interface name: \"%s\".\n", iface_name);
    return pn_executor_value_u32(0);
  }

#undef PN_WRITE_BUILTIN
}

static PNRuntimeValue pn_builtin_NACL_IRT_BASIC_EXIT(PNThread* thread,
                                                     PNFunction* function,
                                                     uint32_t num_args,
                                                     PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(exit_code, 0, i32);
  PN_TRACE(IRT, "    NACL_IRT_BASIC_EXIT(%d)\n", exit_code);
  executor->exit_code = exit_code;
  executor->exiting = PN_TRUE;
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_BASIC_GETTOD(PNThread* thread,
                                                       PNFunction* function,
                                                       uint32_t num_args,
                                                       PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(tv_p, 0, i32);
  PN_TRACE(IRT, "    NACL_IRT_BASIC_GETTOD(%u)\n", tv_p);
  struct timeval tv;
  gettimeofday(&tv, NULL);

  /*
  * offset size
  * 0  8 time_t      tv_sec;
  * 8  4 suseconds_t tv_usec;
  * 16 total
  */
  pn_memory_write_u64(executor->memory, tv_p + 0, tv.tv_sec);
  pn_memory_write_u32(executor->memory, tv_p + 8, tv.tv_usec);
  PN_TRACE(IRT, "      tv_sec => %" PRId64 " tv_usec = %u\n",
           (int64_t)tv.tv_sec, (int32_t)tv.tv_usec);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_BASIC_SYSCONF(PNThread* thread,
                                                        PNFunction* function,
                                                        uint32_t num_args,
                                                        PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(name, 0, u32);
  PN_BUILTIN_ARG(value_p, 1, u32);
  PN_TRACE(IRT, "    NACL_IRT_BASIC_SYSCONF(%u, %u)\n", name, value_p);
  switch (name) {
    case 2: /* _SC_PAGESIZE */
      pn_memory_write_u32(executor->memory, value_p, PN_PAGESIZE);
      PN_TRACE(IRT, "      SC_PAGESIZE => %u\n", PN_PAGESIZE);
      break;
    default:
      return pn_executor_value_u32(PN_EINVAL);
  }
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FDIO_CLOSE(PNThread* thread,
                                                     PNFunction* function,
                                                     uint32_t num_args,
                                                     PNValueId* arg_ids) {
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(fd, 0, u32);
  PN_TRACE(IRT, "    NACL_IRT_FDIO_CLOSE(%u)\n", fd);
  if (fd > 2) {
    PN_TRACE(IRT, "      fd > 2, errno = EINVAL\n");
    return pn_executor_value_u32(PN_EINVAL);
  }
  /* Lie and say we closed the fd */
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FDIO_READ(PNThread* thread,
                                                    PNFunction* function,
                                                    uint32_t num_args,
                                                    PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 4);
  PN_BUILTIN_ARG(fd, 0, u32);
  PN_BUILTIN_ARG(buf_p, 1, u32);
  PN_BUILTIN_ARG(count, 2, u32);
  PN_BUILTIN_ARG(nread_p, 3, u32);
  PN_TRACE(IRT, "    NACL_IRT_FDIO_READ(%u, %u, %u, %u)\n", fd, buf_p, count,
           nread_p);
  if (fd != 0) {
    PN_TRACE(IRT, "      fd != 0, errno = EINVAL\n");
    return pn_executor_value_u32(PN_EINVAL);
  }

  pn_memory_check(executor->memory, buf_p, count);
  void* buf_pointer = executor->memory->data + buf_p;
  ssize_t nread = read(fd, buf_pointer, count);
  pn_memory_write_u32(executor->memory, nread_p, (int32_t)nread);
  PN_TRACE(IRT, "      returning %d\n", (int32_t)nread);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FDIO_WRITE(PNThread* thread,
                                                     PNFunction* function,
                                                     uint32_t num_args,
                                                     PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 4);
  PN_BUILTIN_ARG(fd, 0, u32);
  PN_BUILTIN_ARG(buf_p, 1, u32);
  PN_BUILTIN_ARG(count, 2, u32);
  PN_BUILTIN_ARG(nwrote_p, 3, u32);
  PN_TRACE(IRT, "    NACL_IRT_FDIO_WRITE(%u, %u, %u, %u)\n", fd, buf_p, count,
           nwrote_p);
  if (fd != 1 && fd != 2) {
    PN_TRACE(IRT, "      fd != 1 && fd != 2, errno = EINVAL\n");
    return pn_executor_value_u32(PN_EINVAL);
  }

  pn_memory_check(executor->memory, buf_p, count);
  void* buf_pointer = executor->memory->data + buf_p;
  ssize_t nwrote = write(fd, buf_pointer, count);
  pn_memory_write_u32(executor->memory, nwrote_p, (int32_t)nwrote);
  PN_TRACE(IRT, "      returning %d\n", (int32_t)nwrote);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FDIO_FSTAT(PNThread* thread,
                                                     PNFunction* function,
                                                     uint32_t num_args,
                                                     PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(fd, 0, u32);
  PN_BUILTIN_ARG(stat_p, 1, u32);
  PN_TRACE(IRT, "    NACL_IRT_FDIO_FSTAT(%u, %u)\n", fd, stat_p);
  if (fd > 2) {
    PN_TRACE(IRT, "      fd > 2, errno = EINVAL\n");
    return pn_executor_value_u32(PN_EINVAL);
  }

  /* TODO(binji): fake values? */
  struct stat buf;
  int result = fstat(fd, &buf);
  if (result != 0) {
    PNErrno err = pn_from_errno(errno);
    PN_TRACE(IRT, "      errno = %d\n", err);
    return pn_executor_value_u32(err);
  }

  /*
  * offset size
  * 0  8 dev_t     st_dev;
  * 8  8 ino_t     st_ino;
  * 16 4 mode_t    st_mode;
  * 20 4 nlink_t   st_nlink;
  * 24 4 uid_t     st_uid;
  * 28 4 gid_t     st_gid;
  * 32 8 dev_t     st_rdev;
  * 40 8 off_t     st_size;
  * 48 4 blksize_t st_blksize;
  * 52 4 blkcnt_t  st_blocks;
  * 56 8 time_t    st_atime;
  * 64 8 int64_t   st_atimensec;
  * 72 8 time_t    st_mtime;
  * 80 8 int64_t   st_mtimensec;
  * 88 8 time_t    st_ctime;
  * 96 8 int64_t   st_ctimensec;
  * 104 total
  */
  pn_memory_write_u64(executor->memory, stat_p + 0, buf.st_dev);
  pn_memory_write_u64(executor->memory, stat_p + 8, buf.st_ino);
  pn_memory_write_u32(executor->memory, stat_p + 16, buf.st_mode);
  pn_memory_write_u32(executor->memory, stat_p + 20, buf.st_nlink);
  pn_memory_write_u32(executor->memory, stat_p + 24, buf.st_uid);
  pn_memory_write_u32(executor->memory, stat_p + 28, buf.st_gid);
  pn_memory_write_u64(executor->memory, stat_p + 32, buf.st_rdev);
  pn_memory_write_u64(executor->memory, stat_p + 40, buf.st_size);
  pn_memory_write_u32(executor->memory, stat_p + 48, buf.st_blksize);
  pn_memory_write_u32(executor->memory, stat_p + 52, buf.st_blocks);
  pn_memory_write_u64(executor->memory, stat_p + 56, buf.st_atime);
  pn_memory_write_u64(executor->memory, stat_p + 64, buf.st_atim.tv_nsec);
  pn_memory_write_u64(executor->memory, stat_p + 72, buf.st_mtime);
  pn_memory_write_u64(executor->memory, stat_p + 80, buf.st_mtim.tv_nsec);
  pn_memory_write_u64(executor->memory, stat_p + 88, buf.st_ctime);
  pn_memory_write_u64(executor->memory, stat_p + 96, buf.st_ctim.tv_nsec);

  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FDIO_ISATTY(PNThread* thread,
                                                      PNFunction* function,
                                                      uint32_t num_args,
                                                      PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(fd, 0, u32);
  PN_BUILTIN_ARG(result_p, 1, u32);
  PN_TRACE(IRT, "    NACL_IRT_FDIO_ISATTY(%u, %u)\n", fd, result_p);

  if (fd > 2) {
    PN_TRACE(IRT, "      fd > 2, errno = EINVAL\n");
    return pn_executor_value_u32(PN_EINVAL);
  }

  int32_t result = isatty(fd);
  pn_memory_write_i32(executor->memory, result_p, result);
  PN_TRACE(IRT, "      returning %d, errno = 0\n", result);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FILENAME_GETCWD(PNThread* thread,
                                                          PNFunction* function,
                                                          uint32_t num_args,
                                                          PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(pathname_p, 0, u32);
  PN_BUILTIN_ARG(len, 1, u32);
  PN_TRACE(IRT, "    NACL_IRT_FILENAME_GETCWD(%u, %u)\n", pathname_p, len);

  pn_memory_check(executor->memory, pathname_p, len);
  void* pathname = executor->memory->data + pathname_p;
  void* result = getcwd(pathname, len);
  PNErrno err = result ? 0 : pn_from_errno(errno);
  PN_TRACE(IRT, "      returning %s, errno = %d\n", pathname, err);
  return pn_executor_value_u32(err);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FILENAME_OPEN(PNThread* thread,
                                                        PNFunction* function,
                                                        uint32_t num_args,
                                                        PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 4);
  PN_BUILTIN_ARG(pathname_p, 0, u32);
  PN_BUILTIN_ARG(oflag, 1, u32);
  PN_BUILTIN_ARG(cmode, 2, u32);
  PN_BUILTIN_ARG(newfd_p, 3, u32);

  pn_memory_check_cstr(executor->memory, pathname_p);
  char* pathname = executor->memory->data + pathname_p;
  PN_TRACE(IRT, "    NACL_IRT_FILENAME_OPEN(%u (%s), %u, %u, %u)\n", pathname_p,
           pathname, oflag, cmode, newfd_p);
  PN_TRACE(IRT, "      errno = ENOENT\n");
  (void)executor;
  (void)pathname;
  return pn_executor_value_u32(PN_ENOENT);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FILENAME_READLINK(
    PNThread* thread,
    PNFunction* function,
    uint32_t num_args,
    PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 4);
  PN_BUILTIN_ARG(path_p, 0, u32);
  PN_BUILTIN_ARG(buf_p, 1, u32);
  PN_BUILTIN_ARG(count, 2, u32);
  PN_BUILTIN_ARG(nread_p, 3, u32);

  pn_memory_check_cstr(executor->memory, path_p);
  char* path = executor->memory->data + path_p;

  PN_TRACE(IRT, "    NACL_IRT_FILENAME_READLINK(%u (%s), %u, %u, %u)\n", path_p,
           path, buf_p, count, nread_p);
  pn_memory_check(executor->memory, buf_p, count);
  void* buf = executor->memory->data + buf_p;
  ssize_t nread = readlink(path, buf, count);
  pn_memory_write_u32(executor->memory, nread_p, (int32_t)nread);
  PNErrno err = nread >= 0 ? 0 : pn_from_errno(errno);
  PN_TRACE(IRT, "      nread = %d, errno = %d\n", (int32_t)nread, err);
  return pn_executor_value_u32(err);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FILENAME_STAT(PNThread* thread,
                                                        PNFunction* function,
                                                        uint32_t num_args,
                                                        PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(pathname_p, 0, u32);
  PN_BUILTIN_ARG(stat_p, 1, u32);

  pn_memory_check_cstr(executor->memory, pathname_p);
  char* pathname = executor->memory->data + pathname_p;

  PN_TRACE(IRT, "    NACL_IRT_FILENAME_STAT(%u (%s), %u)\n", pathname_p,
           pathname, stat_p);

  struct stat buf;
  int result = stat(pathname, &buf);
  if (result != 0) {
    PNErrno err = pn_from_errno(errno);
    PN_TRACE(IRT, "      errno = %d\n", err);
    return pn_executor_value_u32(err);
  }

  pn_memory_write_u64(executor->memory, stat_p + 0, buf.st_dev);
  pn_memory_write_u64(executor->memory, stat_p + 8, buf.st_ino);
  pn_memory_write_u32(executor->memory, stat_p + 16, buf.st_mode);
  pn_memory_write_u32(executor->memory, stat_p + 20, buf.st_nlink);
  pn_memory_write_u32(executor->memory, stat_p + 24, buf.st_uid);
  pn_memory_write_u32(executor->memory, stat_p + 28, buf.st_gid);
  pn_memory_write_u64(executor->memory, stat_p + 32, buf.st_rdev);
  pn_memory_write_u64(executor->memory, stat_p + 40, buf.st_size);
  pn_memory_write_u32(executor->memory, stat_p + 48, buf.st_blksize);
  pn_memory_write_u32(executor->memory, stat_p + 52, buf.st_blocks);
  pn_memory_write_u64(executor->memory, stat_p + 56, buf.st_atime);
  pn_memory_write_u64(executor->memory, stat_p + 64, buf.st_atim.tv_nsec);
  pn_memory_write_u64(executor->memory, stat_p + 72, buf.st_mtime);
  pn_memory_write_u64(executor->memory, stat_p + 80, buf.st_mtim.tv_nsec);
  pn_memory_write_u64(executor->memory, stat_p + 88, buf.st_ctime);
  pn_memory_write_u64(executor->memory, stat_p + 96, buf.st_ctim.tv_nsec);

  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_MEMORY_MMAP(PNThread* thread,
                                                      PNFunction* function,
                                                      uint32_t num_args,
                                                      PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 6);
  PN_BUILTIN_ARG(addr_pp, 0, u32);
  PN_BUILTIN_ARG(len, 1, u32);
  PN_BUILTIN_ARG(prot, 2, u32);
  PN_BUILTIN_ARG(flags, 3, u32);
  PN_BUILTIN_ARG(fd, 4, i32);
  PN_BUILTIN_ARG(off, 5, u64);
  PN_TRACE(IRT, "    NACL_IRT_MEMORY_MMAP(%u, %u, %u, %u, %d, %" PRId64 ")\n",
           addr_pp, len, prot, flags, fd, off);

  if ((flags & 0x20) != 0x20) { /* MAP_ANONYMOUS */
    PN_TRACE(IRT, "      not anonymous, errno = EINVAL\n");
    return pn_executor_value_u32(PN_EINVAL);
  }

  /* TODO(binji): optimize */
  PNMemory* memory = executor->memory;
  assert(pn_is_aligned(executor->heap_end, PN_PAGESIZE));
  len = pn_align_up(len, PN_PAGESIZE);
  uint32_t pages = len >> PN_PAGESHIFT;
  uint32_t first_page = memory->heap_start >> PN_PAGESHIFT;
  uint32_t last_page = executor->heap_end >> PN_PAGESHIFT;
  uint32_t consecutive = 0;
  uint32_t result;
  uint32_t page_start;
  uint32_t new_heap_end;
  uint32_t i;
  PN_TRACE(IRT, "      Searching from [%d, %d)\n", first_page, last_page);
  for (i = first_page; i < last_page; ++i) {
    if (!pn_bitset_is_set(&executor->mapped_pages, i)) {
      if (consecutive == 0) {
        PN_TRACE(IRT, "      %d unmapped. starting. %d pages needed.\n", i,
                 pages);
        page_start = i;
      }
      if (++consecutive == pages) {
        /* Found a spot */
        PN_TRACE(IRT, "      found %d consecutive pages.\n", pages);
        result = page_start << PN_PAGESHIFT;
        goto found;
      }
    } else {
      if (consecutive > 0) {
        PN_TRACE(IRT, "      %d mapped.\n", i);
      }
      consecutive = 0;
    }
  }

  /* Move heap_end back, if possible */
  result = executor->heap_end;
  assert(pn_is_aligned(result, PN_PAGESIZE));
  pn_memory_check(memory, result, len);
  new_heap_end = executor->heap_end + len;
  if (new_heap_end >
      executor->main_thread.current_call_frame->memory_stack_top) {
    PN_FATAL("Out of heap\n");
  }
  executor->heap_end = new_heap_end;

found:
  for (i = 0; i < pages; ++i) {
    pn_bitset_set(&executor->mapped_pages, (result >> PN_PAGESHIFT) + i,
                  PN_TRUE);
  }
  pn_memory_write_u32(memory, addr_pp, result);
  PN_TRACE(IRT, "      returning %u, errno = 0\n", result);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_MEMORY_MUNMAP(PNThread* thread,
                                                        PNFunction* function,
                                                        uint32_t num_args,
                                                        PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 2);
  PN_BUILTIN_ARG(addr_p, 0, u32);
  PN_BUILTIN_ARG(len, 1, u32);

  PNMemory* memory = executor->memory;
  uint32_t old_addr_p = addr_p;
  addr_p = pn_align_down(addr_p, PN_PAGESIZE);
  len = pn_align_up(old_addr_p + len, PN_PAGESIZE) - addr_p;
  uint32_t pages = len >> PN_PAGESHIFT;
  uint32_t first_page = memory->heap_start >> PN_PAGESHIFT;
  uint32_t last_page = executor->heap_end >> PN_PAGESHIFT;
  uint32_t begin = addr_p >> PN_PAGESHIFT;
  uint32_t end = begin + pages;
  if (begin < first_page) {
    begin = first_page;
  }
  if (end > last_page) {
    end = last_page;
  }
  uint32_t i;
  for (i = begin; i < end; ++i) {
    pn_bitset_set(&executor->mapped_pages, i, PN_FALSE);
  }

  PN_TRACE(IRT, "    NACL_IRT_MEMORY_MUNMAP(%u, %u)\n", addr_p, len);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_TLS_INIT(PNThread* thread,
                                                   PNFunction* function,
                                                   uint32_t num_args,
                                                   PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(thread_ptr_p, 0, u32);
  PN_TRACE(IRT, "    NACL_IRT_TLS_INIT(%u)\n", thread_ptr_p);
  /* How big is TLS? */
  pn_memory_check(executor->memory, thread_ptr_p, 1);
  thread->tls = thread_ptr_p;
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_FUTEX_WAIT_ABS(PNThread* thread,
                                                         PNFunction* function,
                                                         uint32_t num_args,
                                                         PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  /* (Doc From irt.h)
  If |*addr| still contains |value|, futex_wait_abs() waits to be woken up by a
  futex_wake(addr,...) call from another thread; otherwise, it immediately
  returns EAGAIN (which is the same as EWOULDBLOCK).  If woken by another
  thread, it returns 0.  If |abstime| is non-NULL and the time specified by
  |*abstime| passes, this returns ETIMEDOUT.
  */
  PN_CHECK(num_args == 3);
  PN_BUILTIN_ARG(addr_p, 0, u32);
  PN_BUILTIN_ARG(value, 1, u32);
  PN_BUILTIN_ARG(abstime_p, 2, u32);
  switch (thread->futex_state) {
    case PN_FUTEX_NONE: {
      PN_TRACE(IRT, "    NACL_IRT_WAIT_ABS(%u, %u, %u)\n", addr_p, value,
               abstime_p);
      uint32_t read = pn_memory_read_u32(executor->memory, addr_p);
      if (read != value) {
        return pn_executor_value_u32(PN_EAGAIN);
      }

      thread->wait_addr = addr_p;

      if (abstime_p != 0) {
        thread->has_timeout = PN_TRUE;
        thread->timeout_sec = pn_memory_read_u64(executor->memory, abstime_p);
        /* IRT timeout is a timespec, which uses nanoseconds. Convert to
         * microseconds for comparison with gettimeofday */
        thread->timeout_usec =
            pn_memory_read_u32(executor->memory, abstime_p + 8) /
            PN_NANOSECONDS_IN_A_MICROSECOND;
      } else {
        thread->has_timeout = PN_FALSE;
        thread->timeout_sec = 0;
        thread->timeout_usec = 0;
      }

      thread->state = PN_THREAD_BLOCKED;

      /* Return an arbitrary value. We'll set the real value when the thread is
       * unblocked */
      return pn_executor_value_u32(0);
    }

    case PN_FUTEX_TIMEDOUT:
      PN_TRACE(IRT, "    NACL_IRT_WAIT_ABS(%u, %u, %u)\n", addr_p, value,
               abstime_p);
      PN_TRACE(IRT, "      errno = ETIMEDOUT (%d)\n", PN_ETIMEDOUT);
      thread->futex_state = PN_FUTEX_NONE;
      return pn_executor_value_u32(PN_ETIMEDOUT);

    case PN_FUTEX_WOKEN:
      PN_TRACE(IRT, "    NACL_IRT_WAIT_ABS(%u, %u, %u)\n", addr_p, value,
               abstime_p);
      PN_TRACE(IRT, "      errno = 0\n");
      thread->futex_state = PN_FUTEX_NONE;
      return pn_executor_value_u32(0);

    default:
      PN_UNREACHABLE();
      return pn_executor_value_u32(PN_EINVAL);
  }
}

static PNRuntimeValue pn_builtin_NACL_IRT_FUTEX_WAKE(PNThread* thread,
                                                     PNFunction* function,
                                                     uint32_t num_args,
                                                     PNValueId* arg_ids) {
  /*
  (Doc From irt.h)
  futex_wake() wakes up threads that are waiting on |addr| using futex_wait().
  |nwake| is the maximum number of threads that will be woken up.  The number
  of threads that were woken is returned in |*count|.
  */
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 3);
  PN_BUILTIN_ARG(addr_p, 0, u32);
  PN_BUILTIN_ARG(nwake, 1, u32);
  PN_BUILTIN_ARG(count_p, 2, u32);
  PN_TRACE(IRT, "    NACL_IRT_WAKE(%u, %u, %u)\n", addr_p, nwake, count_p);

  uint32_t woken = 0;
  PNThread* i;
  for (i = thread->next; i != thread && woken < nwake; i = i->next) {
    if (i->state == PN_THREAD_BLOCKED && i->wait_addr == addr_p) {
      PN_TRACE(IRT, "      waking thread %d\n", i->id);
      i->state = PN_THREAD_RUNNING;
      i->futex_state = PN_FUTEX_WOKEN;
      woken++;
    }
  }

  pn_memory_write_u32(executor->memory, count_p, woken);
  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_THREAD_CREATE(PNThread* thread,
                                                        PNFunction* function,
                                                        uint32_t num_args,
                                                        PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 3);
  PN_BUILTIN_ARG(start_func_p, 0, u32);
  PN_BUILTIN_ARG(stack_p, 1, u32);
  PN_BUILTIN_ARG(thread_p, 2, u32);
  PN_TRACE(IRT, "    NACL_IRT_THREAD_CREATE(%u, %u, %u)\n", start_func_p,
           stack_p, thread_p);
  PNThread* main_thread = &executor->main_thread;
  PNThread* new_thread;
  if (executor->dead_threads) {
    /* Dead thread list is singly-linked */
    new_thread = executor->dead_threads;
    executor->dead_threads = new_thread->next;
  } else {
    new_thread = pn_allocator_alloc(&executor->allocator, sizeof(PNThread),
                                    PN_DEFAULT_ALIGN);
  }

  pn_allocator_init(&new_thread->allocator, PN_MIN_CHUNKSIZE, "thread");
  new_thread->executor = executor;
  new_thread->current_call_frame = &executor->sentinel_frame;
  new_thread->tls = thread_p;
  new_thread->id = executor->next_thread_id++;
  new_thread->state = PN_THREAD_RUNNING;
  new_thread->futex_state = PN_FUTEX_NONE;
  new_thread->next = main_thread;
  new_thread->prev = main_thread->prev;
  main_thread->prev->next = new_thread;
  main_thread->prev = new_thread;

  PNFunctionId new_function_id = pn_function_pointer_to_index(start_func_p);
  assert(new_function_id >= PN_MAX_BUILTINS);
  new_function_id -= PN_MAX_BUILTINS;
  PN_CHECK(new_function_id < executor->module->num_functions);
  PNFunction* new_function =
      pn_module_get_function(executor->module, new_function_id);
  pn_thread_push_function(new_thread, new_function_id, new_function);
  new_thread->current_call_frame->memory_stack_top = stack_p;

  PN_TRACE(IRT, "      created thread %d\n", new_thread->id);

  return pn_executor_value_u32(0);
}

static PNRuntimeValue pn_builtin_NACL_IRT_THREAD_EXIT(PNThread* thread,
                                                      PNFunction* function,
                                                      uint32_t num_args,
                                                      PNValueId* arg_ids) {
  PNExecutor* executor = thread->executor;
  PN_CHECK(num_args == 1);
  PN_BUILTIN_ARG(stack_flag_p, 0, u32);
  PN_TRACE(IRT, "    NACL_IRT_THREAD_EXIT(%u)\n", stack_flag_p);
  PN_CHECK(thread != &executor->main_thread);
  thread->state = PN_THREAD_DEAD;

  if (stack_flag_p) {
    pn_memory_write_u32(executor->memory, stack_flag_p, 0);
  }

  return pn_executor_value_u32(0);
}

#define PN_BUILTIN_STUB(name)                                    \
  static PNRuntimeValue pn_builtin_##name(                       \
      PNThread* thread, PNFunction* function, uint32_t num_args, \
      PNValueId* arg_ids) {                                      \
    PN_TRACE(IRT, "    " #name "(...)\n");                       \
    return pn_executor_value_u32(PN_ENOSYS);                     \
  }

PN_BUILTIN_STUB(NACL_IRT_BASIC_CLOCK)
PN_BUILTIN_STUB(NACL_IRT_BASIC_NANOSLEEP)
PN_BUILTIN_STUB(NACL_IRT_BASIC_SCHED_YIELD)
PN_BUILTIN_STUB(NACL_IRT_FDIO_DUP)
PN_BUILTIN_STUB(NACL_IRT_FDIO_DUP2)
PN_BUILTIN_STUB(NACL_IRT_FDIO_SEEK)
PN_BUILTIN_STUB(NACL_IRT_FDIO_GETDENTS)
PN_BUILTIN_STUB(NACL_IRT_FDIO_FCHDIR)
PN_BUILTIN_STUB(NACL_IRT_FDIO_FCHMOD)
PN_BUILTIN_STUB(NACL_IRT_FDIO_FSYNC)
PN_BUILTIN_STUB(NACL_IRT_FDIO_FDATASYNC)
PN_BUILTIN_STUB(NACL_IRT_FDIO_FTRUNCATE)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_MKDIR)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_RMDIR)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_CHDIR)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_UNLINK)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_TRUNCATE)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_LSTAT)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_LINK)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_RENAME)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_SYMLINK)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_CHMOD)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_ACCESS)
PN_BUILTIN_STUB(NACL_IRT_FILENAME_UTIMES)
PN_BUILTIN_STUB(NACL_IRT_MEMORY_MPROTECT)
PN_BUILTIN_STUB(NACL_IRT_TLS_GET)
PN_BUILTIN_STUB(NACL_IRT_THREAD_NICE)

#undef PN_BUILTIN_STUB

#undef PN_BUILTIN_ARG

static uint32_t g_pn_opcode_count[PN_MAX_OPCODE];

static void pn_thread_execute_instruction(PNThread* thread) {
  PNExecutor* executor = thread->executor;
  PNCallFrame* frame = thread->current_call_frame;
  PNLocation* location = &frame->location;
  PNModule* module = executor->module;
  PNFunction* function = &module->functions[location->function_id];
  PNBasicBlock* bb = &function->bbs[location->bb_id];
  PNInstruction* inst = bb->instructions[location->instruction_id];

#if PN_TRACING
  if (PN_IS_TRACE(EXECUTE)) {
    pn_instruction_trace(module, function, inst, PN_TRUE);
  }
#endif

  g_pn_opcode_count[inst->opcode]++;

  switch (inst->opcode) {
    case PN_OPCODE_ALLOCA_INT32: {
      PNInstructionAlloca* i = (PNInstructionAlloca*)inst;
      PNRuntimeValue size = pn_thread_get_value(thread, i->size_id);
      frame->memory_stack_top =
          pn_align_down(frame->memory_stack_top - size.i32, i->alignment);
      if (thread == &executor->main_thread &&
          frame->memory_stack_top < executor->heap_end) {
        PN_FATAL("Out of stack\n");
        break;
      }
      PNRuntimeValue result;
      result.u32 = frame->memory_stack_top;
      pn_thread_set_value(thread, i->result_value_id, result);
      PN_TRACE(EXECUTE, "    %%%d = %u  %%%d = %d\n", i->result_value_id,
               result.u32, i->size_id, size.i32);
      location->instruction_id++;
      break;
    }

#define OPCODE_BINOP(op, ty)                                                  \
  do {                                                                        \
    PNInstructionBinop* i = (PNInstructionBinop*)inst;                        \
    PNRuntimeValue value0 = pn_thread_get_value(thread, i->value0_id);        \
    PNRuntimeValue value1 = pn_thread_get_value(thread, i->value1_id);        \
    PNRuntimeValue result = pn_executor_value_##ty(value0.ty op value1.ty);   \
    pn_thread_set_value(thread, i->result_value_id, result);                  \
    PN_TRACE(EXECUTE, "    %%%d = " PN_FORMAT_##ty "  %%%d = " PN_FORMAT_##ty \
             "  %%%d = " PN_FORMAT_##ty "\n",                                 \
             i->result_value_id, result.ty, i->value0_id, value0.ty,          \
             i->value1_id, value1.ty);                                        \
    location->instruction_id++;                                               \
  } while (0) /* no semicolon */

    case PN_OPCODE_BINOP_ADD_DOUBLE:  OPCODE_BINOP(+, f64); break;
    case PN_OPCODE_BINOP_ADD_FLOAT:   OPCODE_BINOP(+, f32); break;
    case PN_OPCODE_BINOP_ADD_INT8:    OPCODE_BINOP(+, u8); break;
    case PN_OPCODE_BINOP_ADD_INT16:   OPCODE_BINOP(+, u16); break;
    case PN_OPCODE_BINOP_ADD_INT32:   OPCODE_BINOP(+, u32); break;
    case PN_OPCODE_BINOP_ADD_INT64:   OPCODE_BINOP(+, u64); break;
    case PN_OPCODE_BINOP_AND_INT1:
    case PN_OPCODE_BINOP_AND_INT8:    OPCODE_BINOP(&, u8); break;
    case PN_OPCODE_BINOP_AND_INT16:   OPCODE_BINOP(&, u16); break;
    case PN_OPCODE_BINOP_AND_INT32:   OPCODE_BINOP(&, u32); break;
    case PN_OPCODE_BINOP_AND_INT64:   OPCODE_BINOP(&, u64); break;
    case PN_OPCODE_BINOP_ASHR_INT8:   OPCODE_BINOP(>>, i8); break;
    case PN_OPCODE_BINOP_ASHR_INT16:  OPCODE_BINOP(>>, i16); break;
    case PN_OPCODE_BINOP_ASHR_INT32:  OPCODE_BINOP(>>, i32); break;
    case PN_OPCODE_BINOP_ASHR_INT64:  OPCODE_BINOP(>>, i64); break;
    case PN_OPCODE_BINOP_LSHR_INT8:   OPCODE_BINOP(>>, u8); break;
    case PN_OPCODE_BINOP_LSHR_INT16:  OPCODE_BINOP(>>, u16); break;
    case PN_OPCODE_BINOP_LSHR_INT32:  OPCODE_BINOP(>>, u32); break;
    case PN_OPCODE_BINOP_LSHR_INT64:  OPCODE_BINOP(>>, u64); break;
    case PN_OPCODE_BINOP_MUL_DOUBLE:  OPCODE_BINOP(*, f64); break;
    case PN_OPCODE_BINOP_MUL_FLOAT:   OPCODE_BINOP(*, f32); break;
    case PN_OPCODE_BINOP_MUL_INT8:    OPCODE_BINOP(*, u8); break;
    case PN_OPCODE_BINOP_MUL_INT16:   OPCODE_BINOP(*, u16); break;
    case PN_OPCODE_BINOP_MUL_INT32:   OPCODE_BINOP(*, u32); break;
    case PN_OPCODE_BINOP_MUL_INT64:   OPCODE_BINOP(*, u64); break;
    case PN_OPCODE_BINOP_OR_INT1:
    case PN_OPCODE_BINOP_OR_INT8:     OPCODE_BINOP(|, u8); break;
    case PN_OPCODE_BINOP_OR_INT16:    OPCODE_BINOP(|, u16); break;
    case PN_OPCODE_BINOP_OR_INT32:    OPCODE_BINOP(|, u32); break;
    case PN_OPCODE_BINOP_OR_INT64:    OPCODE_BINOP(|, u64); break;
    case PN_OPCODE_BINOP_SDIV_DOUBLE: OPCODE_BINOP(/, f64); break;
    case PN_OPCODE_BINOP_SDIV_FLOAT:  OPCODE_BINOP(/, f32); break;
    case PN_OPCODE_BINOP_SDIV_INT32:  OPCODE_BINOP(/, i32); break;
    case PN_OPCODE_BINOP_SDIV_INT64:  OPCODE_BINOP(/, i64); break;
    case PN_OPCODE_BINOP_SHL_INT8:    OPCODE_BINOP(<<, u8); break;
    case PN_OPCODE_BINOP_SHL_INT16:   OPCODE_BINOP(<<, u16); break;
    case PN_OPCODE_BINOP_SHL_INT32:   OPCODE_BINOP(<<, u32); break;
    case PN_OPCODE_BINOP_SHL_INT64:   OPCODE_BINOP(<<, u64); break;
    case PN_OPCODE_BINOP_SREM_INT32:  OPCODE_BINOP(%, i32); break;
    case PN_OPCODE_BINOP_SREM_INT64:  OPCODE_BINOP(%, i64); break;
    case PN_OPCODE_BINOP_SUB_DOUBLE:  OPCODE_BINOP(-, f64); break;
    case PN_OPCODE_BINOP_SUB_FLOAT:   OPCODE_BINOP(-, f32); break;
    case PN_OPCODE_BINOP_SUB_INT8:    OPCODE_BINOP(-, u8); break;
    case PN_OPCODE_BINOP_SUB_INT16:   OPCODE_BINOP(-, u16); break;
    case PN_OPCODE_BINOP_SUB_INT32:   OPCODE_BINOP(-, u32); break;
    case PN_OPCODE_BINOP_SUB_INT64:   OPCODE_BINOP(-, u64); break;
    case PN_OPCODE_BINOP_UDIV_INT8:   OPCODE_BINOP(/, u8); break;
    case PN_OPCODE_BINOP_UDIV_INT16:  OPCODE_BINOP(/, u16); break;
    case PN_OPCODE_BINOP_UDIV_INT32:  OPCODE_BINOP(/, u32); break;
    case PN_OPCODE_BINOP_UDIV_INT64:  OPCODE_BINOP(/, u64); break;
    case PN_OPCODE_BINOP_UREM_INT8:   OPCODE_BINOP(%, u8); break;
    case PN_OPCODE_BINOP_UREM_INT16:  OPCODE_BINOP(%, u16); break;
    case PN_OPCODE_BINOP_UREM_INT32:  OPCODE_BINOP(%, u32); break;
    case PN_OPCODE_BINOP_UREM_INT64:  OPCODE_BINOP(%, u64); break;
    case PN_OPCODE_BINOP_XOR_INT1:
    case PN_OPCODE_BINOP_XOR_INT8:    OPCODE_BINOP(^, u8); break;
    case PN_OPCODE_BINOP_XOR_INT16:   OPCODE_BINOP(^, u16); break;
    case PN_OPCODE_BINOP_XOR_INT32:   OPCODE_BINOP(^, u32); break;
    case PN_OPCODE_BINOP_XOR_INT64:   OPCODE_BINOP(^, u64); break;

#undef OPCODE_BINOP

    case PN_OPCODE_BR: {
      PNInstructionBr* i = (PNInstructionBr*)inst;
      PNBasicBlockId new_bb_id = i->true_bb_id;
      pn_thread_do_phi_assigns(thread, &function->bbs[location->bb_id],
                               new_bb_id);
      location->bb_id = new_bb_id;
      location->instruction_id = 0;
      PN_TRACE(EXECUTE, "bb = %d\n", location->bb_id);
      break;
    }

    case PN_OPCODE_BR_INT1: {
      PNInstructionBr* i = (PNInstructionBr*)inst;
      PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);
      PNBasicBlockId new_bb_id = value.u8 ? i->true_bb_id : i->false_bb_id;
      pn_thread_do_phi_assigns(thread, &function->bbs[location->bb_id],
                               new_bb_id);
      location->bb_id = new_bb_id;
      location->instruction_id = 0;
      PN_TRACE(EXECUTE, "    %%%d = %u\n", i->value_id, value.u8);
      PN_TRACE(EXECUTE, "bb = %d\n", new_bb_id);
      break;
    }

    case PN_OPCODE_CALL:
    case PN_OPCODE_CALL_INDIRECT: {
      PNInstructionCall* i = (PNInstructionCall*)inst;

      PNFunctionId new_function_id;
      if (i->is_indirect) {
        PNRuntimeValue function_value =
            pn_thread_get_value(thread, i->callee_id);
        PNFunctionId callee_function_id =
            pn_function_pointer_to_index(function_value.u32);
        if (callee_function_id < PN_MAX_BUILTINS) {
          /* Builtin function. Call it directly, don't set up a new frame */
          switch (callee_function_id) {
#define PN_BUILTIN(e)                                              \
  case PN_BUILTIN_##e: {                                           \
    PNRuntimeValue result =                                        \
        pn_builtin_##e(thread, function, i->num_args, i->arg_ids); \
    if (i->result_value_id != PN_INVALID_VALUE_ID) {               \
      pn_thread_set_value(thread, i->result_value_id, result);     \
    }                                                              \
    break;                                                         \
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
            location->instruction_id++;
          }
          break;
        } else {
          new_function_id = callee_function_id - PN_MAX_BUILTINS;
          assert(new_function_id < executor->module->num_functions);
          PN_TRACE(EXECUTE, "    %%%d = %u ", i->callee_id, function_value.u32);
        }
      } else {
        PNValue* function_value =
            pn_module_get_value(executor->module, i->callee_id);
        assert(function_value->code == PN_VALUE_CODE_FUNCTION);
        new_function_id = function_value->index;
        PN_TRACE(EXECUTE, "    ");
      }

      PNFunction* new_function =
          pn_module_get_function(executor->module, new_function_id);

      pn_thread_push_function(thread, new_function_id, new_function);

      uint32_t n;
      for (n = 0; n < i->num_args; ++n) {
        PNValueId value_id = executor->module->num_values + n;
        PNRuntimeValue arg =
            pn_executor_get_value_from_frame(executor, frame, i->arg_ids[n]);

        pn_executor_value_trace(executor, function, i->arg_ids[n], arg, "",
                                "  ");
        pn_thread_set_value(thread, value_id, arg);
      }

      PN_TRACE(EXECUTE, "\nfunction = %d bb = 0\n", new_function_id);
      break;
    }

    case PN_OPCODE_CAST_BITCAST_DOUBLE_INT64:
    case PN_OPCODE_CAST_BITCAST_FLOAT_INT32:
    case PN_OPCODE_CAST_BITCAST_INT32_FLOAT:
    case PN_OPCODE_CAST_BITCAST_INT64_DOUBLE: {
      PNInstructionCast* i = (PNInstructionCast*)inst;
      PNRuntimeValue result = pn_thread_get_value(thread, i->value_id);
      pn_thread_set_value(thread, i->result_value_id, result);
      pn_executor_value_trace(executor, function, i->result_value_id, result,
                              "    ", "\n");
      pn_executor_value_trace(executor, function, i->value_id, result, "    ",
                              "\n");

      location->instruction_id++;
      break;
    }

#define OPCODE_CAST(from, to)                                                \
  do {                                                                       \
    PNInstructionCast* i = (PNInstructionCast*)inst;                         \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);         \
    PNRuntimeValue result = pn_executor_value_##to(value.from);              \
    pn_thread_set_value(thread, i->result_value_id, result);                 \
    PN_TRACE(EXECUTE,                                                        \
             "    %%%d = " PN_FORMAT_##to "  %%%d = " PN_FORMAT_##from "\n", \
             i->result_value_id, result.to, i->value_id, result.from);       \
    location->instruction_id++;                                              \
  } while (0) /* no semicolon */

#define OPCODE_CAST_SEXT1(size)                                           \
  do {                                                                    \
    PNInstructionCast* i = (PNInstructionCast*)inst;                      \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);      \
    PNRuntimeValue result =                                               \
        pn_executor_value_i##size(-(int##size##_t)(value.u8 & 1));        \
    pn_thread_set_value(thread, i->result_value_id, result);              \
    PN_TRACE(EXECUTE, "    %%%d = " PN_FORMAT_i##size "  %%%d = %u\n",    \
             i->result_value_id, result.i##size, i->value_id, result.u8); \
    location->instruction_id++;                                           \
  } while (0) /* no semicolon */

#define OPCODE_CAST_TRUNC1(size)                                          \
  do {                                                                    \
    PNInstructionCast* i = (PNInstructionCast*)inst;                      \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);      \
    PNRuntimeValue result = pn_executor_value_u8(value.u##size & 1);      \
    pn_thread_set_value(thread, i->result_value_id, result);              \
    PN_TRACE(EXECUTE, "    %%%d = %u  %%%d = " PN_FORMAT_u##size "\n",    \
             i->result_value_id, result.u8, i->value_id, result.u##size); \
    location->instruction_id++;                                           \
  } while (0) /* no semicolon */

#define OPCODE_CAST_ZEXT1(size)                                           \
  do {                                                                    \
    PNInstructionCast* i = (PNInstructionCast*)inst;                      \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);      \
    PNRuntimeValue result =                                               \
        pn_executor_value_u##size((uint##size##_t)(value.u8 & 1));        \
    pn_thread_set_value(thread, i->result_value_id, result);              \
    PN_TRACE(EXECUTE, "    %%%d = " PN_FORMAT_u##size "  %%%d = %u\n",    \
             i->result_value_id, result.u##size, i->value_id, result.u8); \
    location->instruction_id++;                                           \
  } while (0) /* no semicolon */

    case PN_OPCODE_CAST_FPEXT_FLOAT_DOUBLE:   OPCODE_CAST(f32, f64); break;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT8:   OPCODE_CAST(f64, i8); break;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT16:  OPCODE_CAST(f64, i16); break;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT32:  OPCODE_CAST(f64, i32); break;
    case PN_OPCODE_CAST_FPTOSI_DOUBLE_INT64:  OPCODE_CAST(f64, i64); break;
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT8:    OPCODE_CAST(f32, i8); break;
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT16:   OPCODE_CAST(f32, i16); break;
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT32:   OPCODE_CAST(f32, i32); break;
    case PN_OPCODE_CAST_FPTOSI_FLOAT_INT64:   OPCODE_CAST(f32, i64); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT8:   OPCODE_CAST(f64, u8); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT16:  OPCODE_CAST(f64, u16); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT32:  OPCODE_CAST(f64, u32); break;
    case PN_OPCODE_CAST_FPTOUI_DOUBLE_INT64:  OPCODE_CAST(f64, u64); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT8:    OPCODE_CAST(f32, u8); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT16:   OPCODE_CAST(f32, u16); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT32:   OPCODE_CAST(f32, u32); break;
    case PN_OPCODE_CAST_FPTOUI_FLOAT_INT64:   OPCODE_CAST(f32, u64); break;
    case PN_OPCODE_CAST_FPTRUNC_DOUBLE_FLOAT: OPCODE_CAST(f64, f32); break;
    case PN_OPCODE_CAST_SEXT_INT1_INT8:       OPCODE_CAST_SEXT1(8); break;
    case PN_OPCODE_CAST_SEXT_INT1_INT16:      OPCODE_CAST_SEXT1(16); break;
    case PN_OPCODE_CAST_SEXT_INT1_INT32:      OPCODE_CAST_SEXT1(32); break;
    case PN_OPCODE_CAST_SEXT_INT1_INT64:      OPCODE_CAST_SEXT1(64); break;
    case PN_OPCODE_CAST_SEXT_INT8_INT16:      OPCODE_CAST(i8, i16); break;
    case PN_OPCODE_CAST_SEXT_INT8_INT32:      OPCODE_CAST(i8, i32); break;
    case PN_OPCODE_CAST_SEXT_INT8_INT64:      OPCODE_CAST(i8, i64); break;
    case PN_OPCODE_CAST_SEXT_INT16_INT32:     OPCODE_CAST(i16, i32); break;
    case PN_OPCODE_CAST_SEXT_INT16_INT64:     OPCODE_CAST(i16, i64); break;
    case PN_OPCODE_CAST_SEXT_INT32_INT64:     OPCODE_CAST(i32, i64); break;
    case PN_OPCODE_CAST_SITOFP_INT8_DOUBLE:   OPCODE_CAST(i8, f64); break;
    case PN_OPCODE_CAST_SITOFP_INT8_FLOAT:    OPCODE_CAST(i8, f32); break;
    case PN_OPCODE_CAST_SITOFP_INT16_DOUBLE:  OPCODE_CAST(i16, f64); break;
    case PN_OPCODE_CAST_SITOFP_INT16_FLOAT:   OPCODE_CAST(i16, f32); break;
    case PN_OPCODE_CAST_SITOFP_INT32_DOUBLE:  OPCODE_CAST(i32, f64); break;
    case PN_OPCODE_CAST_SITOFP_INT32_FLOAT:   OPCODE_CAST(i32, f32); break;
    case PN_OPCODE_CAST_SITOFP_INT64_DOUBLE:  OPCODE_CAST(i64, f64); break;
    case PN_OPCODE_CAST_SITOFP_INT64_FLOAT:   OPCODE_CAST(i64, f32); break;
    case PN_OPCODE_CAST_TRUNC_INT8_INT1:      OPCODE_CAST_TRUNC1(8); break;
    case PN_OPCODE_CAST_TRUNC_INT16_INT1:     OPCODE_CAST_TRUNC1(16); break;
    case PN_OPCODE_CAST_TRUNC_INT16_INT8:     OPCODE_CAST(i16, i8); break;
    case PN_OPCODE_CAST_TRUNC_INT32_INT1:     OPCODE_CAST_TRUNC1(32); break;
    case PN_OPCODE_CAST_TRUNC_INT32_INT8:     OPCODE_CAST(i32, i8); break;
    case PN_OPCODE_CAST_TRUNC_INT32_INT16:    OPCODE_CAST(i32, i16); break;
    case PN_OPCODE_CAST_TRUNC_INT64_INT8:     OPCODE_CAST(i64, i8); break;
    case PN_OPCODE_CAST_TRUNC_INT64_INT16:    OPCODE_CAST(i64, i16); break;
    case PN_OPCODE_CAST_TRUNC_INT64_INT32:    OPCODE_CAST(i64, i32); break;
    case PN_OPCODE_CAST_UITOFP_INT8_DOUBLE:   OPCODE_CAST(u8, f64); break;
    case PN_OPCODE_CAST_UITOFP_INT8_FLOAT:    OPCODE_CAST(u8, f32); break;
    case PN_OPCODE_CAST_UITOFP_INT16_DOUBLE:  OPCODE_CAST(u16, f64); break;
    case PN_OPCODE_CAST_UITOFP_INT16_FLOAT:   OPCODE_CAST(u16, f32); break;
    case PN_OPCODE_CAST_UITOFP_INT32_DOUBLE:  OPCODE_CAST(u32, f64); break;
    case PN_OPCODE_CAST_UITOFP_INT32_FLOAT:   OPCODE_CAST(u32, f32); break;
    case PN_OPCODE_CAST_UITOFP_INT64_DOUBLE:  OPCODE_CAST(u64, f64); break;
    case PN_OPCODE_CAST_UITOFP_INT64_FLOAT:   OPCODE_CAST(u64, f32); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT8:       OPCODE_CAST_ZEXT1(8); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT16:      OPCODE_CAST_ZEXT1(16); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT32:      OPCODE_CAST_ZEXT1(32); break;
    case PN_OPCODE_CAST_ZEXT_INT1_INT64:      OPCODE_CAST_ZEXT1(64); break;
    case PN_OPCODE_CAST_ZEXT_INT8_INT16:      OPCODE_CAST(u8, u16); break;
    case PN_OPCODE_CAST_ZEXT_INT8_INT32:      OPCODE_CAST(u8, u32); break;
    case PN_OPCODE_CAST_ZEXT_INT8_INT64:      OPCODE_CAST(u8, u64); break;
    case PN_OPCODE_CAST_ZEXT_INT16_INT32:     OPCODE_CAST(u16, u32); break;
    case PN_OPCODE_CAST_ZEXT_INT16_INT64:     OPCODE_CAST(u16, u64); break;
    case PN_OPCODE_CAST_ZEXT_INT32_INT64:     OPCODE_CAST(u32, u64); break;

#undef OPCODE_CAST
#undef OPCODE_CAST_SEXT1
#undef OPCODE_CAST_TRUNC1
#undef OPCODE_CAST_ZEXT1

#define OPCODE_CMP2(op, ty)                                               \
  do {                                                                    \
    PNInstructionCmp2* i = (PNInstructionCmp2*)inst;                      \
    PNRuntimeValue value0 = pn_thread_get_value(thread, i->value0_id);    \
    PNRuntimeValue value1 = pn_thread_get_value(thread, i->value1_id);    \
    PNRuntimeValue result = pn_executor_value_u8(value0.ty op value1.ty); \
    pn_thread_set_value(thread, i->result_value_id, result);              \
    PN_TRACE(EXECUTE, "    %%%d = %u  %%%d = " PN_FORMAT_##ty             \
             "  %%%d = " PN_FORMAT_##ty "\n",                             \
             i->result_value_id, result.u8, i->value0_id, value0.ty,      \
             i->value1_id, value1.ty);                                    \
    location->instruction_id++;                                           \
  } while (0) /* no semicolon */

#define OPCODE_CMP2_NOT(op, ty)                                              \
  do {                                                                       \
    PNInstructionCmp2* i = (PNInstructionCmp2*)inst;                         \
    PNRuntimeValue value0 = pn_thread_get_value(thread, i->value0_id);       \
    PNRuntimeValue value1 = pn_thread_get_value(thread, i->value1_id);       \
    PNRuntimeValue result = pn_executor_value_u8(!(value0.ty op value1.ty)); \
    pn_thread_set_value(thread, i->result_value_id, result);                 \
    PN_TRACE(EXECUTE, "    %%%d = %u  %%%d = " PN_FORMAT_##ty                \
             "  %%%d = " PN_FORMAT_##ty "\n",                                \
             i->result_value_id, result.u8, i->value0_id, value0.ty,         \
             i->value1_id, value1.ty);                                       \
    location->instruction_id++;                                              \
  } while (0) /* no semicolon */

#define OPCODE_CMP2_ORD(ty)                                                \
  do {                                                                     \
    PNInstructionCmp2* i = (PNInstructionCmp2*)inst;                       \
    PNRuntimeValue value0 = pn_thread_get_value(thread, i->value0_id);     \
    PNRuntimeValue value1 = pn_thread_get_value(thread, i->value1_id);     \
    PNRuntimeValue result = pn_executor_value_u8(value0.ty == value1.ty || \
                                                 value0.ty != value1.ty);  \
    pn_thread_set_value(thread, i->result_value_id, result);               \
    PN_TRACE(EXECUTE, "    %%%d = %u  %%%d = " PN_FORMAT_##ty              \
             "  %%%d = " PN_FORMAT_##ty "\n",                              \
             i->result_value_id, result.u8, i->value0_id, value0.ty,       \
             i->value1_id, value1.ty);                                     \
    location->instruction_id++;                                            \
  } while (0) /* no semicolon */

#define OPCODE_CMP2_UNO(ty)                                            \
  do {                                                                 \
    PNInstructionCmp2* i = (PNInstructionCmp2*)inst;                   \
    PNRuntimeValue value0 = pn_thread_get_value(thread, i->value0_id); \
    PNRuntimeValue value1 = pn_thread_get_value(thread, i->value1_id); \
    PNRuntimeValue result = pn_executor_value_u8(                      \
        !(value0.ty == value1.ty || value0.ty != value1.ty));          \
    pn_thread_set_value(thread, i->result_value_id, result);           \
    PN_TRACE(EXECUTE, "    %%%d = %u  %%%d = " PN_FORMAT_##ty          \
             "  %%%d = " PN_FORMAT_##ty "\n",                          \
             i->result_value_id, result.u8, i->value0_id, value0.ty,   \
             i->value1_id, value1.ty);                                 \
    location->instruction_id++;                                        \
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

    case PN_OPCODE_FCMP_OEQ_DOUBLE: OPCODE_CMP2(==, f64); break;
    case PN_OPCODE_FCMP_OEQ_FLOAT:  OPCODE_CMP2(==, f32); break;
    case PN_OPCODE_FCMP_OGE_DOUBLE: OPCODE_CMP2(>=, f64); break;
    case PN_OPCODE_FCMP_OGE_FLOAT:  OPCODE_CMP2(>=, f32); break;
    case PN_OPCODE_FCMP_OGT_DOUBLE: OPCODE_CMP2(>, f64); break;
    case PN_OPCODE_FCMP_OGT_FLOAT:  OPCODE_CMP2(>, f32); break;
    case PN_OPCODE_FCMP_OLE_DOUBLE: OPCODE_CMP2(<=, f64); break;
    case PN_OPCODE_FCMP_OLE_FLOAT:  OPCODE_CMP2(<=, f32); break;
    case PN_OPCODE_FCMP_OLT_DOUBLE: OPCODE_CMP2(<, f64); break;
    case PN_OPCODE_FCMP_OLT_FLOAT:  OPCODE_CMP2(<, f32); break;
    case PN_OPCODE_FCMP_ONE_DOUBLE: OPCODE_CMP2(!=, f64); break;
    case PN_OPCODE_FCMP_ONE_FLOAT:  OPCODE_CMP2(!=, f32); break;
    case PN_OPCODE_FCMP_ORD_DOUBLE: OPCODE_CMP2_ORD(f64); break;
    case PN_OPCODE_FCMP_ORD_FLOAT:  OPCODE_CMP2_ORD(f32); break;
    case PN_OPCODE_FCMP_UEQ_DOUBLE: OPCODE_CMP2_NOT(!=, f64); break;
    case PN_OPCODE_FCMP_UEQ_FLOAT:  OPCODE_CMP2_NOT(!=, f32); break;
    case PN_OPCODE_FCMP_UGE_DOUBLE: OPCODE_CMP2_NOT(<, f64); break;
    case PN_OPCODE_FCMP_UGE_FLOAT:  OPCODE_CMP2_NOT(<, f32); break;
    case PN_OPCODE_FCMP_UGT_DOUBLE: OPCODE_CMP2_NOT(<=, f64); break;
    case PN_OPCODE_FCMP_UGT_FLOAT:  OPCODE_CMP2_NOT(<=, f32); break;
    case PN_OPCODE_FCMP_ULE_DOUBLE: OPCODE_CMP2_NOT(>, f64); break;
    case PN_OPCODE_FCMP_ULE_FLOAT:  OPCODE_CMP2_NOT(>, f32); break;
    case PN_OPCODE_FCMP_ULT_DOUBLE: OPCODE_CMP2_NOT(>=, f64); break;
    case PN_OPCODE_FCMP_ULT_FLOAT:  OPCODE_CMP2_NOT(>=, f32); break;
    case PN_OPCODE_FCMP_UNE_DOUBLE: OPCODE_CMP2_NOT(==, f64); break;
    case PN_OPCODE_FCMP_UNE_FLOAT:  OPCODE_CMP2_NOT(==, f32); break;
    case PN_OPCODE_FCMP_UNO_DOUBLE: OPCODE_CMP2_UNO(f64); break;
    case PN_OPCODE_FCMP_UNO_FLOAT:  OPCODE_CMP2_UNO(f32); break;


    case PN_OPCODE_FORWARDTYPEREF:
      location->instruction_id++;
      break;

    case PN_OPCODE_ICMP_EQ_INT8:   OPCODE_CMP2(==, u8); break;
    case PN_OPCODE_ICMP_EQ_INT16:  OPCODE_CMP2(==, u16); break;
    case PN_OPCODE_ICMP_EQ_INT32:  OPCODE_CMP2(==, u32); break;
    case PN_OPCODE_ICMP_EQ_INT64:  OPCODE_CMP2(==, u64); break;
    case PN_OPCODE_ICMP_NE_INT8:   OPCODE_CMP2(!=, u8); break;
    case PN_OPCODE_ICMP_NE_INT16:  OPCODE_CMP2(!=, u16); break;
    case PN_OPCODE_ICMP_NE_INT32:  OPCODE_CMP2(!=, u32); break;
    case PN_OPCODE_ICMP_NE_INT64:  OPCODE_CMP2(!=, u64); break;
    case PN_OPCODE_ICMP_SGE_INT8:  OPCODE_CMP2(>=, i8); break;
    case PN_OPCODE_ICMP_SGE_INT16: OPCODE_CMP2(>=, i16); break;
    case PN_OPCODE_ICMP_SGE_INT32: OPCODE_CMP2(>=, i32); break;
    case PN_OPCODE_ICMP_SGE_INT64: OPCODE_CMP2(>=, i64); break;
    case PN_OPCODE_ICMP_SGT_INT8:  OPCODE_CMP2(>, i8); break;
    case PN_OPCODE_ICMP_SGT_INT16: OPCODE_CMP2(>, i16); break;
    case PN_OPCODE_ICMP_SGT_INT32: OPCODE_CMP2(>, i32); break;
    case PN_OPCODE_ICMP_SGT_INT64: OPCODE_CMP2(>, i64); break;
    case PN_OPCODE_ICMP_SLE_INT8:  OPCODE_CMP2(<=, i8); break;
    case PN_OPCODE_ICMP_SLE_INT16: OPCODE_CMP2(<=, i16); break;
    case PN_OPCODE_ICMP_SLE_INT32: OPCODE_CMP2(<=, i32); break;
    case PN_OPCODE_ICMP_SLE_INT64: OPCODE_CMP2(<=, i64); break;
    case PN_OPCODE_ICMP_SLT_INT8:  OPCODE_CMP2(<, i8); break;
    case PN_OPCODE_ICMP_SLT_INT16: OPCODE_CMP2(<, i16); break;
    case PN_OPCODE_ICMP_SLT_INT32: OPCODE_CMP2(<, i32); break;
    case PN_OPCODE_ICMP_SLT_INT64: OPCODE_CMP2(<, i64); break;
    case PN_OPCODE_ICMP_UGE_INT8:  OPCODE_CMP2(>=, u8); break;
    case PN_OPCODE_ICMP_UGE_INT16: OPCODE_CMP2(>=, u16); break;
    case PN_OPCODE_ICMP_UGE_INT32: OPCODE_CMP2(>=, u32); break;
    case PN_OPCODE_ICMP_UGE_INT64: OPCODE_CMP2(>=, u64); break;
    case PN_OPCODE_ICMP_UGT_INT8:  OPCODE_CMP2(>, u8); break;
    case PN_OPCODE_ICMP_UGT_INT16: OPCODE_CMP2(>, u16); break;
    case PN_OPCODE_ICMP_UGT_INT32: OPCODE_CMP2(>, u32); break;
    case PN_OPCODE_ICMP_UGT_INT64: OPCODE_CMP2(>, u64); break;
    case PN_OPCODE_ICMP_ULE_INT8:  OPCODE_CMP2(<=, u8); break;
    case PN_OPCODE_ICMP_ULE_INT16: OPCODE_CMP2(<=, u16); break;
    case PN_OPCODE_ICMP_ULE_INT32: OPCODE_CMP2(<=, u32); break;
    case PN_OPCODE_ICMP_ULE_INT64: OPCODE_CMP2(<=, u64); break;
    case PN_OPCODE_ICMP_ULT_INT8:  OPCODE_CMP2(<, u8); break;
    case PN_OPCODE_ICMP_ULT_INT16: OPCODE_CMP2(<, u16); break;
    case PN_OPCODE_ICMP_ULT_INT32: OPCODE_CMP2(<, u32); break;
    case PN_OPCODE_ICMP_ULT_INT64: OPCODE_CMP2(<, u64); break;

#undef OPCODE_CMP2
#undef OPCODE_CMP2_NOT
#undef OPCODE_CMP2_ORD
#undef OPCODE_CMP2_UNO

    case PN_OPCODE_INTRINSIC_LLVM_MEMCPY: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PN_CHECK(i->num_args == 5);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t dst_p = pn_thread_get_value(thread, i->arg_ids[0]).u32;
      uint32_t src_p = pn_thread_get_value(thread, i->arg_ids[1]).u32;
      uint32_t len = pn_thread_get_value(thread, i->arg_ids[2]).u32;
      uint32_t align = pn_thread_get_value(thread, i->arg_ids[3]).u32;
      uint8_t is_volatile = pn_thread_get_value(thread, i->arg_ids[4]).u8;
      PN_TRACE(INTRINSICS,
               "    llvm.memcpy(dst_p:%u, src_p:%u, len:%u, align:%u, "
               "is_volatile:%u)\n",
               dst_p, src_p, len, align, is_volatile);
      PN_TRACE(EXECUTE,
               "    %%%d = %u  %%%d = %u  %%%d = %u  %%%d = %u  %%%d = %u\n",
               i->arg_ids[0], dst_p, i->arg_ids[1], src_p, i->arg_ids[2], len,
               i->arg_ids[3], align, i->arg_ids[4], is_volatile);
      (void)align;
      (void)is_volatile;

      if (len > 0) {
        pn_memory_check(executor->memory, dst_p, len);
        pn_memory_check(executor->memory, src_p, len);
        void* dst_pointer = executor->memory->data + dst_p;
        void* src_pointer = executor->memory->data + src_p;
        memcpy(dst_pointer, src_pointer, len);
      }
      location->instruction_id++;
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_MEMSET: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PN_CHECK(i->num_args == 5);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t dst_p = pn_thread_get_value(thread, i->arg_ids[0]).u32;
      uint8_t value = pn_thread_get_value(thread, i->arg_ids[1]).u8;
      uint32_t len = pn_thread_get_value(thread, i->arg_ids[2]).u32;
      uint32_t align = pn_thread_get_value(thread, i->arg_ids[3]).u32;
      uint8_t is_volatile = pn_thread_get_value(thread, i->arg_ids[4]).u8;
      PN_TRACE(INTRINSICS,
               "    llvm.memset(dst_p:%u, value:%u, len:%u, align:%u, "
               "is_volatile:%u)\n",
               dst_p, value, len, align, is_volatile);
      PN_TRACE(EXECUTE,
               "    %%%d = %u  %%%d = %u  %%%d = %u  %%%d = %u  %%%d = %u\n",
               i->arg_ids[0], dst_p, i->arg_ids[1], value, i->arg_ids[2], len,
               i->arg_ids[3], align, i->arg_ids[4], is_volatile);
      (void)align;
      (void)is_volatile;

      if (len > 0) {
        pn_memory_check(executor->memory, dst_p, len);
        void* dst_pointer = executor->memory->data + dst_p;
        memset(dst_pointer, value, len);
      }
      location->instruction_id++;
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_MEMMOVE: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PN_CHECK(i->num_args == 5);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t dst_p = pn_thread_get_value(thread, i->arg_ids[0]).u32;
      uint32_t src_p = pn_thread_get_value(thread, i->arg_ids[1]).u32;
      uint32_t len = pn_thread_get_value(thread, i->arg_ids[2]).u32;
      uint32_t align = pn_thread_get_value(thread, i->arg_ids[3]).u32;
      uint8_t is_volatile = pn_thread_get_value(thread, i->arg_ids[4]).u8;
      PN_TRACE(INTRINSICS,
               "    llvm.memmove(dst_p:%u, src_p:%u, len:%u, align:%u, "
               "is_volatile:%u)\n",
               dst_p, src_p, len, align, is_volatile);
      PN_TRACE(EXECUTE,
               "    %%%d = %u  %%%d = %u  %%%d = %u  %%%d = %u  %%%d = %u\n",
               i->arg_ids[0], dst_p, i->arg_ids[1], src_p, i->arg_ids[2], len,
               i->arg_ids[3], align, i->arg_ids[4], is_volatile);
      (void)align;
      (void)is_volatile;

      if (len > 0) {
        pn_memory_check(executor->memory, dst_p, len);
        pn_memory_check(executor->memory, src_p, len);
        void* dst_pointer = executor->memory->data + dst_p;
        void* src_pointer = executor->memory->data + src_p;
        memmove(dst_pointer, src_pointer, len);
      }
      location->instruction_id++;
      break;
    }

#define OPCODE_INTRINSIC_CMPXCHG(ty)                                          \
  do {                                                                        \
    PNInstructionCall* i = (PNInstructionCall*)inst;                          \
    PN_CHECK(i->num_args == 5);                                               \
    uint32_t addr_p = pn_thread_get_value(thread, i->arg_ids[0]).u32;         \
    pn_##ty expected = pn_thread_get_value(thread, i->arg_ids[1]).ty;         \
    pn_##ty desired = pn_thread_get_value(thread, i->arg_ids[2]).ty;          \
    uint32_t memory_order_success =                                           \
        pn_thread_get_value(thread, i->arg_ids[3]).u32;                       \
    uint32_t memory_order_failure =                                           \
        pn_thread_get_value(thread, i->arg_ids[4]).u32;                       \
    pn_##ty read = pn_memory_read_##ty(executor->memory, addr_p);             \
    PNRuntimeValue result = pn_executor_value_##ty(read);                     \
    if (read == expected) {                                                   \
      pn_memory_write_##ty(executor->memory, addr_p, desired);                \
    }                                                                         \
    pn_thread_set_value(thread, i->result_value_id, result);                  \
    PN_TRACE(INTRINSICS, "    llvm.nacl.atomic.cmpxchg." #ty                  \
                         "(addr_p:%u, expected:" PN_FORMAT_##ty               \
             ", desired:" PN_FORMAT_##ty ", ...)\n",                          \
             addr_p, expected, desired);                                      \
    PN_TRACE(EXECUTE, "    %%%d = " PN_FORMAT_##ty                            \
             "  %%%d = %u  %%%d = " PN_FORMAT_##ty "  %%%d = " PN_FORMAT_##ty \
             " %%%d = %u  %%%d = %u\n",                                       \
             i->result_value_id, result.ty, i->arg_ids[0], addr_p,            \
             i->arg_ids[1], expected, i->arg_ids[2], desired, i->arg_ids[3],  \
             memory_order_success, i->arg_ids[4], memory_order_failure);      \
    (void) memory_order_success;                                              \
    (void) memory_order_failure;                                              \
    location->instruction_id++;                                               \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I8:
      OPCODE_INTRINSIC_CMPXCHG(u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I16:
      OPCODE_INTRINSIC_CMPXCHG(u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I32:
      OPCODE_INTRINSIC_CMPXCHG(u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_CMPXCHG_I64:
      OPCODE_INTRINSIC_CMPXCHG(u64);
      break;

#undef OPCODE_INTRINSIC_CMPXCHG

#define OPCODE_INTRINSIC_LOAD(ty)                                              \
  do {                                                                         \
    PNInstructionCall* i = (PNInstructionCall*)inst;                           \
    PN_CHECK(i->num_args == 2);                                                \
    uint32_t addr_p = pn_thread_get_value(thread, i->arg_ids[0]).u32;          \
    uint32_t flags = pn_thread_get_value(thread, i->arg_ids[1]).u32;           \
    pn_##ty value = pn_memory_read_##ty(executor->memory, addr_p);             \
    PNRuntimeValue result = pn_executor_value_##ty(value);                     \
    pn_thread_set_value(thread, i->result_value_id, result);                   \
    PN_TRACE(INTRINSICS,                                                       \
             "    llvm.nacl.atomic.load." #ty "(addr_p:%u, flags:%u)\n",       \
             addr_p, flags);                                                   \
    PN_TRACE(EXECUTE, "    %%%d = " PN_FORMAT_##ty "  %%%d = %u  %%%d = %u\n", \
             i->result_value_id, result.ty, i->arg_ids[0], addr_p,             \
             i->arg_ids[1], flags);                                            \
    (void) flags;                                                              \
    location->instruction_id++;                                                \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I8:
      OPCODE_INTRINSIC_LOAD(u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I16:
      OPCODE_INTRINSIC_LOAD(u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I32:
      OPCODE_INTRINSIC_LOAD(u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_LOAD_I64:
      OPCODE_INTRINSIC_LOAD(u64);
      break;

#undef OPCODE_INTRINSIC_LOAD

#define OPCODE_INTRINSIC_RMW(opval, op, ty)                                    \
  do {                                                                         \
    PNInstructionCall* i = (PNInstructionCall*)inst;                           \
    PN_CHECK(i->num_args == 4);                                                \
    PN_CHECK(pn_thread_get_value(thread, i->arg_ids[0]).u32 == opval);         \
    uint32_t addr_p = pn_thread_get_value(thread, i->arg_ids[1]).u32;          \
    pn_##ty value = pn_thread_get_value(thread, i->arg_ids[2]).ty;             \
    uint32_t memory_order = pn_thread_get_value(thread, i->arg_ids[3]).u32;    \
    pn_##ty old_value = pn_memory_read_##ty(executor->memory, addr_p);         \
    pn_##ty new_value = old_value op value;                                    \
    pn_memory_write_##ty(executor->memory, addr_p, new_value);                 \
    PNRuntimeValue result = pn_executor_value_u32(old_value);                  \
    pn_thread_set_value(thread, i->result_value_id, result);                   \
    PN_TRACE(INTRINSICS, "    llvm.nacl.atomic.rmw." #ty                       \
                         "(op: %s, addr_p:%u, value: " PN_FORMAT_##ty ")\n",   \
             #op, addr_p, value);                                              \
    PN_TRACE(EXECUTE, "    %%%d = " PN_FORMAT_##ty                             \
             "  %%%d = %u  %%%d = %u  %%%d = " PN_FORMAT_##ty "  %%%d = %u\n", \
             i->result_value_id, result.ty, i->arg_ids[0], opval,              \
             i->arg_ids[1], addr_p, i->arg_ids[2], value, i->arg_ids[3],       \
             memory_order);                                                    \
    (void) memory_order;                                                       \
    location->instruction_id++;                                                \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I8:
      OPCODE_INTRINSIC_RMW(1, +, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I16:
      OPCODE_INTRINSIC_RMW(1, +, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I32:
      OPCODE_INTRINSIC_RMW(1, +, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_ADD_I64:
      OPCODE_INTRINSIC_RMW(1, +, u64);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I8:
      OPCODE_INTRINSIC_RMW(2, -, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I16:
      OPCODE_INTRINSIC_RMW(2, -, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I32:
      OPCODE_INTRINSIC_RMW(2, -, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_SUB_I64:
      OPCODE_INTRINSIC_RMW(2, -, u64);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I8:
      OPCODE_INTRINSIC_RMW(3, &, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I16:
      OPCODE_INTRINSIC_RMW(3, &, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I32:
      OPCODE_INTRINSIC_RMW(3, &, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_AND_I64:
      OPCODE_INTRINSIC_RMW(3, &, u64);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I8:
      OPCODE_INTRINSIC_RMW(4, |, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I16:
      OPCODE_INTRINSIC_RMW(4, |, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I32:
      OPCODE_INTRINSIC_RMW(4, |, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_OR_I64:
      OPCODE_INTRINSIC_RMW(4, |, u64);
      break;

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I8:
      OPCODE_INTRINSIC_RMW(5, ^, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I16:
      OPCODE_INTRINSIC_RMW(5, ^, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I32:
      OPCODE_INTRINSIC_RMW(5, ^, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_XOR_I64:
      OPCODE_INTRINSIC_RMW(5, ^, u64);
      break;

#define OPCODE_INTRINSIC_EXCHANGE(opval, ty)                                   \
  do {                                                                         \
    PNInstructionCall* i = (PNInstructionCall*)inst;                           \
    PN_CHECK(i->num_args == 4);                                                \
    PN_CHECK(pn_thread_get_value(thread, i->arg_ids[0]).u32 == opval);         \
    uint32_t addr_p = pn_thread_get_value(thread, i->arg_ids[1]).u32;          \
    pn_##ty value = pn_thread_get_value(thread, i->arg_ids[2]).ty;             \
    uint32_t memory_order = pn_thread_get_value(thread, i->arg_ids[3]).u32;    \
    pn_##ty old_value = pn_memory_read_##ty(executor->memory, addr_p);         \
    pn_##ty new_value = value;                                                 \
    pn_memory_write_##ty(executor->memory, addr_p, new_value);                 \
    PNRuntimeValue result = pn_executor_value_u32(old_value);                  \
    pn_thread_set_value(thread, i->result_value_id, result);                   \
    PN_TRACE(INTRINSICS, "    llvm.nacl.atomic.exchange." #ty                  \
                         "(addr_p:%u, value: " PN_FORMAT_##ty ")\n",           \
             addr_p, value);                                                   \
    PN_TRACE(EXECUTE, "    %%%d = " PN_FORMAT_##ty                             \
             "  %%%d = %u  %%%d = %u  %%%d = " PN_FORMAT_##ty "  %%%d = %u\n", \
             i->result_value_id, result.ty, i->arg_ids[0], opval,              \
             i->arg_ids[1], addr_p, i->arg_ids[2], value, i->arg_ids[3],       \
             memory_order);                                                    \
    (void) memory_order;                                                       \
    location->instruction_id++;                                                \
  } while (0) /* no semicolon */

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I8:
      OPCODE_INTRINSIC_EXCHANGE(6, u8);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I16:
      OPCODE_INTRINSIC_EXCHANGE(6, u16);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I32:
      OPCODE_INTRINSIC_EXCHANGE(6, u32);
      break;
    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_EXCHANGE_I64:
      OPCODE_INTRINSIC_EXCHANGE(6, u64);
      break;

#undef OPCODE_INTRINSIC_RMW
#undef OPCODE_INTRINSIC_EXCHANGE

    case PN_OPCODE_INTRINSIC_LLVM_NACL_LONGJMP: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PN_CHECK(i->num_args == 2);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t jmpbuf_p = pn_thread_get_value(thread, i->arg_ids[0]).u32;
      PNRuntimeValue value = pn_thread_get_value(thread, i->arg_ids[1]);
      PN_TRACE(INTRINSICS, "    llvm.nacl.longjmp(jmpbuf: %u, value: %u)\n",
               jmpbuf_p, value.u32);
      PN_TRACE(EXECUTE, "    %%%d = %u  %%%d = %u\n", i->arg_ids[0], jmpbuf_p,
               i->arg_ids[1], value.u32);

      PNJmpBufId id = pn_memory_read_u32(executor->memory, jmpbuf_p);

      /* Search the call stack for the matching jmpbuf id */
      PNCallFrame* f = frame;
      while (f != &executor->sentinel_frame) {
        PNJmpBuf* buf = f->jmpbuf_head;
        while (buf) {
          if (buf->id == id) {
            /* Found it */
            thread->current_call_frame = f;
            pn_allocator_reset_to_mark(&thread->allocator, f->mark);
            /* Reset the frame to its original state */
            *thread->current_call_frame = buf->frame;
            location = &thread->current_call_frame->location;
            PN_TRACE(EXECUTE, "function = %d  bb = %d\n", location->function_id,
                     location->bb_id);
            /* Set the return value */
            PNFunction* new_function =
                &module->functions[location->function_id];
            PNBasicBlock* new_bb = &new_function->bbs[location->bb_id];
            PNInstructionCall* c =
                (PNInstructionCall*)
                    new_bb->instructions[location->instruction_id];
            pn_thread_set_value(thread, c->result_value_id, value);
            pn_executor_value_trace(executor, function, i->arg_ids[1], value,
                                    "    ", "\n");
            location->instruction_id++;
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
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PN_CHECK(i->num_args == 1);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      uint32_t jmpbuf_p = pn_thread_get_value(thread, i->arg_ids[0]).u32;
      PNJmpBuf* buf = pn_allocator_alloc(&thread->allocator, sizeof(PNJmpBuf),
                                         PN_DEFAULT_ALIGN);
      buf->id = executor->next_jmpbuf_id++;
      buf->frame = *frame;
      buf->next = frame->jmpbuf_head;
      frame->jmpbuf_head = buf;
      pn_memory_write_u32(executor->memory, jmpbuf_p, buf->id);
      PNRuntimeValue result = pn_executor_value_u32(0);
      pn_thread_set_value(thread, i->result_value_id, result);
      PN_TRACE(INTRINSICS, "    llvm.nacl.setjmp(jmpbuf: %u)\n", jmpbuf_p);
      PN_TRACE(EXECUTE, "    %%%d = %u  %%%d = %u\n", i->result_value_id,
               result.u32, i->arg_ids[0], jmpbuf_p);
      location->instruction_id++;
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_STORE_I32: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PN_CHECK(i->num_args == 3);
      PN_CHECK(i->result_value_id == PN_INVALID_VALUE_ID);
      uint32_t value = pn_thread_get_value(thread, i->arg_ids[0]).u32;
      uint32_t addr_p = pn_thread_get_value(thread, i->arg_ids[1]).u32;
      uint32_t flags = pn_thread_get_value(thread, i->arg_ids[2]).u32;
      pn_memory_write_u32(executor->memory, addr_p, value);
      PN_TRACE(
          INTRINSICS,
          "    llvm.nacl.atomic.store.u32(value: %u, addr_p:%u, flags: %u)\n",
          value, addr_p, flags);
      PN_TRACE(EXECUTE, "    %%%d = %u  %%%d = %u  %%%d = %u\n", i->arg_ids[0],
               value, i->arg_ids[1], addr_p, i->arg_ids[2], flags);
      (void)flags;
      location->instruction_id++;
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_NACL_READ_TP: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PN_CHECK(i->num_args == 0);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      PNRuntimeValue result = pn_executor_value_u32(thread->tls);
      pn_thread_set_value(thread, i->result_value_id, result);
      PN_TRACE(INTRINSICS, "    llvm.nacl.read.tp()\n");
      PN_TRACE(EXECUTE, "    %%%d = %u\n", i->result_value_id, result.u32);
      location->instruction_id++;
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_SQRT_F32: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PN_CHECK(i->num_args == 1);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      float value = pn_thread_get_value(thread, i->arg_ids[0]).f32;
      PNRuntimeValue result = pn_executor_value_f32(sqrtf(value));
      pn_thread_set_value(thread, i->result_value_id, result);
      PN_TRACE(INTRINSICS, "    llvm.sqrt.f32(%f)\n", value);
      PN_TRACE(EXECUTE, "    %%%d = %f  %%%d = %f\n", i->result_value_id,
               result.f32, i->arg_ids[0], value);
      location->instruction_id++;
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_SQRT_F64: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PN_CHECK(i->num_args == 1);
      PN_CHECK(i->result_value_id != PN_INVALID_VALUE_ID);
      double value = pn_thread_get_value(thread, i->arg_ids[0]).f64;
      PNRuntimeValue result = pn_executor_value_f64(sqrt(value));
      pn_thread_set_value(thread, i->result_value_id, result);
      PN_TRACE(INTRINSICS, "    llvm.sqrt.f64(%f)\n", value);
      PN_TRACE(EXECUTE, "    %%%d = %f  %%%d = %f\n", i->result_value_id,
               result.f64, i->arg_ids[0], value);
      location->instruction_id++;
      break;
    }

    case PN_OPCODE_INTRINSIC_LLVM_TRAP: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PN_CHECK(i->num_args == 0);
      PN_TRACE(INTRINSICS, "    llvm.trap()\n");
      executor->exit_code = -1;
      executor->exiting = PN_TRUE;
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
    PN_OPCODE_INTRINSIC_STUB(LLVM_STACKRESTORE)
    PN_OPCODE_INTRINSIC_STUB(LLVM_STACKSAVE)
    PN_OPCODE_INTRINSIC_STUB(START)

#define OPCODE_LOAD(ty)                                             \
  do {                                                              \
    PNInstructionLoad* i = (PNInstructionLoad*)inst;                \
    PNRuntimeValue src = pn_thread_get_value(thread, i->src_id);    \
    PNRuntimeValue result = pn_executor_value_##ty(                 \
        pn_memory_read_##ty(executor->memory, src.u32));            \
    pn_thread_set_value(thread, i->result_value_id, result);        \
    PN_TRACE(EXECUTE, "    %%%d = " PN_FORMAT_##ty "  %%%d = %u\n", \
             i->result_value_id, result.ty, i->src_id, src.u32);    \
    location->instruction_id++;                                     \
  } while (0) /*no semicolon */

    case PN_OPCODE_LOAD_DOUBLE: OPCODE_LOAD(f64); break;
    case PN_OPCODE_LOAD_FLOAT: OPCODE_LOAD(f32); break;
    case PN_OPCODE_LOAD_INT8: OPCODE_LOAD(u8); break;
    case PN_OPCODE_LOAD_INT16: OPCODE_LOAD(u16); break;
    case PN_OPCODE_LOAD_INT32: OPCODE_LOAD(u32); break;
    case PN_OPCODE_LOAD_INT64: OPCODE_LOAD(u64); break;

#undef OPCODE_LOAD

    case PN_OPCODE_PHI:
      location->instruction_id++;
      break;

    case PN_OPCODE_RET: {
      thread->current_call_frame = frame->parent;
      location = &frame->parent->location;
      pn_allocator_reset_to_mark(&thread->allocator, frame->parent->mark);
      PN_TRACE(EXECUTE, "function = %d  bb = %d\n", location->function_id,
               location->bb_id);
      location->instruction_id++;
      break;
    }

    case PN_OPCODE_RET_VALUE: {
      PNInstructionRet* i = (PNInstructionRet*)inst;
      PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);

      thread->current_call_frame = frame->parent;
      location = &frame->parent->location;

      if (location->function_id != PN_INVALID_FUNCTION_ID) {
        PNFunction* new_function = &module->functions[location->function_id];
        PNBasicBlock* new_bb = &new_function->bbs[location->bb_id];
        PNInstructionCall* c =
            (PNInstructionCall*)new_bb->instructions[location->instruction_id];
        pn_thread_set_value(thread, c->result_value_id, value);
        pn_executor_value_trace(executor, function, i->value_id, value, "    ",
                                "\n");
        pn_allocator_reset_to_mark(&thread->allocator, frame->parent->mark);
        PN_TRACE(EXECUTE, "function = %d  bb = %d\n", location->function_id,
                 location->bb_id);
        pn_executor_value_trace(executor, new_function, c->result_value_id,
                                value, "    ", "\n");
      } else {
        /* Returning a value from _start; let's consider that the exit code */
        executor->exit_code = value.i32;
        pn_executor_value_trace(executor, function, i->value_id, value, "    ",
                                "\n");
        PN_TRACE(EXECUTE, "function = %d  bb = %d\n", location->function_id,
                 location->bb_id);
      }

      location->instruction_id++;
      break;
    }

#define OPCODE_STORE(ty)                                             \
  do {                                                               \
    PNInstructionStore* i = (PNInstructionStore*)inst;               \
    PNRuntimeValue dest = pn_thread_get_value(thread, i->dest_id);   \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id); \
    PN_TRACE(EXECUTE, "    %%%d = %u  %%%d = " PN_FORMAT_##ty "\n",  \
             i->dest_id, dest.u32, i->value_id, value.ty);           \
    pn_memory_write_##ty(executor->memory, dest.u32, value.ty);      \
    location->instruction_id++;                                      \
  } while (0) /*no semicolon */

    case PN_OPCODE_STORE_DOUBLE: OPCODE_STORE(f64); break;
    case PN_OPCODE_STORE_FLOAT: OPCODE_STORE(f32); break;
    case PN_OPCODE_STORE_INT8: OPCODE_STORE(u8); break;
    case PN_OPCODE_STORE_INT16: OPCODE_STORE(u16); break;
    case PN_OPCODE_STORE_INT32: OPCODE_STORE(u32); break;
    case PN_OPCODE_STORE_INT64: OPCODE_STORE(u64); break;

#undef OPCODE_STORE

#define OPCODE_SWITCH(ty)                                             \
  do {                                                                \
    PNInstructionSwitch* i = (PNInstructionSwitch*)inst;              \
    PNRuntimeValue value = pn_thread_get_value(thread, i->value_id);  \
    PNBasicBlockId new_bb_id = i->default_bb_id;                      \
    uint32_t c;                                                       \
    for (c = 0; c < i->num_cases; ++c) {                              \
      PNSwitchCase* switch_case = &i->cases[c];                       \
      if (value.ty == switch_case->value) {                           \
        new_bb_id = switch_case->bb_id;                               \
        break;                                                        \
      }                                                               \
    }                                                                 \
    pn_thread_do_phi_assigns(thread, &function->bbs[location->bb_id], \
                             new_bb_id);                              \
    location->bb_id = new_bb_id;                                      \
    location->instruction_id = 0;                                     \
    PN_TRACE(EXECUTE, "    %%%d = " PN_FORMAT_##ty "\n", i->value_id, \
             value.ty);                                               \
    PN_TRACE(EXECUTE, "bb = %d\n", new_bb_id);                        \
  } while (0) /* no semicolon */

    case PN_OPCODE_SWITCH_INT1:
    case PN_OPCODE_SWITCH_INT8: OPCODE_SWITCH(i8); break;
    case PN_OPCODE_SWITCH_INT16: OPCODE_SWITCH(i16); break;
    case PN_OPCODE_SWITCH_INT32: OPCODE_SWITCH(i32); break;
    case PN_OPCODE_SWITCH_INT64: OPCODE_SWITCH(i64); break;

#undef OPCODE_SWITCH

    case PN_OPCODE_UNREACHABLE:
      PN_FATAL("Reached unreachable instruction!\n");
      location->instruction_id++;
      break;

    case PN_OPCODE_VSELECT: {
      PNInstructionVselect* i = (PNInstructionVselect*)inst;
      PNRuntimeValue cond = pn_thread_get_value(thread, i->cond_id);
      PNValueId value_id = (cond.u8 & 1) ? i->true_value_id : i->false_value_id;
      PNRuntimeValue result = pn_thread_get_value(thread, value_id);
      pn_thread_set_value(thread, i->result_value_id, result);
      pn_executor_value_trace(executor, function, i->result_value_id, result,
                              "    ", "  ");
      pn_executor_value_trace(executor, function, i->cond_id, cond, "", "  ");
      pn_executor_value_trace(executor, function, value_id, result, "", "\n");
      location->instruction_id++;
      break;
    }

    default:
      PN_FATAL("Invalid opcode: %d\n", inst->opcode);
      break;
  }
}

/* Option parsing, environment variables */

enum {
  PN_FLAG_VERBOSE,
  PN_FLAG_HELP,
  PN_FLAG_MEMORY_SIZE,
  PN_FLAG_RUN,
  PN_FLAG_ENV,
  PN_FLAG_USE_HOST_ENV,
#if PN_TRACING
  PN_FLAG_TRACE_ALL,
  PN_FLAG_TRACE_BLOCK,
  PN_FLAG_TRACE_BCDIS,
#define PN_TRACE_FLAGS(name, flag) PN_FLAG_TRACE_##name,
  PN_FOREACH_TRACE(PN_TRACE_FLAGS)
#undef PN_TRACE_FLAGS
  PN_FLAG_TRACE_FUNCTION_FILTER,
#endif /* PN_TRACING */
  PN_FLAG_PRINT_ALL,
  PN_FLAG_PRINT_NAMED_FUNCTIONS,
#if PN_TIMERS
  PN_FLAG_PRINT_TIME,
#endif /* PN_TIMERS */
  PN_FLAG_PRINT_OPCODE_COUNTS,
  PN_FLAG_PRINT_STATS,
  PN_NUM_FLAGS
};

static struct option g_pn_long_options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {"memory-size", required_argument, NULL, 'm'},
    {"run", no_argument, NULL, 'r'},
    {"env", required_argument, NULL, 'e'},
    {"use-host-env", no_argument, NULL, 'E'},
#if PN_TRACING
    {"trace-all", no_argument, NULL, 't'},
    {"trace-block", no_argument, NULL, 0},
    {"trace-bcdis", no_argument, NULL, 0},
#define PN_TRACE_FLAGS(name, flag)        \
    { "trace-" flag, no_argument, NULL, 0 },
    PN_FOREACH_TRACE(PN_TRACE_FLAGS)
#undef PN_TRACE_FLAGS
    {"trace-function-filter", required_argument, NULL, 0},
#endif /* PN_TRACING */
    {"print-all", no_argument, NULL, 'p'},
    {"print-named-functions", no_argument, NULL, 0},
#if PN_TIMERS
    {"print-time", no_argument, NULL, 0},
#endif /* PN_TIMERS */
    {"print-opcode-counts", no_argument, NULL, 0},
    {"print-stats", no_argument, NULL, 0},
    {NULL, 0, NULL, 0},
};

PN_STATIC_ASSERT(PN_NUM_FLAGS + 1 == PN_ARRAY_SIZE(g_pn_long_options));

typedef struct PNOptionHelp {
  int flag;
  const char* metavar;
  const char* help;
} PNOptionHelp;

static PNOptionHelp g_pn_option_help[] = {
    {PN_FLAG_MEMORY_SIZE, "SIZE",
     "size of runtime memory. suffixes k=1024, m=1024*1024"},
    {PN_FLAG_ENV, "KEY=VALUE", "set runtime environment variable KEY to VALUE"},
#if PN_TRACING
    {PN_FLAG_TRACE_FUNCTION_FILTER, "NAME",
     "only trace function with given name or id"},
#endif /* PN_TRACING */
    {PN_NUM_FLAGS, NULL},
};

static void pn_usage(const char* prog) {
  PN_PRINT("usage: %s [option] filename\n", prog);
  PN_PRINT("options:\n");
  struct option* opt = &g_pn_long_options[0];
  int i = 0;
  for (; opt->name; ++i, ++opt) {
    PNOptionHelp* help = NULL;

    int n = 0;
    while (g_pn_option_help[n].help) {
      if (i == g_pn_option_help[n].flag) {
        help = &g_pn_option_help[n];
        break;
      }
      n++;
    }

    if (opt->val) {
      PN_PRINT("  -%c, ", opt->val);
    } else {
      PN_PRINT("      ");
    }

    if (help && help->metavar) {
      char buf[100];
      snprintf(buf, 100, "%s=%s", opt->name, help->metavar);
      PN_PRINT("--%-30s", buf);
    } else {
      PN_PRINT("--%-30s", opt->name);
    }

    if (help) {
      PN_PRINT("%s", help->help);
    }

    PN_PRINT("\n");
  }
  exit(0);
}

static char** pn_environ_copy(char** environ) {
  char** env;
  int num_keys = 0;
  for (env = environ; *env; ++env) {
    num_keys++;
  }

  char** result = pn_calloc(num_keys + 1, sizeof(char*));
  int i = 0;
  for (env = environ; *env; ++env, ++i) {
    result[i] = pn_strdup(*env);
  }

  return result;
}

static void pn_environ_free(char** environ) {
  char** env;
  for (env = environ; *env; ++env) {
    pn_free(*env);
  }
  pn_free(environ);
}

static void pn_environ_put(char*** environ, char* value) {
  char* equals = strchr(value, '=');
  int count = equals ? equals - value : strlen(value);
  PNBool remove = equals == NULL;

  int num_keys = 0;
  if (*environ) {
    char** env;
    for (env = *environ; *env; ++env, ++num_keys) {
      if (strncasecmp(value, *env, count) != 0) {
        continue;
      }

      pn_free(*env);
      if (remove) {
        for (; *env; ++env) {
          *env = *(env + 1);
        }
      } else {
        *env = pn_strdup(value);
      }
      return;
    }
  }

  if (!remove) {
    *environ = pn_realloc(*environ, (num_keys + 2) * sizeof(char*));
    (*environ)[num_keys] = pn_strdup(value);
    (*environ)[num_keys + 1] = 0;
  }
}

static void pn_environ_put_all(char*** environ, char** environ_to_put) {
  char** env;
  for (env = environ_to_put; *env; ++env) {
    pn_environ_put(environ, *env);
  }
}

static void pn_environ_print(char** environ) {
  if (environ) {
    char** env;
    for (env = environ; *env; ++env) {
      PN_PRINT("  %s\n", *env);
    }
  }
}

static void pn_options_parse(int argc, char** argv, char** env) {
  int c;
  int option_index;
  char** environ_copy = pn_environ_copy(env);

  while (1) {
    c = getopt_long(argc, argv, "vm:re:Ehtp", g_pn_long_options, &option_index);
    if (c == -1) {
      break;
    }

  redo_switch:
    switch (c) {
      case 0:
        c = g_pn_long_options[option_index].val;
        if (c) {
          goto redo_switch;
        }

        switch (option_index) {
          case PN_FLAG_VERBOSE:
          case PN_FLAG_HELP:
          case PN_FLAG_MEMORY_SIZE:
          case PN_FLAG_RUN:
          case PN_FLAG_ENV:
          case PN_FLAG_USE_HOST_ENV:
#if PN_TRACING
          case PN_FLAG_TRACE_ALL:
#endif /* PN_TRACING */
          case PN_FLAG_PRINT_ALL:
            /* Handled above by goto */
            PN_UNREACHABLE();

#if PN_TRACING
          case PN_FLAG_TRACE_BCDIS:
#define PN_TRACE_UNSET(name, flag) g_pn_trace_##name = PN_FALSE;
            PN_FOREACH_TRACE(PN_TRACE_UNSET)
#undef PN_TRACE_UNSET
            g_pn_trace_ABBREV = PN_TRUE;
            g_pn_trace_BLOCKINFO_BLOCK = PN_TRUE;
            g_pn_trace_TYPE_BLOCK = PN_TRUE;
            g_pn_trace_GLOBALVAR_BLOCK = PN_TRUE;
            g_pn_trace_VALUE_SYMTAB_BLOCK = PN_TRUE;
            g_pn_trace_CONSTANTS_BLOCK = PN_TRUE;
            g_pn_trace_FUNCTION_BLOCK = PN_TRUE;
            g_pn_trace_MODULE_BLOCK = PN_TRUE;
            g_pn_trace_INSTRUCTIONS = PN_TRUE;
            break;

          case PN_FLAG_TRACE_BLOCK:
            g_pn_trace_BLOCKINFO_BLOCK = PN_TRUE;
            g_pn_trace_TYPE_BLOCK = PN_TRUE;
            g_pn_trace_GLOBALVAR_BLOCK = PN_TRUE;
            g_pn_trace_VALUE_SYMTAB_BLOCK = PN_TRUE;
            g_pn_trace_CONSTANTS_BLOCK = PN_TRUE;
            g_pn_trace_FUNCTION_BLOCK = PN_TRUE;
            g_pn_trace_MODULE_BLOCK = PN_TRUE;
            break;

#define PN_TRACE_OPTIONS(name, flag) \
  case PN_FLAG_TRACE_##name:         \
    g_pn_trace_##name = PN_TRUE;     \
    break;
            PN_FOREACH_TRACE(PN_TRACE_OPTIONS);
#undef PN_TRACE_OPTIONS

          case PN_FLAG_TRACE_FUNCTION_FILTER:
            g_pn_trace_function_filter = optarg;
            break;

#endif /* PN_TRACING */

          case PN_FLAG_PRINT_NAMED_FUNCTIONS:
            g_pn_print_named_functions = PN_TRUE;
            break;

#if PN_TIMERS
          case PN_FLAG_PRINT_TIME:
            g_pn_print_time = PN_TRUE;
            break;
#endif /* PN_TIMERS */

          case PN_FLAG_PRINT_STATS:
            g_pn_print_stats = PN_TRUE;
            break;

          case PN_FLAG_PRINT_OPCODE_COUNTS:
            g_pn_print_opcode_counts = PN_TRUE;
            break;

            break;
        }
        break;

      case 'v':
        g_pn_verbose++;
        break;

      case 'm': {
        char* endptr;
        errno = 0;
        long int size = strtol(optarg, &endptr, 10);
        size_t optarg_len = strlen(optarg);

        if (errno != 0) {
          PN_FATAL("Unable to parse memory-size flag \"%s\".\n", optarg);
        }

        if (endptr == optarg + optarg_len - 1) {
          if (*endptr == 'k' || *endptr == 'K') {
            size *= 1024;
          } else if (*endptr == 'm' || *endptr == 'M') {
            size *= 1024 * 1024;
          } else {
            PN_FATAL("Unknown suffix on memory-size \"%s\".\n", optarg);
          }
        } else if (endptr == optarg + optarg_len) {
          /* Size in bytes, do nothing */
        } else {
          PN_FATAL("Unable to parse memory-size flag \"%s\".\n", optarg);
        }

        if (size < PN_MEMORY_GUARD_SIZE) {
          PN_FATAL(
              "Cannot set memory-size (%ld) smaller than guard size (%d).\n",
              size, PN_MEMORY_GUARD_SIZE);
        }

        size = pn_align_up(size, PN_PAGESIZE);
        PN_TRACE(FLAGS, "Setting memory-size to %ld\n", size);
        g_pn_memory_size = size;
        break;
      }

      case 'r':
        g_pn_run = PN_TRUE;
        break;

      case 'e':
        pn_environ_put(&g_pn_environ, optarg);
        break;

      case 'E':
        pn_environ_put_all(&g_pn_environ, environ_copy);
        break;

      case 't':
#if PN_TRACING
#define PN_TRACE_OPTIONS(name, flag) g_pn_trace_##name = PN_TRUE;
        PN_FOREACH_TRACE(PN_TRACE_OPTIONS);
#undef PN_TRACE_OPTIONS
#else
        PN_ERROR("PN_TRACING not enabled.\n");
#endif
        break;

      case 'p':
#if PN_TIMERS
        g_pn_print_time = PN_TRUE;
#endif /* PN_TIMERS */
        g_pn_print_stats = PN_TRUE;
        break;

      case 'h':
        pn_usage(argv[0]);

      case '?':
        break;

      default:
        PN_ERROR("getopt_long returned '%c' (%d)\n", c, c);
        break;
    }
  }

  if (optind < argc) {
    g_pn_filename = argv[optind];
  } else {
    PN_ERROR("No filename given.\n");
    pn_usage(argv[0]);
  }

  g_pn_argv = argv + optind;

#if PN_TRACING
  /* Handle flag dependencies */
  if (g_pn_trace_INSTRUCTIONS) {
    g_pn_trace_BASIC_BLOCKS = PN_TRUE;
  }

  if (g_pn_trace_BASIC_BLOCKS) {
    g_pn_trace_FUNCTION_BLOCK = PN_TRUE;
  }

  if (g_pn_trace_EXECUTE) {
    g_pn_trace_IRT = PN_TRUE;
    g_pn_trace_INTRINSICS = PN_TRUE;
  }
#endif /* PN_TRACING */

  pn_environ_free(environ_copy);
#if PN_TRACING
  if (g_pn_trace_FLAGS) {
    PN_PRINT("*** ARGS:\n");
    if (g_pn_argv) {
      char** p;
      int i = 0;
      for (p = g_pn_argv; *p; ++p, ++i) {
        PN_PRINT("  [%d] %s\n", i, *p);
      }
    }
    PN_PRINT("*** ENVIRONMENT:\n");
    pn_environ_print(g_pn_environ);
  }
#endif /* PN_TRACING */
}

static void pn_header_read(PNBitStream* bs) {
  const char sig[] = "PEXE";
  int i;
  for (i = 0; i < 4; ++i) {
    if (pn_bitstream_read(bs, 8) != sig[i]) {
      PN_FATAL("Expected '%c'\n", sig[i]);
    }
  }

  uint32_t num_fields = pn_bitstream_read(bs, 16);
  pn_bitstream_read(bs, 16); /* num_bytes */
  for (i = 0; i < num_fields; ++i) {
    uint32_t ftype = pn_bitstream_read(bs, 4);
    uint32_t id = pn_bitstream_read(bs, 4);
    if (id != 1) {
      PN_FATAL("bad header id: %d\n", id);
    }

    /* Align to u16 */
    pn_bitstream_read(bs, 8);
    uint32_t length = pn_bitstream_read(bs, 16);

    switch (ftype) {
      case 0:
        pn_bitstream_skip_bytes(bs, length);
        break;
      case 1:
        pn_bitstream_read(bs, 32);
        break;
      default:
        PN_FATAL("bad ftype %d\n", ftype);
    }
  }
}

static uint32_t pn_max_num_constants(PNModule* module) {
  uint32_t result = 0;
  uint32_t n;
  for (n = 0; n < module->num_functions; ++n) {
    if (module->functions[n].num_constants > result) {
      result = module->functions[n].num_constants;
    }
  }

  return result;
}

static uint32_t pn_max_num_values(PNModule* module) {
  uint32_t result = 0;
  uint32_t n;
  for (n = 0; n < module->num_functions; ++n) {
    if (module->functions[n].num_values > result) {
      result = module->functions[n].num_values;
    }
  }

  return result;
}

static uint32_t pn_max_num_bbs(PNModule* module) {
  uint32_t result = 0;
  uint32_t n;
  for (n = 0; n < module->num_functions; ++n) {
    if (module->functions[n].num_bbs > result) {
      result = module->functions[n].num_bbs;
    }
  }

  return result;
}

static const char* pn_human_readable_size_leaky(size_t size) {
  const size_t gig = 1024 * 1024 * 1024;
  const size_t meg = 1024 * 1024;
  const size_t kilo = 1024;
  char buffer[100];
  if (size >= gig) {
    snprintf(buffer, 100, "%.1fG", (size + gig - 1) / (double)gig);
  } else if (size >= meg) {
    snprintf(buffer, 100, "%.1fM", (size + meg - 1) / (double)meg);
  } else if (size >= kilo) {
    snprintf(buffer, 100, "%.1fK", (size + kilo - 1) / (double)kilo);
  } else {
    snprintf(buffer, 100, "%" PRIdPTR, size);
  }
  return pn_strdup(buffer);
}

static void pn_allocator_print_stats_leaky(PNAllocator* allocator) {
  PN_PRINT("%12s allocator: used: %7s frag: %7s\n", allocator->name,
           pn_human_readable_size_leaky(allocator->total_used),
           pn_human_readable_size_leaky(allocator->internal_fragmentation));
}

typedef struct PNOpcodeCountPair {
  PNOpcode opcode;
  uint32_t count;
} PNOpcodeCountPair;

static int pn_opcode_count_pair_compare(const void* a, const void* b) {
  return ((PNOpcodeCountPair*)b)->count - ((PNOpcodeCountPair*)a)->count;
}

int main(int argc, char** argv, char** envp) {
  PN_BEGIN_TIME(TOTAL);
  pn_options_parse(argc, argv, envp);
  PN_BEGIN_TIME(FILE_READ);
  FILE* f = fopen(g_pn_filename, "r");
  if (!f) {
    PN_FATAL("unable to read %s\n", g_pn_filename);
  }

  fseek(f, 0, SEEK_END);
  size_t fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  void* data = pn_malloc(fsize);

  size_t read_size = fread(data, 1, fsize, f);
  if (read_size != fsize) {
    PN_FATAL("unable to read data from file\n");
  }

  fclose(f);
  PN_END_TIME(FILE_READ);

  PNBitStream bs;
  pn_bitstream_init(&bs, data, fsize);
  pn_header_read(&bs);

  uint32_t entry = pn_bitstream_read(&bs, 2);
  if (entry != PN_ENTRY_SUBBLOCK) {
    PN_FATAL("expected subblock at top-level\n");
  }

  PNBlockId block_id = pn_bitstream_read_vbr(&bs, 8);
  PN_CHECK(block_id == PN_BLOCKID_MODULE);
  PN_TRACE(MODULE_BLOCK, "module {  // BlockID = %d\n", block_id);
  PN_TRACE_INDENT(MODULE_BLOCK, 2);

  PNMemory memory = {};
  memory.size = g_pn_memory_size;
  memory.data = pn_malloc(memory.size);

  PNModule module = {};
  module.memory = &memory;
  pn_allocator_init(&module.allocator, PN_MIN_CHUNKSIZE, "module");
  pn_allocator_init(&module.value_allocator, PN_MIN_CHUNKSIZE, "value");
  pn_allocator_init(&module.instruction_allocator, PN_MIN_CHUNKSIZE,
                    "instruction");
  pn_allocator_init(&module.temp_allocator, PN_MIN_CHUNKSIZE, "temp");

  PNBlockInfoContext context = {};
  pn_module_block_read(&module, &context, &bs);
  PN_TRACE_DEDENT(MODULE_BLOCK, 2);
  PN_TRACE(MODULE_BLOCK, "}\n");

  if (g_pn_run) {
    PN_BEGIN_TIME(EXECUTE);
    pn_memory_init_startinfo(&memory, g_pn_argv, g_pn_environ);

    PNExecutor executor = {};
    PNThread* thread = &executor.main_thread;
    thread->executor = &executor;
    thread->next = thread;
    thread->prev = thread;
    thread->futex_state = PN_FUTEX_NONE;

    pn_executor_init(&executor, &module);
    PNBool running = PN_TRUE;
    uint32_t last_thread_id = executor.main_thread.id;
    while (running) {
      uint32_t i;
      for (i = 0; i < PN_INSTRUCTIONS_QUANTUM && running &&
                  thread->state == PN_THREAD_RUNNING;
           ++i) {
        pn_thread_execute_instruction(thread);
        running = thread->current_call_frame != &executor.sentinel_frame &&
                  !executor.exiting;
      }

      if (!running) {
        break;
      }

      /* Remove the dead thread from the executing linked-list. Only the
       * currently executing thread should be in this state. */
      PNThread* next_thread = thread->next;
      if (thread->state == PN_THREAD_DEAD) {
        assert(thread != &executor.main_thread);
        /* Unlink from executing linked list */
        thread->prev->next = thread->next;
        thread->next->prev = thread->prev;

        /* Link into dead list, singly-linked */
        thread->next = executor.dead_threads;
        thread->prev = NULL;
        executor.dead_threads = thread;
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
    PN_END_TIME(EXECUTE);

    if (g_pn_verbose) {
      PN_PRINT("Exit code: %d\n", executor.exit_code);
    }
  }

  PN_END_TIME(TOTAL);

#if PN_TIMERS
  if (g_pn_print_time) {
    PN_PRINT("-----------------\n");
#define PN_PRINT_TIMER(name)                                          \
  struct timespec* timer_##name = &g_pn_timer_times[PN_TIMER_##name]; \
  PN_PRINT("timer %-30s: %f sec (%%%.0f)\n", #name,                   \
           pn_timespec_to_double(timer_##name),                       \
           100 * pn_timespec_to_double(timer_##name) /                \
               pn_timespec_to_double(timer_TOTAL));
    PN_FOREACH_TIMER(PN_PRINT_TIMER);
  }
#endif /* PN_TIMERS */

  if (g_pn_print_named_functions) {
    PN_PRINT("-----------------\n");
    uint32_t i;
    for (i = 0; i < module.num_functions; ++i) {
      PNFunction* function = &module.functions[i];
      if (function->name) {
        PN_PRINT("%d. %s\n", i, function->name);
      }
    }
  }

  if (g_pn_print_stats) {
    PN_PRINT("-----------------\n");
    PN_PRINT("num_types: %u\n", module.num_types);
    PN_PRINT("num_functions: %u\n", module.num_functions);
    PN_PRINT("num_global_vars: %u\n", module.num_global_vars);
    PN_PRINT("max num_constants: %u\n", pn_max_num_constants(&module));
    PN_PRINT("max num_values: %u\n", pn_max_num_values(&module));
    PN_PRINT("max num_bbs: %u\n", pn_max_num_bbs(&module));
    PN_PRINT("global_var size : %s\n",
             pn_human_readable_size_leaky(memory.globalvar_end -
                                          memory.globalvar_start));
    PN_PRINT("startinfo size : %s\n",
             pn_human_readable_size_leaky(memory.startinfo_end -
                                          memory.startinfo_start));
    pn_allocator_print_stats_leaky(&module.allocator);
    pn_allocator_print_stats_leaky(&module.value_allocator);
    pn_allocator_print_stats_leaky(&module.instruction_allocator);
  }

  if (g_pn_print_opcode_counts) {
    PNOpcodeCountPair pairs[PN_MAX_OPCODE];
    uint32_t n;
    for (n = 0; n < PN_MAX_OPCODE; ++n) {
      pairs[n].opcode = n;
      pairs[n].count = g_pn_opcode_count[n];
    }
    qsort(&pairs, PN_MAX_OPCODE, sizeof(PNOpcodeCountPair),
          pn_opcode_count_pair_compare);

    PN_PRINT("-----------------\n");
    for (n = 0; n < PN_MAX_OPCODE; ++n) {
      if (pairs[n].count > 0) {
        PN_PRINT("%40s %d\n", g_pn_opcode_names[pairs[n].opcode],
                 pairs[n].count);
      }
    }
  }

  return 0;
}
