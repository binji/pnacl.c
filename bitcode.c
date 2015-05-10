/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef PN_TIMERS
#define PN_TIMERS 1
#endif

#ifndef PN_TRACING
#define PN_TRACING 1
#endif

#define PN_ARENA_SIZE (32 * 1024 * 1024)
#define PN_VALUE_ARENA_SIZE (8 * 1024 * 1024)
#define PN_INSTRUCTION_ARENA_SIZE (32 * 1024 * 1024)
#define PN_TEMP_ARENA_SIZE (1 * 1024 * 1024)
#define PN_MAX_BLOCK_ABBREV_OP 10
#define PN_MAX_BLOCK_ABBREV 100
#define PN_MAX_FUNCTION_ARGS 15
#define PN_MAX_FUNCTION_NAME 256

#define PN_FALSE 0
#define PN_TRUE 1

#define PN_INVALID_VALUE_ID ((PNValueId)~0)
#define PN_INVALID_BLOCK_ID ((PNBasicBlockId)~0)
#define PN_INVALID_TYPE_ID ((PNTypeId)~0)

#define PN_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#if PN_TRACING
#define PN_TRACE(flag, ...) if (g_pn_trace_##flag) printf(__VA_ARGS__)
#define PN_IS_TRACE(flag) g_pn_trace_##flag
#else
#define PN_TRACE(flag, ...) (void)0
#define PN_IS_TRACE(flag) PN_FALSE
#endif /* PN_TRACING */
#define PN_WARN(...) if( g_pn_verbose > 0) fprintf(stderr, __VA_ARGS__)
#define PN_ERROR(...) fprintf(stderr, __VA_ARGS__)
#define PN_FATAL(...)      \
  do {                     \
    PN_ERROR(__VA_ARGS__); \
    exit(1);               \
  } while (0)
#define PN_UNREACHABLE() PN_FATAL("unreachable")
#define PN_STATIC_ASSERT(x) int __pn_static_assert_##__LINE__[x ? 1 : -1]

#if PN_TIMERS

#define PN_NANOSECONDS_IN_A_SECOND 1000000000

#define PN_BEGIN_TIME(name)          \
  struct timespec start_time_##name; \
  clock_gettime(CLOCK_MONOTONIC, &start_time_##name) /* no semicolon */

#define PN_END_TIME(name)                                               \
  do {                                                                  \
    struct timespec end_time_##name;                                    \
    clock_gettime(CLOCK_MONOTONIC, &end_time_##name);                   \
    struct timespec time_delta_##name;                                  \
    PNTimespecSubtract(&time_delta_##name, &end_time_##name,            \
                       &start_time_##name);                             \
    struct timespec* timer_##name = &g_pn_timer_times[PN_TIMER_##name]; \
    struct timespec new_timer_##name;                                   \
    PNTimespecAdd(&new_timer_##name, timer_##name, &time_delta_##name); \
    *timer_##name = new_timer_##name;                                   \
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
  V(CALCULATE_USES)               \
  V(CALCULATE_PRED_BBS)           \
  V(CALCULATE_PHI_ASSIGNS)        \
  V(CALCULATE_LIVENESS)           \
  V(FUNCTION_TRACE)

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
typedef uint16_t PNValueId;
typedef uint32_t PNFunctionId;
typedef uint16_t PNConstantId;
typedef uint32_t PNGlobalVarId;
typedef uint32_t PNInitializerId;
typedef uint16_t PNInstructionId;
typedef uint16_t PNBasicBlockId;
typedef uint16_t PNAlignment;

static int g_pn_verbose;
static const char* g_pn_filename;
static PNBool g_pn_print_stats;

#if PN_TIMERS
static PNBool g_pn_print_time;
#endif /* PN_TIMERS */

#if PN_TRACING
#define PN_FOREACH_TRACE(V)                   \
  V(BLOCKINFO_BLOCK, "blockinfo-block")       \
  V(TYPE_BLOCK, "type-block")                 \
  V(GLOBALVAR_BLOCK, "globalvar-block")       \
  V(VALUE_SYMTAB_BLOCK, "value-symtab-block") \
  V(CONSTANTS_BLOCK, "constants-block")       \
  V(FUNCTION_BLOCK, "function-block")         \
  V(MODULE_BLOCK, "module-block")             \
  V(BASIC_BLOCKS, "basic-blocks")             \
  V(INSTRUCTIONS, "instructions")

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

typedef enum PNConstantsCode {
  PN_CONSTANTS_CODE_SETTYPE = 1,
  PN_CONSTANTS_CODE_UNDEF = 3,
  PN_CONSTANTS_CODE_INTEGER = 4,
  PN_CONSTANTS_CODE_FLOAT = 6,
} PNConstantsCode;

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

typedef struct PNArena {
  void* data;
  uint32_t size;
  uint32_t capacity;
  /* Last allocation. This is the only one that can be realloc'd */
  void* last_alloc;
} PNArena;

typedef struct PNArenaMark {
  uint32_t size;
  void* last_alloc;
} PNArenaMark;

typedef struct PNBitSet {
  uint32_t num_bits_set;
  uint32_t num_words;
  uint32_t* words;
} PNBitSet;

typedef struct PNBitStream {
  uint8_t* data;
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

typedef struct PNSwitchCaseValue {
  int32_t low;
  int32_t high;
  PNBool is_single;
} PNSwitchCaseValue;

typedef struct PNSwitchCase {
  PNBasicBlockId bb_id;
  uint32_t num_values;
  PNSwitchCaseValue* values;
} PNSwitchCase;

typedef struct PNPhiIncoming {
  PNBasicBlockId bb_id;
  PNValueId value_id;
} PNPhiIncoming;

typedef struct PNInstruction {
  PNFunctionCode code;
} PNInstruction;

typedef struct PNInstructionBinop {
  PNFunctionCode code;
  PNValueId result_value_id;
  PNValueId value0_id;
  PNValueId value1_id;
  PNBinOp opcode;
  int32_t flags;
} PNInstructionBinop;

typedef struct PNInstructionCast {
  PNFunctionCode code;
  PNValueId result_value_id;
  PNValueId value_id;
  PNCast opcode;
  PNTypeId type_id;
} PNInstructionCast;

typedef struct PNInstructionRet {
  PNFunctionCode code;
  PNValueId value_id; /* Or PN_INVALID_VALUE_ID */
} PNInstructionRet;

typedef struct PNInstructionBr {
  PNFunctionCode code;
  PNBasicBlockId true_bb_id;
  PNBasicBlockId false_bb_id; /* Or PN_INVALID_BLOCK_ID */
  PNValueId value_id;         /* Or PN_INVALID_VALUE_ID */
} PNInstructionBr;

typedef struct PNInstructionSwitch {
  PNFunctionCode code;
  PNValueId value_id;
  PNBasicBlockId default_bb_id;
  uint32_t num_cases;
  PNSwitchCase* cases;
  PNTypeId type_id;
} PNInstructionSwitch;

typedef struct PNInstructionUnreachable {
  PNFunctionCode code;
} PNInstructionUnreachable;

typedef struct PNInstructionPhi {
  PNFunctionCode code;
  PNValueId result_value_id;
  uint32_t num_incoming;
  PNPhiIncoming* incoming;
  PNTypeId type_id;
} PNInstructionPhi;

typedef struct PNInstructionAlloca {
  PNFunctionCode code;
  PNValueId result_value_id;
  PNValueId size_id;
  PNAlignment alignment;
} PNInstructionAlloca;

typedef struct PNInstructionLoad {
  PNFunctionCode code;
  PNValueId result_value_id;
  PNValueId src_id;
  PNAlignment alignment;
  PNTypeId type_id;
} PNInstructionLoad;

typedef struct PNInstructionStore {
  PNFunctionCode code;
  PNValueId dest_id;
  PNValueId value_id;
  PNAlignment alignment;
} PNInstructionStore;

typedef struct PNInstructionCmp2 {
  PNFunctionCode code;
  PNValueId result_value_id;
  PNValueId value0_id;
  PNValueId value1_id;
  PNCmp2 opcode;
} PNInstructionCmp2;

typedef struct PNInstructionVselect {
  PNFunctionCode code;
  PNValueId result_value_id;
  PNValueId cond_id;
  PNValueId true_value_id;
  PNValueId false_value_id;
} PNInstructionVselect;

typedef struct PNInstructionForwardtyperef {
  PNFunctionCode code;
  PNValueId value_id;
  PNValueId type_id;
} PNInstructionForwardtyperef;

typedef struct PNInstructionCall {
  PNFunctionCode code;
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
  PNValueId source_value_id;
  PNValueId dest_value_id;
} PNPhiAssign;

typedef struct PNBasicBlock {
  uint32_t num_instructions;
  void* instructions;
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

typedef struct PNConstant {
  PNConstantsCode code;
  PNTypeId type_id;
  union {
    /* PN_CONSTANTS_CODE_INTEGER */
    int32_t int_value;
    /* PN_CONSTANTS_CODE_FLOAT */
    float float_value;
  };
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
  char name[PN_MAX_FUNCTION_NAME];
  PNTypeId type_id;
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
  union {
    /* PN_TYPE_CODE_INTEGER */
    uint32_t width;
    /* PN_TYPE_CODE_FUNCTION */
    struct {
      PNBool is_varargs;
      PNTypeId return_type;
      uint32_t num_args;
      PNTypeId arg_types[PN_MAX_FUNCTION_ARGS];
    };
  };
} PNType;

typedef struct PNInitializer {
  PNGlobalVarCode code;
  union {
    /* PN_GLOBALVAR_CODE_ZEROFILL */
    /* PN_GLOBALVAR_CODE_DATA */
    struct {
      uint32_t num_bytes;
      uint8_t* data; /* NULL when ZEROFILL. Allocated, should be free'd. */
    };
    /* PN_GLOBALVAR_CODE_RELOC */
    struct {
      uint32_t index;
      int32_t addend;
    };
  };
} PNInitializer;

typedef struct PNGlobalVar {
  uint32_t num_initializers;
  PNInitializer* initializers;
  PNAlignment alignment;
  PNBool is_constant;
} PNGlobalVar;

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

  PNArena arena;
  PNArena value_arena;
  PNArena instruction_arena;
  PNArena temp_arena;
} PNModule;

typedef struct PNLivenessState {
  PNBitSet* livein;
  PNBitSet* liveout;
} PNLivenessState;

typedef struct PNBlockInfoContext {
  uint32_t num_abbrevs;
  PNBlockAbbrevs block_abbrev_map[PN_MAX_BLOCK_IDS];
  PNBool use_relative_ids;
} PNBlockInfoContext;

#if PN_TIMERS
void PNTimespecCheck(struct timespec* a) {
  assert(a->tv_sec >= 0);
  assert(a->tv_nsec >= 0 && a->tv_nsec < PN_NANOSECONDS_IN_A_SECOND);
}

void PNTimespecSubtract(struct timespec* result,
                        struct timespec* a,
                        struct timespec* b) {
  PNTimespecCheck(a);
  PNTimespecCheck(b);
  result->tv_sec = a->tv_sec - b->tv_sec;
  result->tv_nsec = a->tv_nsec - b->tv_nsec;
  if (result->tv_nsec < 0) {
    result->tv_sec--;
    result->tv_nsec += PN_NANOSECONDS_IN_A_SECOND;
  }
  PNTimespecCheck(result);
}

void PNTimespecAdd(struct timespec* result,
                   struct timespec* a,
                   struct timespec* b) {
  PNTimespecCheck(a);
  PNTimespecCheck(b);
  result->tv_sec = a->tv_sec + b->tv_sec;
  result->tv_nsec = a->tv_nsec + b->tv_nsec;
  if (result->tv_nsec >= PN_NANOSECONDS_IN_A_SECOND) {
    result->tv_sec++;
    result->tv_nsec -= PN_NANOSECONDS_IN_A_SECOND;
  }
  PNTimespecCheck(result);
}

double PNTimespecToDouble(struct timespec* t) {
  return (double)t->tv_sec + (double)t->tv_nsec / PN_NANOSECONDS_IN_A_SECOND;
}

#endif /* PN_TIMERS */

static void pn_arena_init(PNArena* arena, uint32_t size) {
  arena->data = malloc(size);
  arena->size = 0;
  arena->capacity = size;
  arena->last_alloc = NULL;
}

static void pn_arena_destroy(PNArena* arena) {
  free(arena->data);
}

static void* pn_arena_alloc(PNArena* arena, uint32_t size) {
  uint32_t avail = arena->capacity - arena->size;
  if (size > avail) {
    PN_FATAL("Arena exhausted. Requested: %u, avail: %u, capacity: %u\n", size,
             avail, arena->capacity);
  }

  /* Align to 8 bytes */
  size = (size + 7) & ~7;

  void* ret = (uint8_t*)arena->data + arena->size;
  arena->size += size;
  arena->last_alloc = ret;
  return ret;
}

static void* pn_arena_allocz(PNArena* arena, uint32_t size) {
  void* p = pn_arena_alloc(arena, size);
  memset(p, 0, size);
  return p;
}

static void* pn_arena_realloc(PNArena* arena, void* p, uint32_t new_size) {
  if (p) {
    if (p != arena->last_alloc) {
      PN_FATAL(
          "Attempting to realloc, but it was not the last allocation:\n"
          "p = %p, last_alloc = %p\n",
          p, arena->last_alloc);
    }

    arena->size = (uint8_t*)p - (uint8_t*)arena->data;
  }
  void* ret = pn_arena_alloc(arena, new_size);
  assert(!p || ret == p);
  return ret;
}

static PNArenaMark pn_arena_mark(PNArena* arena) {
  PNArenaMark mark;
  mark.size = arena->size;
  mark.last_alloc = arena->last_alloc;

  return mark;
}

static void pn_arena_reset_to_mark(PNArena* arena, PNArenaMark mark) {
  arena->size = mark.size;
  arena->last_alloc = mark.last_alloc;
}

static void pn_bitset_init(PNArena* arena, PNBitSet* bitset, int32_t size) {
  bitset->num_bits_set = 0;
  bitset->num_words = (size + 31) >> 5;
  bitset->words =
      (uint32_t*)pn_arena_allocz(arena, sizeof(uint32_t) * bitset->num_words);
}

static void pn_bitset_set(PNBitSet* bitset, uint32_t bit, PNBool set) {
  uint32_t word = bit >> 5;
  uint32_t mask = 1 << (bit & 31);
  assert(word < bitset->num_words);

  PNBool was_set = (bitset->words[word] & mask) != 0;

  if (set) {
    bitset->words[word] |= mask;
  } else {
    bitset->words[word] &= ~mask;
  }

  if (set != was_set) {
    if (set) {
      bitset->num_bits_set++;
    } else {
      bitset->num_bits_set--;
    }
  }
}

static PNBool pn_bitset_is_set(PNBitSet* bitset, uint32_t bit) {
  uint32_t word = bit >> 5;
  uint32_t mask = 1 << (bit & 31);
  assert(word < bitset->num_words);

  return (bitset->words[word] & mask) != 0;
}

static const char* pn_binop_get_name(uint32_t op) {
  const char* names[] = {
      "add", "sub", "mul", "udiv", "sdiv", "urem", "srem", "shl", "lshr",
      "ashr", "and", "or", "xor"
  };

  if (op >= PN_ARRAY_SIZE(names)) {
    PN_FATAL("Invalid op: %u\n", op);
  }

  return names[op];
}

static const char* pn_cast_get_name(uint32_t op) {
  const char* names[] = {
      "trunc", "zext", "sext", "fptoui", "fptosi", "uitofp", "sitofp",
      "fptrunc", "fpext", NULL, NULL, "bitcast"
  };

  if (op >= PN_ARRAY_SIZE(names)) {
    PN_FATAL("Invalid op: %u\n", op);
  }

  return names[op];
}

static const char* pn_cmp2_get_name(uint32_t op) {
  const char* names[] = {
      "fcmp_false", "fcmp_oeq", "fcmp_ogt", "fcmp_oge",  "fcmp_olt", "fcmp_ole",
      "fcmp_one",   "fcmp_ord", "fcmp_uno", "fcmp_ueq",  "fcmp_ugt", "fcmp_uge",
      "fcmp_ult",   "fcmp_ule", "fcmp_une", "fcmp_true", NULL,       NULL,
      NULL,         NULL,       NULL,       NULL,        NULL,       NULL,
      NULL,         NULL,       NULL,       NULL,        NULL,       NULL,
      NULL,         NULL,       "icmp_eq",  "icmp_ne",   "icmp_ugt", "icmp_uge",
      "icmp_ult",   "icmp_ule", "icmp_sgt", "icmp_sge",  "icmp_slt", "icmp_sle",
  };

  if (op >= PN_ARRAY_SIZE(names)) {
    PN_FATAL("Invalid op: %u\n", op);
  }

  return names[op];
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

static void pn_bitstream_init(PNBitStream* bs,
                              uint8_t* data,
                              uint32_t data_len) {
  bs->data = data;
  bs->data_len = data_len;
  bs->curword = 0;
  bs->curword_bits = 0;
  bs->bit_offset = 0;
}

static uint32_t pn_bitstream_read_frac_bits(PNBitStream* bs, int num_bits) {
  assert(num_bits <= bs->curword_bits);
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
  bs->curword = *(uint32_t*)(bs->data + byte_offset);
  if (byte_offset + 4 < bs->data_len) {
    bs->curword_bits = 32;
  } else {
    bs->curword_bits = (bs->data_len - byte_offset) * 8;
  }
  assert(bs->curword_bits <= 32);
}

static uint32_t pn_bitstream_read(PNBitStream* bs, int num_bits) {
  assert(num_bits <= 32);
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
  uint32_t piece = pn_bitstream_read(bs, num_bits);
  uint32_t hi_mask = 1 << (num_bits - 1);
  if ((piece & hi_mask) == 0) {
    return piece;
  }

  uint32_t lo_mask = hi_mask - 1;
  uint32_t result = 0;
  int shift = 0;
  while (1) {
    result |= (piece & lo_mask) << shift;
    if ((piece & hi_mask) == 0) {
      return result;
    }
    shift += num_bits - 1;
    piece = pn_bitstream_read(bs, num_bits);
  }
}

static void pn_bitstream_seek_bit(PNBitStream* bs, uint32_t bit_offset) {
  /* Align to 32 bits */
  bs->bit_offset = bit_offset & ~31;
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
  pn_bitstream_seek_bit(bs, (bs->bit_offset + 31) & ~31);
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
    PN_FATAL("accessing invalid type %d (max %d)", type_id, module->num_types);
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

static PNType* pn_module_append_type(PNModule* module, PNTypeId* out_type_id) {
  *out_type_id = module->num_types;
  uint32_t new_size = sizeof(PNType) * (module->num_types + 1);
  module->types = pn_arena_realloc(&module->arena, module->types, new_size);

  module->num_types++;
  return &module->types[*out_type_id];
}

static void pn_string_concat(PNArena* arena,
                             char** dest,
                             uint32_t* dest_len,
                             const char* src,
                             uint32_t src_len) {
  if (src_len == 0) {
    src_len = strlen(src);
  }

  uint32_t old_dest_len = *dest_len;
  *dest_len += src_len;
  *dest = pn_arena_realloc(arena, *dest, *dest_len);
  memcpy(*dest + old_dest_len - 1, src, src_len);
  (*dest)[*dest_len - 1] = 0;
}

static const char* pn_type_describe(PNModule* module, PNTypeId type_id) {
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
          return "int1";
        case 8:
          return "int8";
        case 16:
          return "int16";
        case 32:
          return "int32";
        case 64:
          return "int64";
        default:
          PN_FATAL("Integer with bad width: %d\n", type->width);
          return "badInteger";
      }
    case PN_TYPE_CODE_FLOAT:
      return "float";

    case PN_TYPE_CODE_DOUBLE:
      return "double";

    case PN_TYPE_CODE_FUNCTION: {
      char* buffer = pn_arena_alloc(&module->temp_arena, 1);
      uint32_t buffer_len = 1;
      buffer[0] = 0;

      pn_string_concat(&module->temp_arena, &buffer, &buffer_len,
                       pn_type_describe(module, type->return_type), 0);
      pn_string_concat(&module->temp_arena, &buffer, &buffer_len, "(", 1);
      uint32_t n;
      for (n = 0; n < type->num_args; ++n) {
        if (n != 0) {
          pn_string_concat(&module->temp_arena, &buffer, &buffer_len, ",", 1);
        }

        pn_string_concat(&module->temp_arena, &buffer, &buffer_len,
                         pn_type_describe(module, type->arg_types[n]), 0);
      }
      pn_string_concat(&module->temp_arena, &buffer, &buffer_len, ")", 1);
      return buffer;
    }

    default:
      return "<unknown>";
  }
}

static PNFunction* pn_module_get_function(PNModule* module,
                                          PNFunctionId function_id) {
  if (function_id < 0 || function_id >= module->num_functions) {
    PN_FATAL("accessing invalid function %d (max %d)", function_id,
             module->num_functions);
  }

  return &module->functions[function_id];
}

static PNConstant* pn_function_get_constant(PNFunction* function,
                                            PNConstantId constant_id) {
  if (constant_id < 0 || constant_id >= function->num_constants) {
    PN_FATAL("accessing invalid constant %d (max %d)", constant_id,
             function->num_constants);
  }

  return &function->constants[constant_id];
}

static PNConstant* pn_function_append_constant(PNModule* module,
                                               PNFunction* function,
                                               PNConstantId* out_constant_id) {
  *out_constant_id = function->num_constants;
  uint32_t new_size = sizeof(PNConstant) * (function->num_constants + 1);
  function->constants =
      pn_arena_realloc(&module->arena, function->constants, new_size);

  function->num_constants++;
  return &function->constants[*out_constant_id];
}

static PNGlobalVar* pn_module_get_global_var(PNModule* module,
                                             PNGlobalVarId global_var_id) {
  if (global_var_id < 0 || global_var_id >= module->num_global_vars) {
    PN_FATAL("accessing invalid global_var %d (max %d)", global_var_id,
             module->num_global_vars);
  }

  return &module->global_vars[global_var_id];
}

static PNValue* pn_module_get_value(PNModule* module, PNValueId value_id) {
  if (value_id < 0 || value_id >= module->num_values) {
    PN_FATAL("accessing invalid value %d (max %d)", value_id,
             module->num_values);
  }

  return &module->values[value_id];
}

static PNValue* pn_module_append_value(PNModule* module,
                                       PNValueId* out_value_id) {
  *out_value_id = module->num_values;
  uint32_t new_size = sizeof(PNValue) * (module->num_values + 1);
  module->values =
      pn_arena_realloc(&module->value_arena, module->values, new_size);

  module->num_values++;
  return &module->values[*out_value_id];
}

static uint32_t pn_function_num_values(PNModule* module, PNFunction* function) {
  return module->num_values + function->num_values;
}

static PNValue* pn_function_get_value(PNModule* module,
                                      PNFunction* function,
                                      PNValueId value_id) {
  if (value_id < 0) {
    PN_FATAL("accessing invalid value %d", value_id);
  } else if (value_id < module->num_values) {
    return &module->values[value_id];
  }

  value_id -= module->num_values;

  if (value_id < function->num_values) {
    return &function->values[value_id];
  }

  PN_FATAL("accessing invalid value %d (max %d)", value_id,
           module->num_values + function->num_values);
}

static PNValue* pn_function_append_value(PNModule* module,
                                         PNFunction* function,
                                         PNValueId* out_value_id) {
  uint32_t index = function->num_values;
  *out_value_id = module->num_values + index;
  uint32_t new_size = sizeof(PNValue) * (function->num_values + 1);
  function->values =
      pn_arena_realloc(&module->value_arena, function->values, new_size);

  function->num_values++;
  return &function->values[index];
}

static const char* pn_value_describe(PNModule* module,
                                     PNFunction* function,
                                     PNValueId value_id) {
  PNValue* value;
  if (value_id >= module->num_values) {
    value = pn_function_get_value(module, function, value_id);
  } else {
    value = pn_module_get_value(module, value_id);
  }

  const char* type_str = pn_type_describe(module, value->type_id);
  int buffer_len = snprintf(NULL, 0, "%%%d(%s)", value_id, type_str);
  char* buffer = pn_arena_alloc(&module->temp_arena, buffer_len + 1);
  snprintf(buffer, buffer_len + 1, "%%%d(%s)", value_id, type_str);
  buffer[buffer_len] = 0;

  return buffer;
}

static void* pn_function_append_instruction(
    PNModule* module,
    PNFunction* function,
    PNBasicBlock* bb,
    uint32_t instruction_size,
    PNInstructionId* out_instruction_id) {
  PNArena* arena = &module->instruction_arena;
  void* p = pn_arena_allocz(arena, instruction_size);
  if (bb->num_instructions == 0) {
    bb->instructions = p;
  }
  *out_instruction_id = bb->num_instructions++;
  return p;
}

#define PN_FUNCTION_APPEND_INSTRUCTION(type, module, function, bb, id) \
  (type*) pn_function_append_instruction(module, function, bb, sizeof(type), id)

static void pn_basic_block_list_append(PNArena* arena,
                                       PNBasicBlockId** bb_list,
                                       uint32_t* num_els,
                                       PNBasicBlockId bb_id) {
  *bb_list = pn_arena_realloc(arena, *bb_list,
                              sizeof(PNBasicBlockId) * (*num_els + 1));
  (*bb_list)[(*num_els)++] = bb_id;
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
    bb->pred_bb_ids = pn_arena_alloc(&module->arena,
                                     sizeof(PNBasicBlockId) * bb->num_pred_bbs);
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

static PNInstruction* pn_instruction_next(PNInstruction* inst) {
  uint8_t* p = (uint8_t*)inst;

#define INST_CASE(M, T) \
  case M:               \
    p += sizeof(T);     \
    break;

  switch (inst->code) {
    INST_CASE(PN_FUNCTION_CODE_INST_BINOP, PNInstructionBinop)
    INST_CASE(PN_FUNCTION_CODE_INST_CAST, PNInstructionCast)
    INST_CASE(PN_FUNCTION_CODE_INST_RET, PNInstructionRet)
    INST_CASE(PN_FUNCTION_CODE_INST_BR, PNInstructionBr)
    INST_CASE(PN_FUNCTION_CODE_INST_UNREACHABLE, PNInstructionUnreachable)
    INST_CASE(PN_FUNCTION_CODE_INST_ALLOCA, PNInstructionAlloca)
    INST_CASE(PN_FUNCTION_CODE_INST_LOAD, PNInstructionLoad)
    INST_CASE(PN_FUNCTION_CODE_INST_STORE, PNInstructionStore)
    INST_CASE(PN_FUNCTION_CODE_INST_CMP2, PNInstructionCmp2)
    INST_CASE(PN_FUNCTION_CODE_INST_VSELECT, PNInstructionVselect)
    INST_CASE(PN_FUNCTION_CODE_INST_FORWARDTYPEREF, PNInstructionForwardtyperef)

#undef INST_CASE

    case PN_FUNCTION_CODE_INST_SWITCH: {
      PNInstructionSwitch* i = (PNInstructionSwitch*)inst;
      p += sizeof(PNInstructionSwitch);
      p += sizeof(PNSwitchCase) * i->num_cases;
      int32_t c;
      for (c = 0; c < i->num_cases; ++c) {
        PNSwitchCase* switch_case = &i->cases[c];
        p += sizeof(PNSwitchCaseValue) * switch_case->num_values;
      }
      break;
    }

    case PN_FUNCTION_CODE_INST_PHI: {
      PNInstructionPhi* i = (PNInstructionPhi*)inst;
      p += sizeof(PNInstructionPhi);
      p += sizeof(PNPhiIncoming) * i->num_incoming;
      break;
    }

    case PN_FUNCTION_CODE_INST_CALL:
    case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      p += sizeof(PNInstructionCall);
      p += sizeof(PNValueId) * i->num_args;
      break;
    }

    default:
      PN_FATAL("Invalid instruction code: %d\n", inst->code);
      break;
  }

  /* TODO(binji): this requires knowledge that the arena aligns to 8 bytes. Do
   * something less fragile. */
  p = (uint8_t*)(((intptr_t)p + 7) & ~7);

  return (PNInstruction*)p;
}

static void pn_instruction_trace(PNModule* module,
                                 PNFunction* function,
                                 PNInstruction* inst) {
#if PN_TRACING
  if (!PN_IS_TRACE(INSTRUCTIONS)) {
    return;
  }

  PNArenaMark mark = pn_arena_mark(&module->temp_arena);

  switch (inst->code) {
    case PN_FUNCTION_CODE_INST_BINOP: {
      PNInstructionBinop* i = (PNInstructionBinop*)inst;
      printf("  %s. binop op:%s(%d) %s %s (flags:%d)\n",
             pn_value_describe(module, function, i->result_value_id),
             pn_binop_get_name(i->opcode), i->opcode,
             pn_value_describe(module, function, i->value0_id),
             pn_value_describe(module, function, i->value1_id), i->flags);
      break;
    }

    case PN_FUNCTION_CODE_INST_CAST: {
      PNInstructionCast* i = (PNInstructionCast*)inst;
      printf("  %s. cast op:%s(%d) %s\n",
             pn_value_describe(module, function, i->result_value_id),
             pn_cast_get_name(i->opcode), i->opcode,
             pn_value_describe(module, function, i->value_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_RET: {
      PNInstructionRet* i = (PNInstructionRet*)inst;
      if (i->value_id != PN_INVALID_VALUE_ID) {
        printf("  ret %s\n", pn_value_describe(module, function, i->value_id));
      } else {
        printf("  ret\n");
      }
      break;
    }

    case PN_FUNCTION_CODE_INST_BR: {
      PNInstructionBr* i = (PNInstructionBr*)inst;
      if (i->false_bb_id != PN_INVALID_BLOCK_ID) {
        printf("  br %s ? %d : %d\n",
               pn_value_describe(module, function, i->value_id), i->true_bb_id,
               i->false_bb_id);
      } else {
        printf("  br %d\n", i->true_bb_id);
      }
      break;
    }

    case PN_FUNCTION_CODE_INST_SWITCH: {
      PNInstructionSwitch* i = (PNInstructionSwitch*)inst;
      printf("  switch value:%s [default:%d]",
             pn_value_describe(module, function, i->value_id),
             i->default_bb_id);

      uint32_t c;
      for (c = 0; c < i->num_cases; ++c) {
        PNSwitchCase* switch_case = &i->cases[c];
        printf(" [");

        int32_t i;
        for (i = 0; i < switch_case->num_values; ++i) {
          PNSwitchCaseValue* value = &switch_case->values[i];
          if (value->is_single) {
            printf("[%d] ", value->low);
          } else {
            printf("[%d,%d] ", value->low, value->high);
          }
        }
        printf("=> bb:%d]", switch_case->bb_id);
      }
      printf("\n");
      break;
    }

    case PN_FUNCTION_CODE_INST_UNREACHABLE:
      printf("  unreachable\n");
      break;

    case PN_FUNCTION_CODE_INST_PHI: {
      PNInstructionPhi* i = (PNInstructionPhi*)inst;
      printf("  %s. phi",
             pn_value_describe(module, function, i->result_value_id));
      int32_t n;
      for (n = 0; n < i->num_incoming; ++n) {
        printf(" bb:%d=>%s", i->incoming[n].bb_id,
               pn_value_describe(module, function, i->incoming[n].value_id));
      }
      printf("\n");
      break;
    }

    case PN_FUNCTION_CODE_INST_ALLOCA: {
      PNInstructionAlloca* i = (PNInstructionAlloca*)inst;
      printf("  %s. alloca %s align=%d\n",
             pn_value_describe(module, function, i->result_value_id),
             pn_value_describe(module, function, i->size_id), i->alignment);
      break;
    }

    case PN_FUNCTION_CODE_INST_LOAD: {
      PNInstructionLoad* i = (PNInstructionLoad*)inst;
      printf("  %s. load src:%s align=%d\n",
             pn_value_describe(module, function, i->result_value_id),
             pn_value_describe(module, function, i->src_id), i->alignment);
      break;
    }

    case PN_FUNCTION_CODE_INST_STORE: {
      PNInstructionStore* i = (PNInstructionStore*)inst;
      printf("  store dest:%s value:%s align=%d\n",
             pn_value_describe(module, function, i->dest_id),
             pn_value_describe(module, function, i->value_id), i->alignment);
      break;
    }

    case PN_FUNCTION_CODE_INST_CMP2: {
      PNInstructionCmp2* i = (PNInstructionCmp2*)inst;
      printf("  %s. cmp2 op:%s(%d) %s %s\n",
             pn_value_describe(module, function, i->result_value_id),
             pn_cmp2_get_name(i->opcode), i->opcode,
             pn_value_describe(module, function, i->value0_id),
             pn_value_describe(module, function, i->value1_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_VSELECT: {
      PNInstructionVselect* i = (PNInstructionVselect*)inst;
      printf("  %s. vselect %s ? %s : %s\n",
             pn_value_describe(module, function, i->result_value_id),
             pn_value_describe(module, function, i->cond_id),
             pn_value_describe(module, function, i->true_value_id),
             pn_value_describe(module, function, i->false_value_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_FORWARDTYPEREF: {
      PNInstructionForwardtyperef* i = (PNInstructionForwardtyperef*)inst;
      printf("  forwardtyperef %s %s\n",
             pn_value_describe(module, function, i->value_id),
             pn_type_describe(module, i->type_id));
      break;
    }

    case PN_FUNCTION_CODE_INST_CALL:
    case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
      PNInstructionCall* i = (PNInstructionCall*)inst;
      PNType* return_type = pn_module_get_type(module, i->return_type_id);
      PNBool is_return_type_void = return_type->code == PN_TYPE_CODE_VOID;
      printf("  ");
      if (!is_return_type_void) {
        printf("%s. ", pn_value_describe(module, function, i->result_value_id));
      }
      printf("call ");
      const char* name = NULL;
      if (i->is_indirect) {
        printf("indirect ");
      } else {
        PNFunction* called_function =
            pn_module_get_function(module, i->callee_id);
        name = called_function->name;
      }
      if (name && name[0]) {
        printf("%s(%s) ", pn_value_describe(module, function, i->callee_id),
               name);
      } else {
        printf("%s ", pn_value_describe(module, function, i->callee_id));
      }
      printf("args:");

      int32_t n;
      for (n = 0; n < i->num_args; ++n) {
        printf(" %s", pn_value_describe(module, function, i->arg_ids[n]));
      }
      printf("\n");
      break;
    }

    default:
      PN_FATAL("Invalid instruction code: %d\n", inst->code);
      break;
  }

  pn_arena_reset_to_mark(&module->temp_arena, mark);
#endif /* PN_TRACING */
}

static void pn_basic_block_trace(PNModule* module,
                                 PNFunction* function,
                                 PNBasicBlock* bb,
                                 PNBasicBlockId bb_id) {
#if PN_TRACING
  if (!PN_IS_TRACE(BASIC_BLOCKS)) {
    return;
  }

  printf("bb:%d (preds:", bb_id);
  uint32_t n;
  for (n = 0; n < bb->num_pred_bbs; ++n) {
    printf(" %d", bb->pred_bb_ids[n]);
  }
  printf(" succs:");
  for (n = 0; n < bb->num_succ_bbs; ++n) {
    printf(" %d", bb->succ_bb_ids[n]);
  }
  printf(")\n");
  if (bb->first_def_id != PN_INVALID_VALUE_ID) {
    printf(" defs: [%%%d,%%%d]\n", bb->first_def_id, bb->last_def_id);
  }
  if (bb->num_uses) {
    printf(" uses:");
    for (n = 0; n < bb->num_uses; ++n) {
      printf(" %%%d", bb->uses[n]);
    }
    printf("\n");
  }
  if (bb->num_phi_uses) {
    printf(" phi uses:");
    for (n = 0; n < bb->num_phi_uses; ++n) {
      printf(" bb:%d=>%%%d", bb->phi_uses[n].incoming.bb_id,
             bb->phi_uses[n].incoming.value_id);
    }
    printf("\n");
  }
  if (bb->num_phi_assigns) {
    printf(" phi assigns:");
    for (n = 0; n < bb->num_phi_assigns; ++n) {
      printf(" %%%d<=%%%d", bb->phi_assigns[n].dest_value_id,
             bb->phi_assigns[n].source_value_id);
    }
    printf("\n");
  }
  if (bb->num_livein) {
    printf(" livein:");
    for (n = 0; n < bb->num_livein; ++n) {
      printf(" %%%d", bb->livein[n]);
    }
    printf("\n");
  }
  if (bb->num_liveout) {
    printf(" liveout:");
    for (n = 0; n < bb->num_liveout; ++n) {
      printf(" %%%d", bb->liveout[n]);
    }
    printf("\n");
  }

  PNInstruction* inst = (PNInstruction*)bb->instructions;
  uint32_t i;
  for (i = 0; i < bb->num_instructions; ++i) {
    pn_instruction_trace(module, function, inst);
    inst = pn_instruction_next(inst);
  }
#endif /* PN_TRACING */
}

static void pn_function_trace_header(PNFunction* function,
                                     PNFunctionId function_id) {
#if PN_TRACING
  if (function->name) {
    printf("function %%%d (%s)\n", function_id, function->name);
  } else {
    printf("function %%%d\n", function_id);
  }
#endif /* PN_TRACING */
}

static void pn_function_trace(PNModule* module,
                              PNFunction* function,
                              PNFunctionId function_id,
                              PNBool print_header) {
#if PN_TRACING
  PN_BEGIN_TIME(FUNCTION_TRACE);

  if (!PN_IS_TRACE(FUNCTION_BLOCK)) {
    return;
  }

  if (print_header) {
    pn_function_trace_header(function, function_id);
  }

  uint32_t i;
  for (i = 0; i < function->num_bbs; ++i) {
    pn_basic_block_trace(module, function, &function->bbs[i], i);
  }
  PN_END_TIME(FUNCTION_TRACE);
#endif /* PN_TRACING */
}

static PNTypeId pn_type_get_implicit_cast_type(PNModule* module,
                                               PNTypeId type0_id,
                                               PNTypeId type1_id) {
  if (type0_id == type1_id) {
    return type0_id;
  }

  PNType* type0 = pn_module_get_type(module, type0_id);
  PNType* type1 = pn_module_get_type(module, type1_id);

  if (type0->code != type1->code) {
    if (type0->code == PN_TYPE_CODE_FLOAT &&
        type1->code == PN_TYPE_CODE_DOUBLE) {
      return type1_id;
    } else if (type0->code == PN_TYPE_CODE_DOUBLE &&
               type1->code == PN_TYPE_CODE_FLOAT) {
      return type0_id;
    } else if (type0->code == PN_TYPE_CODE_FUNCTION &&
               type1->code == PN_TYPE_CODE_INTEGER && type1->width == 32) {
      return type1_id;
    } else if (type0->code == PN_TYPE_CODE_INTEGER && type0->width == 32 &&
               type1->code == PN_TYPE_CODE_FUNCTION) {
      return type1_id;
    } else {
      return PN_INVALID_TYPE_ID;
    }
  } else {
    if (type0->code == PN_TYPE_CODE_INTEGER) {
      if (type0->width > type1->width) {
        return type0_id;
      } else {
        return type1_id;
      }
    } else {
      return PN_INVALID_TYPE_ID;
    }
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
    pn_instruction_trace(module, function, inst);
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
  PNArenaMark mark = pn_arena_mark(&module->temp_arena);
  uint32_t num_invalid = 0;
  PNInstruction** invalid = NULL;

  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;
    PNInstruction* inst = bb->instructions;
    for (m = 0; m < bb->num_instructions; ++m) {
      if (!pn_instruction_calculate_result_value_type(module, function, inst)) {
        /* One of the types is invalid, try again later */
        invalid = pn_arena_realloc(&module->temp_arena, invalid,
                                   sizeof(PNInstruction*) * (num_invalid + 1));
        invalid[num_invalid++] = inst;
      }
      inst = pn_instruction_next(inst);
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
      printf("Unable to resolve types for %d values:\n", num_invalid);
      for (n = 0; n < num_invalid; ++n) {
        pn_instruction_trace(module, function, invalid[n]);
      }
      exit(1);
    }
  }

  pn_arena_reset_to_mark(&module->temp_arena, mark);
  PN_END_TIME(CALCULATE_RESULT_VALUE_TYPES);
}

static void pn_basic_block_set_value_use(PNModule* module,
                                         PNFunction* function,
                                         PNBitSet* uses,
                                         PNValueId value_id) {
  if (value_id >= module->num_values) {
    value_id -= module->num_values;
    if (value_id >= function->num_args + function->num_constants) {
      pn_bitset_set(uses, value_id, PN_TRUE);
    }
  }
}

static void pn_basic_block_calculate_uses(PNModule* module,
                                          PNFunction* function,
                                          PNBasicBlock* bb) {
  PNArenaMark mark = pn_arena_mark(&module->temp_arena);
  PNBitSet uses;

  pn_bitset_init(&module->temp_arena, &uses, function->num_values);

  PNInstruction* inst = (PNInstruction*)bb->instructions;
  uint32_t n;
  for (n = 0; n < bb->num_instructions; ++n) {
    switch (inst->code) {
      case PN_FUNCTION_CODE_INST_BINOP: {
        PNInstructionBinop* i = (PNInstructionBinop*)inst;
        pn_basic_block_set_value_use(module, function, &uses, i->value0_id);
        pn_basic_block_set_value_use(module, function, &uses, i->value1_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_CAST: {
        PNInstructionCast* i = (PNInstructionCast*)inst;
        pn_basic_block_set_value_use(module, function, &uses, i->value_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_RET: {
        PNInstructionRet* i = (PNInstructionRet*)inst;
        if (i->value_id != PN_INVALID_VALUE_ID) {
          pn_basic_block_set_value_use(module, function, &uses, i->value_id);
        }
        break;
      }

      case PN_FUNCTION_CODE_INST_BR: {
        PNInstructionBr* i = (PNInstructionBr*)inst;
        if (i->value_id != PN_INVALID_VALUE_ID) {
          pn_basic_block_set_value_use(module, function, &uses, i->value_id);
        }
        break;
      }

      case PN_FUNCTION_CODE_INST_SWITCH: {
        PNInstructionSwitch* i = (PNInstructionSwitch*)inst;
        pn_basic_block_set_value_use(module, function, &uses, i->value_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_PHI: {
        PNInstructionPhi* i = (PNInstructionPhi*)inst;
        int32_t n;
        for (n = 0; n < i->num_incoming; ++n) {
          bb->phi_uses =
              pn_arena_realloc(&module->arena, bb->phi_uses,
                               sizeof(PNPhiUse) * (bb->num_phi_uses + 1));

          PNPhiUse* use = &bb->phi_uses[bb->num_phi_uses++];
          use->dest_value_id = i->result_value_id;
          use->incoming = i->incoming[n];
        }
      }

      case PN_FUNCTION_CODE_INST_ALLOCA: {
        PNInstructionAlloca* i = (PNInstructionAlloca*)inst;
        pn_basic_block_set_value_use(module, function, &uses, i->size_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_LOAD: {
        PNInstructionLoad* i = (PNInstructionLoad*)inst;
        pn_basic_block_set_value_use(module, function, &uses, i->src_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_STORE: {
        PNInstructionStore* i = (PNInstructionStore*)inst;
        pn_basic_block_set_value_use(module, function, &uses, i->dest_id);
        pn_basic_block_set_value_use(module, function, &uses, i->value_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_CMP2: {
        PNInstructionCmp2* i = (PNInstructionCmp2*)inst;
        pn_basic_block_set_value_use(module, function, &uses, i->value0_id);
        pn_basic_block_set_value_use(module, function, &uses, i->value1_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_VSELECT: {
        PNInstructionVselect* i = (PNInstructionVselect*)inst;
        pn_basic_block_set_value_use(module, function, &uses, i->cond_id);
        pn_basic_block_set_value_use(module, function, &uses, i->true_value_id);
        pn_basic_block_set_value_use(module, function, &uses,
                                     i->false_value_id);
        break;
      }

      case PN_FUNCTION_CODE_INST_CALL:
      case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
        PNInstructionCall* i = (PNInstructionCall*)inst;
        if (i->is_indirect) {
          pn_basic_block_set_value_use(module, function, &uses, i->callee_id);
        }

        uint32_t m;
        for (m = 0; m < i->num_args; ++m) {
          pn_basic_block_set_value_use(module, function, &uses, i->arg_ids[m]);
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
    inst = pn_instruction_next(inst);
  }

  bb->uses =
      pn_arena_alloc(&module->arena, sizeof(PNValueId) * uses.num_bits_set);

  for (n = 0; n < function->num_values; ++n) {
    if (pn_bitset_is_set(&uses, n)) {
      bb->uses[bb->num_uses++] = module->num_values + n;
    }
  }

  pn_arena_reset_to_mark(&module->temp_arena, mark);
}

static void pn_function_calculate_uses(PNModule* module, PNFunction* function) {
  PN_BEGIN_TIME(CALCULATE_USES);
  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    pn_basic_block_calculate_uses(module, function, &function->bbs[n]);
  }
  PN_END_TIME(CALCULATE_USES);
}

static void pn_basic_block_calculate_liveness_per_value(
    PNModule* module,
    PNFunction* function,
    PNLivenessState* state,
    PNBasicBlockId initial_bb_id,
    PNValueId rel_id) {
  PNValueId value_id = module->num_values + rel_id;

  /* Allocate enough space for any predecessor chain. num_bbs is always an
   * upper bound */
  PNArenaMark mark = pn_arena_mark(&module->temp_arena);
  PNBasicBlockId* bb_id_stack = pn_arena_alloc(
      &module->temp_arena, sizeof(PNBasicBlockId) * function->num_bbs);
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

  pn_arena_reset_to_mark(&module->temp_arena, mark);
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
    pn_bitset_set(&state->liveout[pred_bb_id], rel_id, PN_TRUE);
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
  PNArenaMark mark = pn_arena_mark(&module->temp_arena);

  PNLivenessState state;
  state.livein = (PNBitSet*)pn_arena_alloc(
      &module->temp_arena, sizeof(PNBitSet) * function->num_bbs);
  state.liveout = (PNBitSet*)pn_arena_alloc(
      &module->temp_arena, sizeof(PNBitSet) * function->num_bbs);

  uint32_t n;
  for (n = 0; n < function->num_bbs; ++n) {
    pn_bitset_init(&module->temp_arena, &state.livein[n], function->num_values);
    pn_bitset_init(&module->temp_arena, &state.liveout[n],
                   function->num_values);
  }

  for (n = function->num_bbs; n > 0; --n) {
    PNBasicBlockId bb_id = n - 1;
    pn_basic_block_calculate_liveness(module, function, &state, bb_id);
  }

  for (n = 0; n < function->num_bbs; ++n) {
    PNBasicBlock* bb = &function->bbs[n];
    uint32_t m;

    if (state.livein[n].num_bits_set) {
      bb->num_livein = 0;
      bb->livein = pn_arena_alloc(
          &module->arena, sizeof(PNValueId) * state.livein[n].num_bits_set);

      for (m = 0; m < function->num_values; ++m) {
        if (pn_bitset_is_set(&state.livein[n], m)) {
          bb->livein[bb->num_livein++] = module->num_values + m;
        }
      }
    }

    if (state.liveout[n].num_bits_set) {
      bb->num_liveout = 0;
      bb->liveout = pn_arena_alloc(
          &module->arena, sizeof(PNValueId) * state.liveout[n].num_bits_set);

      for (m = 0; m < function->num_values; ++m) {
        if (pn_bitset_is_set(&state.liveout[n], m)) {
          bb->liveout[bb->num_liveout++] = module->num_values + m;
        }
      }
    }
  }

  pn_arena_reset_to_mark(&module->temp_arena, mark);
  PN_END_TIME(CALCULATE_LIVENESS);
}

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

    bb->phi_assigns = pn_arena_alloc(&module->arena,
                                     sizeof(PNPhiAssign) * bb->num_phi_assigns);
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
      *out_value = pn_bitstream_read_vbr(reader->bs, op->num_bits);
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

static PNBool pn_record_try_read_uint16(PNRecordReader* reader,
                                        uint16_t* out_value) {
  uint32_t value;
  PNBool ret = pn_record_try_read_uint32(reader, &value);
  if (ret) {
    if (value >= 1 << 16) {
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

static uint32_t pn_record_read_uint32(PNRecordReader* reader,
                                      const char* name) {
  uint32_t value;
  if (!pn_record_try_read_uint32(reader, &value)) {
    PN_FATAL("unable to read %s.\n", name);
  }

  return value;
}

static float pn_record_read_float(PNRecordReader* reader, const char* name) {
  int32_t value;
  if (!pn_record_try_read_int32(reader, &value)) {
    PN_FATAL("unable to read %s.\n", name);
  }

  assert(sizeof(float) == sizeof(int32_t));
  float float_value;
  memcpy(&float_value, &value, sizeof(float));

  return float_value;
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
  assert(block_id < PN_MAX_BLOCK_IDS);
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

static void pn_block_info_context_append_abbrev(PNBlockInfoContext* context,
                                                PNBlockId block_id,
                                                PNBlockAbbrev* abbrev) {
  assert(block_id < PN_MAX_BLOCK_IDS);
  PNBlockAbbrevs* abbrevs = &context->block_abbrev_map[block_id];
  assert(abbrevs->num_abbrevs < PN_MAX_BLOCK_ABBREV);
  PNBlockAbbrev* dest_abbrev = &abbrevs->abbrevs[abbrevs->num_abbrevs++];
  *dest_abbrev = *abbrev;
}

static PNBlockAbbrev* pn_block_abbrev_read(PNBitStream* bs,
                                           PNBlockAbbrevs* abbrevs) {
  assert(abbrevs->num_abbrevs < PN_MAX_BLOCK_ABBREV);
  PNBlockAbbrev* abbrev = &abbrevs->abbrevs[abbrevs->num_abbrevs++];

  uint32_t num_ops = pn_bitstream_read_vbr(bs, 5);
  assert(num_ops < PN_MAX_BLOCK_ABBREV_OP);
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

static void pn_blockinfo_block_read(PNBlockInfoContext* context,
                                    PNBitStream* bs) {
  PN_BEGIN_TIME(BLOCKINFO_BLOCK_READ);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  (void)pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  PNBlockId block_id = -1;

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_TRACE(BLOCKINFO_BLOCK, "*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        PN_END_TIME(BLOCKINFO_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in blockinfo_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        PNBlockAbbrev* abbrev = pn_block_abbrev_read(bs, &abbrevs);
        pn_block_info_context_append_abbrev(context, block_id, abbrev);
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
            block_id = pn_record_read_int32(&reader, "block id");
            PN_TRACE(BLOCKINFO_BLOCK, "block id: %d\n", block_id);
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
}

static void pn_type_block_read(PNModule* module,
                               PNBlockInfoContext* context,
                               PNBitStream* bs) {
  PN_BEGIN_TIME(TYPE_BLOCK_READ);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num_words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_TYPE, &abbrevs);

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_TRACE(TYPE_BLOCK, "*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        PN_END_TIME(TYPE_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in type_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        pn_block_abbrev_read(bs, &abbrevs);
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
            uint32_t num_entries =
                pn_record_read_uint32(&reader, "num entries");
            (void)num_entries;
            PN_TRACE(TYPE_BLOCK, "type num entries: %d\n", num_entries);
            break;
          }

          case PN_TYPE_CODE_VOID: {
            PNTypeId type_id;
            PNType* type = pn_module_append_type(module, &type_id);
            type->code = PN_TYPE_CODE_VOID;
            PN_TRACE(TYPE_BLOCK, "%d: type void\n", type_id);
            break;
          }

          case PN_TYPE_CODE_FLOAT: {
            PNTypeId type_id;
            PNType* type = pn_module_append_type(module, &type_id);
            type->code = PN_TYPE_CODE_FLOAT;
            PN_TRACE(TYPE_BLOCK, "%d: type float\n", type_id);
            break;
          }

          case PN_TYPE_CODE_DOUBLE: {
            PNTypeId type_id;
            PNType* type = pn_module_append_type(module, &type_id);
            type->code = PN_TYPE_CODE_DOUBLE;
            PN_TRACE(TYPE_BLOCK, "%d: type double\n", type_id);
            break;
          }

          case PN_TYPE_CODE_INTEGER: {
            PNTypeId type_id;
            PNType* type = pn_module_append_type(module, &type_id);
            type->code = PN_TYPE_CODE_INTEGER;
            type->width = pn_record_read_int32(&reader, "width");
            PN_TRACE(TYPE_BLOCK, "%d: type integer %d\n", type_id, type->width);
            break;
          }

          case PN_TYPE_CODE_FUNCTION: {
            PNTypeId type_id;
            PNType* type = pn_module_append_type(module, &type_id);
            type->code = PN_TYPE_CODE_FUNCTION;
            type->is_varargs = pn_record_read_int32(&reader, "is_varargs");
            type->return_type = pn_record_read_int32(&reader, "return_type");
            type->num_args = 0;
            PN_TRACE(TYPE_BLOCK, "%d: type function is_varargs:%d ret:%d ",
                     type_id, type->is_varargs, type->return_type);

            PNTypeId arg_type_id;
            while (pn_record_try_read_uint16(&reader, &arg_type_id)) {
              assert(type->num_args < PN_ARRAY_SIZE(type->arg_types));
              type->arg_types[type->num_args] = arg_type_id;
              PN_TRACE(TYPE_BLOCK, "%d ", arg_type_id);
              type->num_args++;
            }
            PN_TRACE(TYPE_BLOCK, "\n");
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
}

static void pn_globalvar_block_read(PNModule* module,
                                    PNBlockInfoContext* context,
                                    PNBitStream* bs) {
  PN_BEGIN_TIME(GLOBALVAR_BLOCK_READ);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_GLOBALVAR, &abbrevs);

  PNGlobalVar* global_var = NULL;

  uint32_t num_global_vars = 0;
  PNInitializerId initializer_id = 0;

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_TRACE(GLOBALVAR_BLOCK, "*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        PN_END_TIME(GLOBALVAR_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in globalvar_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        pn_block_abbrev_read(bs, &abbrevs);
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
            assert(global_var_id < num_global_vars);

            global_var = &module->global_vars[global_var_id];
            global_var->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;
            global_var->is_constant =
                pn_record_read_int32(&reader, "is_constant") != 0;
            global_var->num_initializers = 1;
            global_var->initializers =
                pn_arena_alloc(&module->arena, sizeof(PNInitializer));
            initializer_id = 0;

            PNValueId value_id;
            PNValue* value = pn_module_append_value(module, &value_id);
            value->code = PN_VALUE_CODE_GLOBAL_VAR;
            value->type_id = pn_module_find_pointer_type(module);
            value->index = global_var_id;

            PN_TRACE(GLOBALVAR_BLOCK,
                     "%%%d. var. alignment:%d is_constant:%d\n", value_id,
                     global_var->alignment, global_var->is_constant);
            break;
          }

          case PN_GLOBALVAR_CODE_COMPOUND: {
            global_var->num_initializers =
                pn_record_read_int32(&reader, "num_initializers");
            global_var->initializers = pn_arena_realloc(
                &module->arena, global_var->initializers,
                global_var->num_initializers * sizeof(PNInitializer));

            PN_TRACE(GLOBALVAR_BLOCK, "  compound. num initializers: %d\n",
                     global_var->num_initializers);
            break;
          }

          case PN_GLOBALVAR_CODE_ZEROFILL: {
            assert(initializer_id < global_var->num_initializers);
            PNInitializer* initializer =
                &global_var->initializers[initializer_id++];
            initializer->code = code;
            initializer->num_bytes = pn_record_read_int32(&reader, "num_bytes");

            PN_TRACE(GLOBALVAR_BLOCK, "  zerofill. num_bytes: %d\n",
                     initializer->num_bytes);
            break;
          }

          case PN_GLOBALVAR_CODE_DATA: {
            assert(initializer_id < global_var->num_initializers);
            PNInitializer* initializer =
                &global_var->initializers[initializer_id++];
            initializer->code = code;

            /* TODO(binji): optimize */
            uint32_t capacity = 16;
            uint8_t* buffer = malloc(capacity);

            uint32_t num_bytes = 0;
            uint32_t value;
            while (pn_record_try_read_uint32(&reader, &value)) {
              if (value >= 256) {
                PN_FATAL("globalvar data out of range: %d\n", value);
              }

              if (num_bytes >= capacity) {
                capacity *= 2;
                buffer = realloc(buffer, capacity);
              }

              buffer[num_bytes++] = value;
            }

            /* TODO(binji): don't realloc down? */
            buffer = realloc(buffer, num_bytes);

            initializer->num_bytes = num_bytes;
            initializer->data = buffer;

            PN_TRACE(GLOBALVAR_BLOCK, "  data. num_bytes: %d\n", num_bytes);
            break;
          }

          case PN_GLOBALVAR_CODE_RELOC: {
            assert(initializer_id < global_var->num_initializers);
            PNInitializer* initializer =
                &global_var->initializers[initializer_id++];
            initializer->code = code;
            initializer->index = pn_record_read_int32(&reader, "reloc index");
            initializer->addend = 0;
            /* Optional */
            pn_record_try_read_int32(&reader, &initializer->addend);

            PN_TRACE(GLOBALVAR_BLOCK, "  reloc. index: %d addend: %d\n",
                     initializer->index, initializer->addend);
            break;
          }

          case PN_GLOBALVAR_CODE_COUNT: {
            num_global_vars =
                pn_record_read_uint32(&reader, "global var count");
            module->global_vars = pn_arena_alloc(
                &module->arena, num_global_vars * sizeof(PNGlobalVar));

            PN_TRACE(GLOBALVAR_BLOCK, "global var count: %d\n",
                     num_global_vars);
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
}

static void pn_value_symtab_block_read(PNModule* module,
                                       PNBlockInfoContext* context,
                                       PNBitStream* bs) {
  PN_BEGIN_TIME(VALUE_SYMTAB_BLOCK_READ);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_VALUE_SYMTAB, &abbrevs);

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_TRACE(VALUE_SYMTAB_BLOCK, "*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        PN_END_TIME(VALUE_SYMTAB_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in valuesymtab_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        pn_block_abbrev_read(bs, &abbrevs);
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
            char buffer[1024];
            char* p = &buffer[0];
            int32_t c;

            while (pn_record_try_read_int32(&reader, &c)) {
              assert(p - &buffer[0] < 1024);
              *p++ = c;
            }
            *p = 0;

            PNValue* value = pn_module_get_value(module, value_id);
            if (value->code == PN_VALUE_CODE_FUNCTION) {
              PNFunctionId function_id = value->index;
              PNFunction* function =
                  pn_module_get_function(module, function_id);
              strncpy(function->name, buffer, PN_MAX_FUNCTION_NAME);
            }

            PN_TRACE(VALUE_SYMTAB_BLOCK, "  entry: id:%d name:\"%s\"\n",
                     value_id, buffer);
            break;
          }

          case PN_VALUESYMBTAB_CODE_BBENTRY: {
            PNBasicBlockId bb_id = pn_record_read_int32(&reader, "bb_id");
            char buffer[1024];
            char* p = &buffer[0];
            int32_t c;

            while (pn_record_try_read_int32(&reader, &c)) {
              assert(p - &buffer[0] < 1024);
              *p++ = c;
            }
            *p = 0;

            (void)bb_id;
            PN_TRACE(VALUE_SYMTAB_BLOCK, "  bbentry: id:%d name:\"%s\"\n",
                     bb_id, buffer);
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
}

static void pn_constants_block_read(PNModule* module,
                                    PNFunction* function,
                                    PNBlockInfoContext* context,
                                    PNBitStream* bs) {
  PN_BEGIN_TIME(CONSTANTS_BLOCK_READ);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_CONSTANTS, &abbrevs);

  PNTypeId cur_type_id = -1;
  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_TRACE(CONSTANTS_BLOCK, "*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        PN_END_TIME(CONSTANTS_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in constants_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        pn_block_abbrev_read(bs, &abbrevs);
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
            PN_TRACE(CONSTANTS_BLOCK, "  constants settype %d\n", cur_type_id);
            break;

          case PN_CONSTANTS_CODE_UNDEF: {
            PNConstantId constant_id;
            PNConstant* constant =
                pn_function_append_constant(module, function, &constant_id);
            constant->code = code;
            constant->type_id = cur_type_id;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_CONSTANT;
            value->type_id = cur_type_id;
            value->index = constant_id;

            PN_TRACE(CONSTANTS_BLOCK, "  %%%d. undef\n", value_id);
            break;
          }

          case PN_CONSTANTS_CODE_INTEGER: {
            int32_t data = pn_decode_sign_rotated_value(
                pn_record_read_int32(&reader, "integer value"));

            PNConstantId constant_id;
            PNConstant* constant =
                pn_function_append_constant(module, function, &constant_id);
            constant->code = code;
            constant->type_id = cur_type_id;
            constant->int_value = data;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_CONSTANT;
            value->type_id = cur_type_id;
            value->index = constant_id;

            PN_TRACE(CONSTANTS_BLOCK, "  %%%d. integer %d\n", value_id, data);
            break;
          }

          case PN_CONSTANTS_CODE_FLOAT: {
            float data = pn_record_read_float(&reader, "float value");

            PNConstantId constant_id;
            PNConstant* constant =
                pn_function_append_constant(module, function, &constant_id);
            constant->code = code;
            constant->type_id = cur_type_id;
            constant->float_value = data;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_CONSTANT;
            value->type_id = cur_type_id;
            value->index = constant_id;

            PN_TRACE(CONSTANTS_BLOCK, "  %%%d. float %g\n", value_id, data);
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
    pn_function_trace_header(function, function_id);
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

    PN_TRACE(FUNCTION_BLOCK, "  %%%d. function arg %d\n", value_id, i);
  }

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
        pn_function_calculate_result_value_types(module, function);
        pn_function_calculate_uses(module, function);
        pn_function_calculate_pred_bbs(module, function);
        pn_function_calculate_phi_assigns(module, function);
        pn_function_calculate_liveness(module, function);
#if PN_TRACING
        /* Print the header if it wasn't printed above */
        PNBool print_header = !PN_IS_TRACE(FUNCTION_BLOCK);
        pn_function_trace(module, function, function_id, print_header);
#endif /* PN_TRACING */

        PN_TRACE(FUNCTION_BLOCK, "*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        PN_END_TIME(FUNCTION_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK: {
        uint32_t id = pn_bitstream_read_vbr(bs, 8);
        switch (id) {
          case PN_BLOCKID_CONSTANTS:
            PN_TRACE(FUNCTION_BLOCK, "*** SUBBLOCK CONSTANTS (%d)\n", id);
            pn_constants_block_read(module, function, context, bs);
            break;

          case PN_BLOCKID_VALUE_SYMTAB:
            PN_TRACE(FUNCTION_BLOCK, "*** SUBBLOCK VALUE_SYMTAB (%d)\n", id);
            pn_value_symtab_block_read(module, context, bs);
            break;

          default:
            PN_FATAL("bad block id %d\n", id);
        }
        break;
      }

      case PN_ENTRY_DEFINE_ABBREV: {
        pn_block_abbrev_read(bs, &abbrevs);
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
          function->bbs = pn_arena_allocz(
              &module->arena, sizeof(PNBasicBlock) * function->num_bbs);
          PN_TRACE(FUNCTION_BLOCK, "num bbs:%d\n", function->num_bbs);
          break;
        }

        if (prev_bb_id != cur_bb_id) {
          assert(cur_bb_id < function->num_bbs);
          prev_bb_id = cur_bb_id;
          cur_bb = &function->bbs[cur_bb_id];
          cur_bb->first_def_id = PN_INVALID_VALUE_ID;
          cur_bb->last_def_id = PN_INVALID_VALUE_ID;

          first_bb_value_id = pn_function_num_values(module, function);
        }

        switch (code) {
          case PN_FUNCTION_CODE_DECLAREBLOCKS:
            /* Handled above so we only print the basic block index when listing
             * instructions */
            break;

          case PN_FUNCTION_CODE_INST_BINOP: {
            PNInstructionId inst_id;
            PNInstructionBinop* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionBinop, module, function, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            /* Fix later, when all values are defined. */
            value->type_id = PN_INVALID_TYPE_ID;
            value->index = inst_id;

            inst->code = code;
            inst->result_value_id = value_id;
            inst->value0_id = pn_record_read_uint32(&reader, "value 0");
            inst->value1_id = pn_record_read_uint32(&reader, "value 1");
            inst->opcode = pn_record_read_int32(&reader, "opcode");
            inst->flags = 0;

            pn_context_fix_value_ids(context, rel_id, 2, &inst->value0_id,
                                     &inst->value1_id);

            /* optional */
            pn_record_try_read_int32(&reader, &inst->flags);
            break;
          }

          case PN_FUNCTION_CODE_INST_CAST: {
            PNInstructionId inst_id;
            PNInstructionCast* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionCast, module, function, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = inst_id;

            inst->code = code;
            inst->result_value_id = value_id;
            inst->value_id = pn_record_read_uint32(&reader, "value");
            inst->type_id = pn_record_read_uint32(&reader, "type_id");
            inst->opcode = pn_record_read_int32(&reader, "opcode");

            value->type_id = inst->type_id;

            pn_context_fix_value_ids(context, rel_id, 1, &inst->value_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_RET: {
            PNInstructionId inst_id;
            PNInstructionRet* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionRet, module, function, cur_bb, &inst_id);
            inst->code = code;
            inst->value_id = PN_INVALID_VALUE_ID;

            if (pn_record_try_read_uint16(&reader, &inst->value_id)) {
              pn_context_fix_value_ids(context, rel_id, 1, &inst->value_id);
            }

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_BR: {
            PNInstructionId inst_id;
            PNInstructionBr* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionBr, module, function, cur_bb, &inst_id);
            inst->code = code;
            inst->true_bb_id = pn_record_read_uint32(&reader, "true_bb");
            inst->false_bb_id = PN_INVALID_BLOCK_ID;

            pn_basic_block_list_append(&module->arena, &cur_bb->succ_bb_ids,
                                       &cur_bb->num_succ_bbs, inst->true_bb_id);

            if (pn_record_try_read_uint16(&reader, &inst->false_bb_id)) {
              inst->value_id = pn_record_read_uint32(&reader, "value");
              pn_context_fix_value_ids(context, rel_id, 1, &inst->value_id);

              pn_basic_block_list_append(&module->arena, &cur_bb->succ_bb_ids,
                                         &cur_bb->num_succ_bbs,
                                         inst->false_bb_id);
            }

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_SWITCH: {
            PNInstructionId inst_id;
            PNInstructionSwitch* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionSwitch, module, function, cur_bb, &inst_id);

            inst->type_id = pn_record_read_uint32(&reader, "type_id");
            inst->value_id = pn_record_read_uint32(&reader, "value");
            inst->default_bb_id = pn_record_read_uint32(&reader, "default bb");

            pn_basic_block_list_append(&module->arena, &cur_bb->succ_bb_ids,
                                       &cur_bb->num_succ_bbs,
                                       inst->default_bb_id);

            pn_context_fix_value_ids(context, rel_id, 1, &inst->value_id);

            inst->code = code;
            inst->num_cases = pn_record_read_int32(&reader, "num cases");
            inst->cases =
                pn_arena_alloc(&module->instruction_arena,
                               sizeof(PNSwitchCase) * inst->num_cases);

            int32_t c = 0;
            for (c = 0; c < inst->num_cases; ++c) {
              PNSwitchCase* switch_case = &inst->cases[c];
              switch_case->num_values =
                  pn_record_read_int32(&reader, "num values");
              switch_case->values = pn_arena_alloc(
                  &module->instruction_arena,
                  sizeof(PNSwitchCaseValue) * switch_case->num_values);

              int32_t i;
              for (i = 0; i < switch_case->num_values; ++i) {
                PNSwitchCaseValue* value = &switch_case->values[i];
                value->is_single = pn_record_read_int32(&reader, "is_single");
                value->low = pn_decode_sign_rotated_value(
                    pn_record_read_int32(&reader, "low"));
                if (!value->is_single) {
                  value->high = pn_decode_sign_rotated_value(
                      pn_record_read_int32(&reader, "high"));
                }
              }
              switch_case->bb_id = pn_record_read_int32(&reader, "bb");

              pn_basic_block_list_append(&module->arena, &cur_bb->succ_bb_ids,
                                         &cur_bb->num_succ_bbs,
                                         switch_case->bb_id);
            }

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_UNREACHABLE: {
            PNInstructionId inst_id;
            PNInstructionUnreachable* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionUnreachable, module, function, cur_bb, &inst_id);
            inst->code = code;
            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_PHI: {
            PNInstructionId inst_id;
            PNInstructionPhi* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionPhi, module, function, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = inst_id;

            inst->code = code;
            inst->result_value_id = value_id;
            inst->type_id = pn_record_read_int32(&reader, "type_id");
            inst->num_incoming = 0;

            value->type_id = inst->type_id;

            while (1) {
              PNBasicBlockId bb;
              PNValueId value;
              if (!pn_record_try_read_uint16(&reader, &value)) {
                break;
              }
              if (context->use_relative_ids) {
                value = value_id - (int32_t)pn_decode_sign_rotated_value(value);
              }

              if (!pn_record_try_read_uint16(&reader, &bb)) {
                PN_FATAL("unable to read phi bb index\n");
              }

              inst->incoming = pn_arena_realloc(
                  &module->instruction_arena, inst->incoming,
                  sizeof(PNPhiIncoming) * (inst->num_incoming + 1));

              PNPhiIncoming* incoming = &inst->incoming[inst->num_incoming];
              incoming->bb_id = bb;
              incoming->value_id = value;
              inst->num_incoming++;
            }
            break;
          }

          case PN_FUNCTION_CODE_INST_ALLOCA: {
            PNInstructionId inst_id;
            PNInstructionAlloca* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionAlloca, module, function, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->type_id = pn_module_find_pointer_type(module);
            value->index = inst_id;

            inst->code = code;
            inst->result_value_id = value_id;
            inst->size_id = pn_record_read_uint32(&reader, "size");
            inst->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;

            pn_context_fix_value_ids(context, rel_id, 1, &inst->size_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_LOAD: {
            PNInstructionId inst_id;
            PNInstructionLoad* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionLoad, module, function, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = inst_id;

            inst->code = code;
            inst->result_value_id = value_id;
            inst->src_id = pn_record_read_uint32(&reader, "src");
            inst->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;
            inst->type_id = pn_record_read_int32(&reader, "type_id");

            value->type_id = inst->type_id;

            pn_context_fix_value_ids(context, rel_id, 1, &inst->src_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_STORE: {
            PNInstructionId inst_id;
            PNInstructionStore* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionStore, module, function, cur_bb, &inst_id);

            inst->code = code;
            inst->dest_id = pn_record_read_uint32(&reader, "dest");
            inst->value_id = pn_record_read_uint32(&reader, "value");
            inst->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;

            pn_context_fix_value_ids(context, rel_id, 2, &inst->dest_id,
                                     &inst->value_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_CMP2: {
            PNInstructionId inst_id;
            PNInstructionCmp2* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionCmp2, module, function, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->type_id = pn_module_find_integer_type(module, 1);
            value->index = inst_id;

            inst->code = code;
            inst->result_value_id = value_id;
            inst->value0_id = pn_record_read_uint32(&reader, "value 0");
            inst->value1_id = pn_record_read_uint32(&reader, "value 1");
            inst->opcode = pn_record_read_int32(&reader, "opcode");

            pn_context_fix_value_ids(context, rel_id, 2, &inst->value0_id,
                                     &inst->value1_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_VSELECT: {
            PNInstructionId inst_id;
            PNInstructionVselect* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionVselect, module, function, cur_bb, &inst_id);

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            /* Fix later, when all values are defined. */
            value->type_id = PN_INVALID_TYPE_ID;
            value->index = inst_id;

            inst->code = code;
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
            PNInstructionForwardtyperef* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionForwardtyperef, module, function, cur_bb,
                &inst_id);

            inst->code = code;
            inst->value_id = pn_record_read_int32(&reader, "value");
            inst->type_id = pn_record_read_int32(&reader, "type");
            break;
          }

          case PN_FUNCTION_CODE_INST_CALL:
          case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
            PNInstructionId inst_id;
            PNInstructionCall* inst = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionCall, module, function, cur_bb, &inst_id);

            inst->code = code;
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
              PNFunction* called_function =
                  pn_module_get_function(module, inst->callee_id);
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

              inst->arg_ids =
                  pn_arena_realloc(&module->instruction_arena, inst->arg_ids,
                                   sizeof(PNValueId) * (inst->num_args + 1));

              inst->arg_ids[inst->num_args] = arg;
              inst->num_args++;
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
            PN_TRACE(MODULE_BLOCK, "*** SUBBLOCK BLOCKINFO (%d)\n", id);
            pn_blockinfo_block_read(context, bs);
            break;
          case PN_BLOCKID_TYPE:
            PN_TRACE(MODULE_BLOCK, "*** SUBBLOCK TYPE (%d)\n", id);
            pn_type_block_read(module, context, bs);
            break;
          case PN_BLOCKID_GLOBALVAR:
            PN_TRACE(MODULE_BLOCK, "*** SUBBLOCK GLOBALVAR (%d)\n", id);
            pn_globalvar_block_read(module, context, bs);
            break;
          case PN_BLOCKID_VALUE_SYMTAB:
            PN_TRACE(MODULE_BLOCK, "*** SUBBLOCK VALUE_SYMTAB (%d)\n", id);
            pn_value_symtab_block_read(module, context, bs);
            break;
          case PN_BLOCKID_FUNCTION: {
            PN_TRACE(MODULE_BLOCK, "*** SUBBLOCK FUNCTION (%d)\n", id);
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

      case PN_ENTRY_DEFINE_ABBREV:
        pn_block_abbrev_read(bs, &abbrevs);
        break;

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
            PN_TRACE(MODULE_BLOCK, "module version: %d\n", module->version);
            break;
          }

          case PN_MODULE_CODE_FUNCTION: {
            module->functions = pn_arena_realloc(
                &module->arena, module->functions,
                sizeof(PNFunction) * (module->num_functions + 1));
            PNFunctionId function_id = module->num_functions++;
            PNFunction* function = &module->functions[function_id];

            function->type_id = pn_record_read_int32(&reader, "type_id");
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
            assert(function_type->code == PN_TYPE_CODE_FUNCTION);
            function->num_args = function_type->num_args;

            PNValueId value_id;
            PNValue* value = pn_module_append_value(module, &value_id);
            value->code = PN_VALUE_CODE_FUNCTION;
            value->type_id = function->type_id;
            value->index = function_id;

            PN_TRACE(MODULE_BLOCK,
                     "%%%d. module function: "
                     "(type:%d,cc:%d,is_proto:%d,linkage:%d)\n",
                     value_id, function->type_id, function->calling_convention,
                     function->is_proto, function->linkage);
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

uint32_t pn_max_num_constants(PNModule* module) {
  uint32_t result = 0;
  uint32_t n;
  for (n = 0; n < module->num_functions; ++n) {
    if (module->functions[n].num_constants > result) {
      result = module->functions[n].num_constants;
    }
  }

  return result;
}

uint32_t pn_max_num_values(PNModule* module) {
  uint32_t result = 0;
  uint32_t n;
  for (n = 0; n < module->num_functions; ++n) {
    if (module->functions[n].num_values > result) {
      result = module->functions[n].num_values;
    }
  }

  return result;
}

uint32_t pn_max_num_bbs(PNModule* module) {
  uint32_t result = 0;
  uint32_t n;
  for (n = 0; n < module->num_functions; ++n) {
    if (module->functions[n].num_bbs > result) {
      result = module->functions[n].num_bbs;
    }
  }

  return result;
}

enum {
  PN_FLAG_VERBOSE,
  PN_FLAG_HELP,
#if PN_TRACING
  PN_FLAG_TRACE_ALL,
  PN_FLAG_TRACE_BLOCK,
#define PN_TRACE_FLAGS(name, flag) PN_FLAG_TRACE_##name,
  PN_FOREACH_TRACE(PN_TRACE_FLAGS)
#undef PN_TRACE_FLAGS
#endif /* PN_TRACING */
  PN_FLAG_PRINT_ALL,
#if PN_TIMERS
      PN_FLAG_PRINT_TIME,
#endif /* PN_TIMERS */
  PN_FLAG_PRINT_STATS,
  PN_NUM_FLAGS
};

static struct option long_options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
#if PN_TRACING
    {"trace-all", no_argument, NULL, 't'},
    {"trace-block", no_argument, NULL, 0},
#define PN_TRACE_FLAGS(name, flag) \
      {"trace-" flag, no_argument, NULL, 0},
    PN_FOREACH_TRACE(PN_TRACE_FLAGS)
#undef PN_TRACE_FLAGS
#endif /* PN_TRACING */
    {"print-all", no_argument, NULL, 'p'},
#if PN_TIMERS
        {"print-time", no_argument, NULL, 0},
#endif /* PN_TIMERS */
    {"print-stats", no_argument, NULL, 0},
    {NULL, 0, NULL, 0},
};

PN_STATIC_ASSERT(PN_NUM_FLAGS + 1 == PN_ARRAY_SIZE(long_options));

void pn_options_parse(int argc, char** argv) {
  int c;
  int option_index;
  while (1) {
    c = getopt_long(argc, argv, "vhtp", long_options, &option_index);
    if (c == -1) {
      break;
    }

redo_switch:
    switch (c) {
      case 0:
        c = long_options[option_index].val;
        if (c) {
          goto redo_switch;
        }

        switch (option_index) {
          case PN_FLAG_VERBOSE: 
          case PN_FLAG_HELP:
#if PN_TRACING
          case PN_FLAG_TRACE_ALL:
#endif /* PN_TRACING */
          case PN_FLAG_PRINT_ALL:
            /* Handled above by goto */
            PN_UNREACHABLE();

#if PN_TRACING
          case PN_FLAG_TRACE_BLOCK:
            g_pn_trace_BLOCKINFO_BLOCK = PN_TRUE;
            g_pn_trace_TYPE_BLOCK = PN_TRUE;
            g_pn_trace_GLOBALVAR_BLOCK = PN_TRUE;
            g_pn_trace_VALUE_SYMTAB_BLOCK = PN_TRUE;
            g_pn_trace_CONSTANTS_BLOCK = PN_TRUE;
            g_pn_trace_FUNCTION_BLOCK = PN_TRUE;
            g_pn_trace_MODULE_BLOCK = PN_TRUE;
            break;

#define PN_TRACE_OPTIONS(name, flag)          \
  case PN_FLAG_TRACE_##name: \
    g_pn_trace_##name = PN_TRUE;              \
    break;
            PN_FOREACH_TRACE(PN_TRACE_OPTIONS);
#undef PN_TRACE_OPTIONS

#endif /* PN_TRACING */

#if PN_TIMERS
          case PN_FLAG_PRINT_TIME:
            g_pn_print_time = PN_TRUE;
            break;
#endif /* PN_TIMERS */

          case PN_FLAG_PRINT_STATS:
            g_pn_print_stats = PN_TRUE;
            break;

            break;
        }
        break;

      case 'v':
        g_pn_verbose++;
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

      case 'h': {
        fprintf(stderr, "usage: %s [option] filename\n", argv[0]);
        fprintf(stderr, "options:\n");
        int i = 0;
        for (; long_options[i].name; ++i) {
          if (long_options[i].val) {
            fprintf(stderr, "  -%c, --%s\n", long_options[i].val,
                    long_options[i].name);
          } else {
            fprintf(stderr, "      --%s\n", long_options[i].name);
          }
        }
        exit(0);
      }

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
    /* TODO(binji): remove default filename */
    g_pn_filename = "simple.pexe";
  }

#if PN_TRACING
  /* Handle flag dependencies */
  if (g_pn_trace_INSTRUCTIONS) {
    g_pn_trace_BASIC_BLOCKS = PN_TRUE;
  }

  if (g_pn_trace_BASIC_BLOCKS) {
    g_pn_trace_FUNCTION_BLOCK = PN_TRUE;
  }
#endif /* PN_TRACING */
}

int main(int argc, char** argv) {
  PN_BEGIN_TIME(TOTAL);
  pn_options_parse(argc, argv);
  PN_BEGIN_TIME(FILE_READ);
  FILE* f = fopen(g_pn_filename, "r");
  if (!f) {
    PN_FATAL("unable to read %s\n", g_pn_filename);
  }

  fseek(f, 0, SEEK_END);
  size_t fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  uint8_t* data = malloc(fsize);

  size_t read_size = fread(data, 1, fsize, f);
  if (read_size != fsize) {
    PN_FATAL("unable to read data from file\n");
  }

  fclose(f);
  PN_END_TIME(FILE_READ);

  PNBitStream bs;
  pn_bitstream_init(&bs, data, fsize);
  pn_header_read(&bs);

  PNModule* module = calloc(1, sizeof(PNModule));
  PNBlockInfoContext context = {};
  pn_arena_init(&module->arena, PN_ARENA_SIZE);
  pn_arena_init(&module->value_arena, PN_VALUE_ARENA_SIZE);
  pn_arena_init(&module->instruction_arena, PN_INSTRUCTION_ARENA_SIZE);
  pn_arena_init(&module->temp_arena, PN_TEMP_ARENA_SIZE);

  uint32_t entry = pn_bitstream_read(&bs, 2);
  PN_TRACE(MODULE_BLOCK, "entry: %d\n", entry);
  if (entry != PN_ENTRY_SUBBLOCK) {
    PN_FATAL("expected subblock at top-level\n");
  }

  PNBlockId block_id = pn_bitstream_read_vbr(&bs, 8);
  assert(block_id == PN_BLOCKID_MODULE);
  pn_module_block_read(module, &context, &bs);
  PN_TRACE(MODULE_BLOCK, "done\n");
  PN_END_TIME(TOTAL);

#if PN_TIMERS
  if (g_pn_print_time) {
    printf("-----------------\n");
#define PN_PRINT_TIMER(name)                                          \
  struct timespec* timer_##name = &g_pn_timer_times[PN_TIMER_##name]; \
  printf("timer %-30s: %f sec (%%%.0f)\n", #name,                     \
         PNTimespecToDouble(timer_##name),                            \
         100 * PNTimespecToDouble(timer_##name) /                     \
             PNTimespecToDouble(timer_TOTAL));
    PN_FOREACH_TIMER(PN_PRINT_TIMER);
  }
#endif /* PN_TIMERS */

  if (g_pn_print_stats) {
    printf("-----------------\n");
    printf("num_types: %u\n", module->num_types);
    printf("num_functions: %u\n", module->num_functions);
    printf("num_global_vars: %u\n", module->num_global_vars);
    printf("max num_constants: %u\n", pn_max_num_constants(module));
    printf("max num_values: %u\n", pn_max_num_values(module));
    printf("max num_bbs: %u\n", pn_max_num_bbs(module));
    printf("arena: %u\n", module->arena.size);
    printf("value arena: %u\n", module->value_arena.size);
    printf("instruction arena: %u\n", module->instruction_arena.size);
  }

  return 0;
}
