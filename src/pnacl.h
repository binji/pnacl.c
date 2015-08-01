/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PNACL_H_
#define PNACL_H_

#include <stdint.h>
#include <stdlib.h>

/**** CONFIG ******************************************************************/

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

/**** TYPEDEFS  ***************************************************************/

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

/**** MACROS ******************************************************************/

#define PN_FALSE 0
#define PN_TRUE 1

#define PN_INVALID_VALUE_ID ((PNValueId)~0)
#define PN_INVALID_BB_ID ((PNBasicBlockId)~0)
#define PN_INVALID_FUNCTION_ID ((PNFunctionId)~0)
#define PN_INVALID_TYPE_ID ((PNTypeId)~0)
#define PN_INVALID_FUNCTION_ID ((PNFunctionId)~0)

#define PN_NANOSECONDS_IN_A_MICROSECOND 1000
#define PN_NANOSECONDS_IN_A_SECOND 1000000000

#define PN_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

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

/**** TRACING *****************************************************************/

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

#else

#define PN_TRACE_PRINT_INDENT() (void)0
#define PN_TRACE(flag, ...) (void)0
#define PN_IS_TRACE(flag) PN_FALSE
#define PN_TRACE_INDENT(flag, c) (void)0
#define PN_TRACE_DEDENT(flag, c) (void)0

#endif /* PN_TRACING */

/**** TIMERS ******************************************************************/

#if PN_TIMERS

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

#else

#define PN_BEGIN_TIME(name) (void)0
#define PN_END_TIME(name) (void)0

#endif

/**** ENUMS *******************************************************************/

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

typedef enum PNIntrinsicId {
  PN_INTRINSIC_NULL,
#define PN_INTRINSIC_DEFINE(e, name) PN_INTRINSIC_##e,
  PN_FOREACH_INTRINSIC(PN_INTRINSIC_DEFINE)
#undef PN_INTRINSIC_DEFINE
  PN_MAX_INTRINSICS,
} PNIntrinsicId;

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

/**** STRUCTS *****************************************************************/

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

/**** FORWARD DECLARATIONS ****************************************************/

#define PN_BUILTIN(e)                                                          \
  static PNRuntimeValue pn_builtin_##e(PNThread* thread, PNFunction* function, \
                                       uint32_t num_args, PNValueId* arg_ids);
  PN_FOREACH_BUILTIN(PN_BUILTIN)
#undef PN_BUILTIN

#endif /* PNACL_H_ */
