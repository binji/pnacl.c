/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TODO(binji): handle variable sizes */
#define PN_ARENA_SIZE (4*1024*1024)
#define PN_VALUE_ARENA_SIZE (1*1024*1024)
#define PN_INSTRUCTION_ARENA_SIZE (32*1024*1024)
#define PN_MAX_BLOCK_ABBREV_OP 10
#define PN_MAX_BLOCK_ABBREV 100
#define PN_MAX_FUNCTIONS 30000
#define PN_MAX_FUNCTION_ARGS 15
#define PN_MAX_FUNCTION_NAME 256

#define PN_FALSE 0
#define PN_TRUE 1

#define PN_INVALID_VALUE_ID ((PNValueId)~0)
#define PN_INVALID_BLOCK_ID ((PNBlockId)~0)

#if 0
#define TRACE(...) (void)0
#else
#define TRACE(...) printf(__VA_ARGS__)
#endif
#define ERROR(...) fprintf(stderr, __VA_ARGS__)
#define FATAL(...)      \
  do {                  \
    ERROR(__VA_ARGS__); \
    exit(1);            \
  } while (0)

#define PN_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef int PNBool;
typedef uint32_t PNTypeId;
typedef uint32_t PNValueId;
typedef uint32_t PNFunctionId;
typedef uint32_t PNConstantId;
typedef uint32_t PNGlobalVarId;
typedef uint32_t PNInitializerId;
typedef uint32_t PNInstructionId;
typedef uint32_t PNBlockId;

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

typedef enum PNBlockID {
  PN_BLOCKID_BLOCKINFO = 0,
  PN_BLOCKID_MODULE = 8,
  PN_BLOCKID_CONSTANTS = 11,
  PN_BLOCKID_FUNCTION = 12,
  PN_BLOCKID_VALUE_SYMTAB = 14,
  PN_BLOCKID_TYPE = 17,
  PN_BLOCKID_GLOBALVAR = 19,
  PN_MAX_BLOCK_IDS,
} PNBlockID;

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
  PNBool is_single;
  int32_t low;
  int32_t high;
} PNSwitchCaseValue;

typedef struct PNSwitchCase {
  PNBlockId bb_id;
  uint32_t num_values;
  PNSwitchCaseValue* values;
} PNSwitchCase;

typedef struct PNPhiIncoming {
  PNBlockId bb_id;
  PNValueId value_id;
} PNPhiIncoming;

typedef struct PNInstructionBinop {
  PNFunctionCode code;
  PNValueId value_id0;
  PNValueId value_id1;
  PNBinOp opcode;
  int32_t flags;
} PNInstructionBinop;

typedef struct PNInstructionCast {
  PNFunctionCode code;
  PNValueId value_id;
  PNTypeId type_id;
  PNCast opcode;
} PNInstructionCast;

typedef struct PNInstructionRet {
  PNFunctionCode code;
  PNValueId value_id; /* Or PN_INVALID_VALUE_ID */
} PNInstructionRet;

typedef struct PNInstructionBr {
  PNFunctionCode code;
  PNBlockId true_bb_id;
  PNBlockId false_bb_id; /* Or PN_INVALID_BLOCK_ID */
  PNValueId value_id;    /* Or PN_INVALID_VALUE_ID */
} PNInstructionBr;

typedef struct PNInstructionSwitch {
  PNFunctionCode code;
  PNTypeId type_id;
  PNValueId value_id;
  PNBlockId default_bb_id;
  uint32_t num_cases;
  PNSwitchCase* cases;
} PNInstructionSwitch;

typedef struct PNInstructionUnreachable {
  PNFunctionCode code;
} PNInstructionUnreachable;

typedef struct PNInstructionPhi {
  PNFunctionCode code;
  PNTypeId type_id;
  uint32_t num_incoming;
  PNPhiIncoming* incoming;
} PNInstructionPhi;

typedef struct PNInstructionAlloca {
  PNFunctionCode code;
  uint32_t size;
  uint32_t alignment;
} PNInstructionAlloca;

typedef struct PNInstructionLoad {
  PNFunctionCode code;
  PNValueId src_id;
  PNTypeId type_id;
  uint32_t alignment;
} PNInstructionLoad;

typedef struct PNInstructionStore {
  PNFunctionCode code;
  PNValueId dest_id;
  PNValueId value_id;
  uint32_t alignment;
} PNInstructionStore;

typedef struct PNInstructionCmp2 {
  PNFunctionCode code;
  PNValueId value_id0;
  PNValueId value_id1;
  PNCmp2 opcode;
} PNInstructionCmp2;

typedef struct PNInstructionVselect {
  PNFunctionCode code;
  PNValueId cond_id;
  PNValueId true_value_id;
  PNValueId false_value_id;
} PNInstructionVselect;

typedef struct PNInstructionCall {
  PNFunctionCode code;
  PNBool is_indirect;
  PNBool is_tail_call;
  uint32_t calling_convention;
  PNValueId callee_id;
  PNTypeId return_type_id;
  uint32_t num_args;
  PNValueId arg_ids[PN_MAX_FUNCTION_ARGS];
} PNInstructionCall;

typedef struct PNFunction {
  char name[PN_MAX_FUNCTION_NAME];
  PNTypeId type_id;
  uint32_t calling_convention;
  PNBool is_proto;
  uint32_t linkage;
  uint32_t num_instructions;
  void* instructions;
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
  uint32_t alignment;
  PNBool is_constant;
  uint32_t num_initializers;
  PNInitializer* initializers;
} PNGlobalVar;

typedef struct PNModule {
  uint32_t version;
  uint32_t num_functions;
  PNFunction functions[PN_MAX_FUNCTIONS];
  uint32_t num_types;
  PNType* types;
  uint32_t num_constants;
  PNConstant* constants;
  uint32_t num_global_vars;
  PNGlobalVar* global_vars;
} PNModule;

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
   *   PN_VALUE_CODE_CONSTANT -> PNModule.constants
   *   PN_VALUE_CODE_FUNCTION_ARG -> function argument index
   *   PN_VALUE_CODE_LOCAL_VAR -> PNBlockInfoContext.values
   */
  uint32_t index;
} PNValue;

typedef struct PNBlockInfoContext {
  uint32_t num_abbrevs;
  PNBlockAbbrevs block_abbrev_map[PN_MAX_BLOCK_IDS];
  uint32_t num_values;
  PNValue* values;
  PNBool use_relative_ids;
  PNModule* module;
  PNArena arena;
  PNArena value_arena;
  PNArena instruction_arena;
} PNBlockInfoContext;

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
    FATAL("Arena exhausted. Requested: %u, avail: %u, capacity: %u\n", size,
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
      FATAL(
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

static const char* pn_binop_get_name(uint32_t op) {
  const char* names[] = {
      "add", "sub", "mul", "udiv", "sdiv", "urem", "srem", "shl", "lshr",
      "ashr", "and", "or", "xor"
  };

  if (op >= PN_ARRAY_SIZE(names)) {
    FATAL("Invalid op: %u\n", op);
  }

  return names[op];
}

static const char* pn_cast_get_name(uint32_t op) {
  const char* names[] = {
      "trunc", "zext", "sext", "fptoui", "fptosi", "uitofp", "sitofp",
      "fptrunc", "fpext", NULL, NULL, "bitcast"
  };

  if (op >= PN_ARRAY_SIZE(names)) {
    FATAL("Invalid op: %u\n", op);
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
    FATAL("Invalid op: %u\n", op);
  }

  return names[op];
}

static uint32_t pn_decode_char6(uint32_t value) {
  const char data[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._";
  if (value >= PN_ARRAY_SIZE(data)) {
    FATAL("Invalid char6 value: %u\n", value);
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

static PNType* pn_context_get_type(PNBlockInfoContext* context,
                                   PNTypeId type_id) {
  if (type_id < 0 || type_id >= context->module->num_types) {
    FATAL("accessing invalid type %d (max %d)", type_id,
          context->module->num_types);
  }

  return &context->module->types[type_id];
}

static PNType* pn_context_append_type(PNBlockInfoContext* context,
                                      PNTypeId* out_type_id) {
  PNArena* arena = &context->arena;
  *out_type_id = context->module->num_types;
  uint32_t new_size = sizeof(PNType) * (context->module->num_types + 1);
  context->module->types =
      pn_arena_realloc(arena, context->module->types, new_size);

  context->module->num_types++;
  return &context->module->types[*out_type_id];
}

static PNFunction* pn_context_get_function(PNBlockInfoContext* context,
                                           PNFunctionId function_id) {
  if (function_id < 0 || function_id >= context->module->num_functions) {
    FATAL("accessing invalid function %d (max %d)", function_id,
          context->module->num_functions);
  }

  return &context->module->functions[function_id];
}

static PNFunction* pn_context_append_function(PNBlockInfoContext* context,
                                              PNFunctionId* out_function_id) {
  *out_function_id = context->module->num_functions;
  if (*out_function_id >= PN_ARRAY_SIZE(context->module->functions)) {
    FATAL("too many functions: %d\n", *out_function_id);
  }

  context->module->num_functions++;
  return &context->module->functions[*out_function_id];
}

static PNConstant* pn_context_get_constant(PNBlockInfoContext* context,
                                           PNConstantId constant_id) {
  if (constant_id < 0 || constant_id >= context->module->num_constants) {
    FATAL("accessing invalid constant %d (max %d)", constant_id,
          context->module->num_constants);
  }

  return &context->module->constants[constant_id];
}

static PNConstant* pn_context_append_constant(PNBlockInfoContext* context,
                                              PNConstantId* out_constant_id) {
  PNArena* arena = &context->arena;
  *out_constant_id = context->module->num_constants;
  uint32_t new_size = sizeof(PNConstant) * (context->module->num_constants + 1);
  context->module->constants =
      pn_arena_realloc(arena, context->module->constants, new_size);

  context->module->num_constants++;
  return &context->module->constants[*out_constant_id];
}

static PNGlobalVar* pn_context_get_global_var(PNBlockInfoContext* context,
                                              PNGlobalVarId global_var_id) {
  if (global_var_id < 0 || global_var_id >= context->module->num_global_vars) {
    FATAL("accessing invalid global_var %d (max %d)", global_var_id,
          context->module->num_global_vars);
  }

  return &context->module->global_vars[global_var_id];
}

static PNValue* pn_context_get_value(PNBlockInfoContext* context,
                                     PNValueId value_id) {
  if (value_id < 0 || value_id >= context->num_values) {
    FATAL("accessing invalid value %d (max %d)", value_id, context->num_values);
  }

  return &context->values[value_id];
}

static PNValue* pn_context_append_value(PNBlockInfoContext* context,
                                        PNValueId* out_value_id) {
  PNArena* arena = &context->value_arena;
  *out_value_id = context->num_values;
  uint32_t new_size = sizeof(PNValue) * (context->num_values + 1);
  context->values = pn_arena_realloc(arena, context->values, new_size);

  context->num_values++;
  return &context->values[*out_value_id];
}

static uint32_t g_num_instructions = 0;

static void* pn_function_append_instruction(
    PNBlockInfoContext* context,
    PNFunction* function,
    uint32_t instruction_size,
    PNInstructionId* out_instruction_id) {
  g_num_instructions++;

  PNArena* arena = &context->instruction_arena;

  *out_instruction_id = function->num_instructions++;
  return pn_arena_alloc(arena, instruction_size);
}

#define PN_FUNCTION_APPEND_INSTRUCTION(type, context, function, id) \
  (type*) pn_function_append_instruction(context, function, sizeof(type), id)

static const char* pn_type_describe(PNBlockInfoContext* context,
                                    PNTypeId type_id) {
  PNType* type = pn_context_get_type(context, type_id);
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
        default: {
          static char buffer[100];
          snprintf(buffer, 100, "badInteger%d", type->width);
          return &buffer[0];
        }
      }
    case PN_TYPE_CODE_FLOAT:
      return "float";

    case PN_TYPE_CODE_DOUBLE:
      return "double";

    case PN_TYPE_CODE_FUNCTION: {
      static char buffer[2048];
      strcpy(buffer, pn_type_describe(context, type->return_type));
      strcat(buffer, "(");
      uint32_t i;
      for (i = 0; i < type->num_args; ++i) {
        if (i != 0) {
          strcat(buffer, ",");
        }
        strcat(buffer, pn_type_describe(context, type->arg_types[i]));
      }
      strcat(buffer, ")");
      return buffer;
    }

    default:
      return "<unknown>";
  }
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
          FATAL("bad encoding for array element: %d\n", elt_op->encoding);
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
      FATAL("bad encoding: %d\n", op->encoding);
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

static PNBool pn_record_try_read_int32(PNRecordReader* reader,
                                       int32_t* out_value) {
  uint32_t value;
  PNBool ret = pn_record_try_read_uint32(reader, &value);
  if (ret) {
    *out_value = value;
  }
  return ret;
}

static PNBool pn_record_try_read_value_id(PNRecordReader* reader,
                                          PNValueId* out_value_id,
                                          PNBool use_relative_ids,
                                          PNValueId rel_value_id) {
  uint32_t value;
  if (!pn_record_try_read_uint32(reader, &value)) {
    return PN_FALSE;
  }

  if (use_relative_ids) {
    *out_value_id = rel_value_id - value;
  } else {
    *out_value_id = value;
  }

  return PN_TRUE;
}

static int32_t pn_record_read_int32(PNRecordReader* reader, const char* name) {
  int32_t value;
  if (!pn_record_try_read_int32(reader, &value)) {
    FATAL("unable to read %s.\n", name);
  }

  return value;
}

static uint32_t pn_record_read_uint32(PNRecordReader* reader,
                                      const char* name) {
  uint32_t value;
  if (!pn_record_try_read_uint32(reader, &value)) {
    FATAL("unable to read %s.\n", name);
  }

  return value;
}

static PNValueId pn_record_read_value_id(PNRecordReader* reader,
                                         const char* name,
                                         PNBool use_relative_ids,
                                         PNValueId rel_value_id) {
  PNValueId value;
  if (!pn_record_try_read_value_id(reader, &value, use_relative_ids,
                                   rel_value_id)) {
    FATAL("unable to read %s.\n", name);
  }

  return value;
}

static void pn_record_reader_finish(PNRecordReader* reader) {
  int count = 0;
  uint32_t dummy;
  while (pn_record_try_read_uint32(reader, &dummy)) {
    ++count;
  }
  if (count) {
    TRACE("pn_record_reader_finish skipped %d values.\n", count);
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
                FATAL("bad encoding for array element: %d\n", elt_op->encoding);
            }
          }
          break;
        }

        case PN_ENCODING_CHAR6:
        case PN_ENCODING_BLOB:
          /* Nothing */
          break;

        default:
          FATAL("bad encoding: %d\n", op->encoding);
      }
    }
  }

  return abbrev;
}

static void pn_blockinfo_block_read(PNBlockInfoContext* context,
                                    PNBitStream* bs) {
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  (void)pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  int32_t block_id = -1;

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        TRACE("*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        return;

      case PN_ENTRY_SUBBLOCK:
        FATAL("unexpected subblock in blockinfo_block\n");

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
            TRACE("block id: %d\n", block_id);
            break;

          case PN_BLOCKINFO_CODE_BLOCKNAME:
            TRACE("block name\n");
            break;

          case PN_BLOCKINFO_CODE_SETRECORDNAME:
            TRACE("block record name\n");
            break;

          default:
            FATAL("bad record code: %d.\n", code);
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
}

static void pn_type_block_read(PNBlockInfoContext* context, PNBitStream* bs) {
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num_words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_TYPE, &abbrevs);

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        TRACE("*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        return;

      case PN_ENTRY_SUBBLOCK:
        FATAL("unexpected subblock in type_block\n");

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
            TRACE("type num entries: %d\n", num_entries);
            break;
          }

          case PN_TYPE_CODE_VOID: {
            PNTypeId type_id;
            PNType* type = pn_context_append_type(context, &type_id);
            type->code = PN_TYPE_CODE_VOID;
            TRACE("%d: type void\n", type_id);
            break;
          }

          case PN_TYPE_CODE_FLOAT: {
            PNTypeId type_id;
            PNType* type = pn_context_append_type(context, &type_id);
            type->code = PN_TYPE_CODE_FLOAT;
            TRACE("%d: type float\n", type_id);
            break;
          }

          case PN_TYPE_CODE_DOUBLE: {
            PNTypeId type_id;
            PNType* type = pn_context_append_type(context, &type_id);
            type->code = PN_TYPE_CODE_DOUBLE;
            TRACE("%d: type double\n", type_id);
            break;
          }

          case PN_TYPE_CODE_INTEGER: {
            PNTypeId type_id;
            PNType* type = pn_context_append_type(context, &type_id);
            type->code = PN_TYPE_CODE_INTEGER;
            type->width = pn_record_read_int32(&reader, "width");
            TRACE("%d: type integer %d\n", type_id, type->width);
            break;
          }

          case PN_TYPE_CODE_FUNCTION: {
            PNTypeId type_id;
            PNType* type = pn_context_append_type(context, &type_id);
            type->code = PN_TYPE_CODE_FUNCTION;
            type->is_varargs = pn_record_read_int32(&reader, "is_varargs");
            type->return_type = pn_record_read_int32(&reader, "return_type");
            type->num_args = 0;
            TRACE("%d: type function is_varargs:%d ret:%d ", type_id,
                  type->is_varargs, type->return_type);

            PNTypeId arg_type_id;
            while (pn_record_try_read_uint32(&reader, &arg_type_id)) {
              assert(type->num_args < PN_ARRAY_SIZE(type->arg_types));
              type->arg_types[type->num_args] = arg_type_id;
              TRACE("%d ", arg_type_id);
              type->num_args++;
            }
            TRACE("\n");
            break;
          }

          default:
            FATAL("bad record code: %d.\n", code);
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
}

static void pn_globalvar_block_read(PNBlockInfoContext* context,
                                    PNBitStream* bs) {
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
        TRACE("*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        return;

      case PN_ENTRY_SUBBLOCK:
        FATAL("unexpected subblock in globalvar_block\n");

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
            PNGlobalVarId global_var_id = context->module->num_global_vars++;
            assert(global_var_id < num_global_vars);

            global_var = &context->module->global_vars[global_var_id];
            global_var->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;
            global_var->is_constant =
                pn_record_read_int32(&reader, "is_constant") != 0;
            global_var->num_initializers = 1;
            global_var->initializers =
                pn_arena_alloc(&context->arena, sizeof(PNInitializer));
            initializer_id = 0;

            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_GLOBAL_VAR;
            value->index = global_var_id;

            TRACE("%%%d. var. alignment:%d is_constant:%d\n", value_id,
                  global_var->alignment, global_var->is_constant);
            break;
          }

          case PN_GLOBALVAR_CODE_COMPOUND: {
            global_var->num_initializers =
                pn_record_read_int32(&reader, "num_initializers");
            global_var->initializers = pn_arena_realloc(
                &context->arena, global_var->initializers,
                global_var->num_initializers * sizeof(PNInitializer));

            TRACE("  compound. num initializers: %d\n",
                  global_var->num_initializers);
            break;
          }

          case PN_GLOBALVAR_CODE_ZEROFILL: {
            assert(initializer_id < global_var->num_initializers);
            PNInitializer* initializer =
                &global_var->initializers[initializer_id++];
            initializer->code = code;
            initializer->num_bytes = pn_record_read_int32(&reader, "num_bytes");

            TRACE("  zerofill. num_bytes: %d\n", initializer->num_bytes);
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
                FATAL("globalvar data out of range: %d\n", value);
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

            TRACE("  data. num_bytes: %d\n", num_bytes);
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

            TRACE("  reloc. index: %d addend: %d\n", initializer->index,
                  initializer->addend);
            break;
          }

          case PN_GLOBALVAR_CODE_COUNT: {
            num_global_vars =
                pn_record_read_uint32(&reader, "global var count");
            context->module->global_vars = pn_arena_alloc(
                &context->arena, num_global_vars * sizeof(PNGlobalVar));

            TRACE("global var count: %d\n", num_global_vars);
            break;
          }

          default:
            FATAL("bad record code: %d.\n", code);
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
}

static void pn_value_symtab_block_read(PNBlockInfoContext* context,
                                       PNBitStream* bs) {
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_VALUE_SYMTAB, &abbrevs);

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        TRACE("*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        return;

      case PN_ENTRY_SUBBLOCK:
        FATAL("unexpected subblock in valuesymtab_block\n");

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

            PNValue* value = pn_context_get_value(context, value_id);
            if (value->code == PN_VALUE_CODE_FUNCTION) {
              PNFunctionId function_id = value->index;
              PNFunction* function =
                  pn_context_get_function(context, function_id);
              strncpy(function->name, buffer, PN_MAX_FUNCTION_NAME);
            }

            TRACE("  entry: id:%d name:\"%s\"\n", value_id, buffer);
            break;
          }

          case PN_VALUESYMBTAB_CODE_BBENTRY: {
            PNBlockId bb_id = pn_record_read_int32(&reader, "bb_id");
            char buffer[1024];
            char* p = &buffer[0];
            int32_t c;

            while (pn_record_try_read_int32(&reader, &c)) {
              assert(p - &buffer[0] < 1024);
              *p++ = c;
            }
            *p = 0;

            (void)bb_id;
            TRACE("  bbentry: id:%d name:\"%s\"\n", bb_id, buffer);
            break;
          }

          default:
            FATAL("bad record code: %d.\n", code);
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
}

static void pn_constants_block_read(PNBlockInfoContext* context,
                                    PNBitStream* bs) {
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
        TRACE("*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        return;

      case PN_ENTRY_SUBBLOCK:
        FATAL("unexpected subblock in constants_block\n");

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
            TRACE("  constants settype %d\n", cur_type_id);
            break;

          case PN_CONSTANTS_CODE_UNDEF: {
            PNConstantId constant_id;
            PNConstant* constant =
                pn_context_append_constant(context, &constant_id);
            constant->code = code;
            constant->type_id = cur_type_id;

            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_CONSTANT;
            value->index = constant_id;

            TRACE("  %%%d. undef\n", value_id);
            break;
          }

          case PN_CONSTANTS_CODE_INTEGER: {
            int32_t data = pn_decode_sign_rotated_value(
                pn_record_read_int32(&reader, "integer value"));

            PNConstantId constant_id;
            PNConstant* constant =
                pn_context_append_constant(context, &constant_id);
            constant->code = code;
            constant->type_id = cur_type_id;
            constant->int_value = data;

            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_CONSTANT;
            value->index = constant_id;

            TRACE("  %%%d. integer %d\n", value_id, data);
            break;
          }

          case PN_CONSTANTS_CODE_FLOAT: {
            /* TODO(binji): read this as a float */
            float data = pn_record_read_int32(&reader, "float value");

            PNConstantId constant_id;
            PNConstant* constant =
                pn_context_append_constant(context, &constant_id);
            constant->code = code;
            constant->type_id = cur_type_id;
            constant->float_value = data;

            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_CONSTANT;
            value->index = constant_id;

            TRACE("  %%%d. float %g\n", value_id, data);
            break;
          }

          default:
            FATAL("bad record code: %d.\n", code);
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
}

static void pn_function_block_read(PNBlockInfoContext* context,
                                   PNBitStream* bs,
                                   PNFunctionId function_id) {
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNBlockAbbrevs abbrevs = {};
  pn_block_info_context_get_abbrev(context, PN_BLOCKID_FUNCTION, &abbrevs);

  PNFunction* function = pn_context_get_function(context, function_id);
  PNType* function_type = pn_context_get_type(context, function->type_id);
  assert(function_type->code == PN_TYPE_CODE_FUNCTION);

  if (function->name) {
    TRACE("function %%%d (%s)\n", function_id, function->name);
  } else {
    TRACE("function %%%d\n", function_id);
  }

  uint32_t i;
  for (i = 0; i < function_type->num_args; ++i) {
    PNValueId value_id;
    PNValue* value = pn_context_append_value(context, &value_id);
    value->code = PN_VALUE_CODE_FUNCTION_ARG;
    value->index = i;

    TRACE("  %%%d. function arg %d\n", value_id, i);
  }

  PNBlockId num_bbs = -1;
  PNBlockId last_bb = -1;
  PNBlockId cur_bb = 0;
  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        TRACE("*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        return;

      case PN_ENTRY_SUBBLOCK: {
        uint32_t id = pn_bitstream_read_vbr(bs, 8);
        TRACE("*** SUBBLOCK %d\n", id);
        switch (id) {
          case PN_BLOCKID_CONSTANTS:
            pn_constants_block_read(context, bs);
            break;

          case PN_BLOCKID_VALUE_SYMTAB:
            pn_value_symtab_block_read(context, bs);
            break;

          default:
            FATAL("bad block id %d\n", id);
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

        if (code == PN_FUNCTION_CODE_DECLAREBLOCKS) {
          num_bbs = pn_record_read_uint32(&reader, "num bbs");
          (void)num_bbs;
          TRACE("num bbs:%d\n", num_bbs);
          break;
        }

        if (last_bb != cur_bb) {
          TRACE("bb:%d\n", cur_bb);
          last_bb = cur_bb;
        }

        switch (code) {
          case PN_FUNCTION_CODE_DECLAREBLOCKS:
            /* Handled above so we only print the basic block index when listing
             * instructions */
            break;

          case PN_FUNCTION_CODE_INST_BINOP: {
            PNInstructionId instruction_id;
            PNInstructionBinop* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionBinop, context, function, &instruction_id);

            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = instruction_id;

            instruction->code = code;
            instruction->value_id0 = pn_record_read_value_id(
                &reader, "value 0", context->use_relative_ids, value_id);
            instruction->value_id1 = pn_record_read_value_id(
                &reader, "value 1", context->use_relative_ids, value_id);
            instruction->opcode = pn_record_read_int32(&reader, "opcode");
            instruction->flags = 0;

            /* optional */
            pn_record_try_read_int32(&reader, &instruction->flags);

            TRACE("  %%%d. binop op:%s(%d) %%%d %%%d (flags:%d)\n", value_id,
                  pn_binop_get_name(instruction->opcode), instruction->opcode,
                  instruction->value_id0, instruction->value_id1,
                  instruction->flags);
            break;
          }

          case PN_FUNCTION_CODE_INST_CAST: {
            PNInstructionId instruction_id;
            PNInstructionCast* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionCast, context, function, &instruction_id);
            instruction->code = code;

            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = instruction_id;

            instruction->code = code;
            instruction->value_id = pn_record_read_value_id(
                &reader, "value", context->use_relative_ids, value_id);
            instruction->type_id = pn_record_read_uint32(&reader, "type_id");
            instruction->opcode = pn_record_read_int32(&reader, "opcode");

            TRACE("  %%%d. cast op:%s(%d) %%%d type:%d\n", value_id,
                  pn_cast_get_name(instruction->opcode), instruction->opcode,
                  instruction->value_id, instruction->type_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_RET: {
            PNInstructionId instruction_id;
            PNInstructionRet* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionRet, context, function, &instruction_id);
            instruction->code = code;
            instruction->value_id = PN_INVALID_VALUE_ID;

            pn_record_try_read_value_id(&reader, &instruction->value_id,
                                        context->use_relative_ids,
                                        context->num_values);

            if (instruction->value_id != PN_INVALID_VALUE_ID) {
              TRACE("  ret %%%d\n", instruction->value_id);
            } else {
              TRACE("  ret\n");
            }

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_BR: {
            PNInstructionId instruction_id;
            PNInstructionBr* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionBr, context, function, &instruction_id);
            instruction->code = code;
            instruction->true_bb_id = pn_record_read_uint32(&reader, "true_bb");
            instruction->false_bb_id = PN_INVALID_BLOCK_ID;

            if (pn_record_try_read_uint32(&reader, &instruction->false_bb_id)) {
              instruction->value_id = pn_record_read_value_id(
                  &reader, "value", context->use_relative_ids,
                  context->num_values);
            }

            if (instruction->false_bb_id != PN_INVALID_BLOCK_ID) {
              TRACE("  br %%%d ? %d : %d\n", instruction->value_id,
                    instruction->true_bb_id, instruction->false_bb_id);
            } else {
              TRACE("  br %d\n", instruction->true_bb_id);
            }
            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_SWITCH: {
            PNInstructionId instruction_id;
            PNInstructionSwitch* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionSwitch, context, function, &instruction_id);
            instruction->code = code;

            instruction->type_id = pn_record_read_uint32(&reader, "type_id");
            instruction->value_id = pn_record_read_value_id(
                &reader, "value", context->use_relative_ids,
                context->num_values);
            instruction->default_bb_id =
                pn_record_read_uint32(&reader, "default bb");

            int32_t c = 0;
            instruction->num_cases = pn_record_read_int32(&reader, "num cases");
            instruction->cases =
                pn_arena_alloc(&context->instruction_arena,
                               sizeof(PNSwitchCase) * instruction->num_cases);

            for (c = 0; c < instruction->num_cases; ++c) {
              PNSwitchCase* switch_case = &instruction->cases[c];
              switch_case->num_values =
                  pn_record_read_int32(&reader, "num values");
              switch_case->values = pn_arena_alloc(
                  &context->instruction_arena,
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
            }

            TRACE("  switch type:%d value:%%%d [default:%d]",
                  instruction->type_id, instruction->value_id,
                  instruction->default_bb_id);

            for (c = 0; c < instruction->num_cases; ++c) {
              PNSwitchCase* switch_case = &instruction->cases[c];
              TRACE(" [");

              int32_t i;
              for (i = 0; i < switch_case->num_values; ++i) {
                PNSwitchCaseValue* value = &switch_case->values[i];
                if (value->is_single) {
                  TRACE("[%%%d] ", value->low);
                } else {
                  TRACE("[%%%d,%%%d] ", value->low, value->high);
                }
              }
              TRACE("=> bb:%d]", switch_case->bb_id);
            }
            TRACE("\n");

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_UNREACHABLE: {
            PNInstructionId instruction_id;
            PNInstructionUnreachable* instruction =
                PN_FUNCTION_APPEND_INSTRUCTION(PNInstructionUnreachable,
                                               context, function,
                                               &instruction_id);
            instruction->code = code;

            TRACE("  unreachable\n");
            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_PHI: {
            PNInstructionId instruction_id;
            PNInstructionPhi* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionPhi, context, function, &instruction_id);
            instruction->code = code;

            PNTypeId type_id = pn_record_read_int32(&reader, "type_id");
            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = instruction_id;

            (void)type_id;
            TRACE("  %%%d. phi type:%d", value_id, type_id);
            while (1) {
              PNBlockId bb;
              PNValueId value;
              if (!pn_record_try_read_uint32(&reader, &value)) {
                break;
              }
              if (context->use_relative_ids) {
                value = value_id - (int32_t)pn_decode_sign_rotated_value(value);
              }

              if (!pn_record_try_read_uint32(&reader, &bb)) {
                FATAL("unable to read phi bb index\n");
              }
              TRACE(" bb:%d=>%%%d", bb, value);
            }
            TRACE("\n");
            break;
          }

          case PN_FUNCTION_CODE_INST_ALLOCA: {
            PNInstructionId instruction_id;
            PNInstructionAlloca* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionAlloca, context, function, &instruction_id);
            instruction->code = code;

            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = instruction_id;

            int32_t size = pn_record_read_int32(&reader, "size");
            int32_t alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;
            (void)size;
            (void)alignment;
            TRACE("  %%%d. alloca %%%d align=%d\n", value_id, size, alignment);
            break;
          }

          case PN_FUNCTION_CODE_INST_LOAD: {
            PNInstructionId instruction_id;
            PNInstructionLoad* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionLoad, context, function, &instruction_id);
            instruction->code = code;

            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = instruction_id;

            PNValueId src = pn_record_read_int32(&reader, "src");
            if (context->use_relative_ids) {
              src = value_id - src;
            }
            PNTypeId type_id = pn_record_read_int32(&reader, "type_id");
            int32_t alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;

            (void)type_id;
            (void)alignment;
            TRACE("  %%%d. load src:%%%d type:%d align=%d\n", value_id, src,
                  type_id, alignment);
            break;
          }

          case PN_FUNCTION_CODE_INST_STORE: {
            PNInstructionId instruction_id;
            PNInstructionStore* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionStore, context, function, &instruction_id);
            instruction->code = code;

            PNValueId dest = pn_record_read_int32(&reader, "dest");
            PNValueId value = pn_record_read_int32(&reader, "value");
            if (context->use_relative_ids) {
              dest = context->num_values - dest;
              value = context->num_values - value;
            }
            int32_t alignment = pn_record_read_int32(&reader, "alignment");
            alignment = (1 << alignment) >> 1;
            TRACE("  store dest:%%%d value:%%%d align=%d\n", dest, value,
                  alignment);
            break;
          }

          case PN_FUNCTION_CODE_INST_CMP2: {
            PNInstructionId instruction_id;
            PNInstructionCmp2* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionCmp2, context, function, &instruction_id);
            instruction->code = code;

            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = instruction_id;

            PNValueId value0 = pn_record_read_int32(&reader, "value 0");
            PNValueId value1 = pn_record_read_int32(&reader, "value 1");
            if (context->use_relative_ids) {
              value0 = value_id - value0;
              value1 = value_id - value1;
            }
            int32_t opcode = pn_record_read_int32(&reader, "opcode");
            (void)opcode;
            TRACE("  %%%d. cmp2 op:%s(%d) %%%d %%%d\n", value_id,
                  pn_cmp2_get_name(opcode), opcode, value0, value1);
            break;
          }

          case PN_FUNCTION_CODE_INST_VSELECT: {
            PNInstructionId instruction_id;
            PNInstructionVselect* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionVselect, context, function, &instruction_id);
            instruction->code = code;

            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->index = instruction_id;

            PNValueId true_value = pn_record_read_int32(&reader, "true_value");
            PNValueId false_value =
                pn_record_read_int32(&reader, "false_value");
            PNValueId cond = pn_record_read_int32(&reader, "cond");
            if (context->use_relative_ids) {
              true_value = value_id - true_value;
              false_value = value_id - false_value;
              cond = value_id - cond;
            }
            TRACE("  %%%d. vselect %%%d ? %%%d : %%%d\n", value_id, cond,
                  true_value, false_value);
            break;
          }

          case PN_FUNCTION_CODE_INST_FORWARDTYPEREF: {
            /* TODO(binji): First value is the value index, what is the second?
             */
            int32_t ftr1 = pn_record_read_int32(&reader, "ftr1");
            int32_t ftr2 = pn_record_read_int32(&reader, "ftr2");
            (void)ftr1;
            (void)ftr2;
            TRACE("  forwardtyperef %d %d\n", ftr1, ftr2);
            break;
          }

          case PN_FUNCTION_CODE_INST_CALL:
          case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
            PNInstructionId instruction_id;
            PNInstructionCall* instruction = PN_FUNCTION_APPEND_INSTRUCTION(
                PNInstructionCall, context, function, &instruction_id);
            instruction->code = code;

            PNBool is_indirect = code == PN_FUNCTION_CODE_INST_CALL_INDIRECT;
            int32_t cc_info = pn_record_read_int32(&reader, "cc_info");
            PNBool is_tail_call = cc_info & 1;
            int32_t calling_convention = cc_info >> 1;
            (void)is_tail_call;
            (void)calling_convention;
            PNValueId callee = pn_record_read_uint32(&reader, "callee");
            if (context->use_relative_ids) {
              callee = context->num_values - callee;
            }

            const char* name = NULL;
            PNTypeId type_id;
            PNTypeId return_type_id;
            if (is_indirect) {
              return_type_id = pn_record_read_int32(&reader, "return_type");
            } else {
              PNFunction* function = pn_context_get_function(context, callee);
              type_id = function->type_id;
              PNType* function_type = pn_context_get_type(context, type_id);
              assert(function_type->code == PN_TYPE_CODE_FUNCTION);
              return_type_id = function_type->return_type;
              name = function->name;
            }

            PNType* return_type = pn_context_get_type(context, return_type_id);
            PNBool is_return_type_void = return_type->code == PN_TYPE_CODE_VOID;
            TRACE("  ");
            PNValueId value_id;
            if (!is_return_type_void) {
              PNValue* value = pn_context_append_value(context, &value_id);
              value->code = PN_VALUE_CODE_LOCAL_VAR;
              value->index = instruction_id;

              TRACE("%%%d. ", value_id);
            } else {
              value_id = context->num_values;
            }
            TRACE("call ");
            if (is_indirect) {
              TRACE("indirect ");
            }
            if (name && name[0]) {
              TRACE("%%%d(%s) ", callee, name);
            } else {
              TRACE("%%%d ", callee);
            }
            TRACE("args:");

            int32_t arg;
            while (pn_record_try_read_int32(&reader, &arg)) {
              if (context->use_relative_ids) {
                arg = value_id - arg;
              }
              TRACE(" %%%d", arg);
            }
            TRACE("\n");

            break;
          }

          default:
            FATAL("bad record code: %d.\n", code);
        }

        if (is_terminator) {
          cur_bb++;
        }

        pn_record_reader_finish(&reader);
        break;
      }
    }
  }
}

static void pn_module_block_read(PNBlockInfoContext* context, PNBitStream* bs) {
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
        return;

      case PN_ENTRY_SUBBLOCK: {
        uint32_t id = pn_bitstream_read_vbr(bs, 8);

        TRACE("*** SUBBLOCK %d\n", id);
        switch (id) {
          case PN_BLOCKID_BLOCKINFO:
            pn_blockinfo_block_read(context, bs);
            break;
          case PN_BLOCKID_TYPE:
            pn_type_block_read(context, bs);
            break;
          case PN_BLOCKID_GLOBALVAR:
            pn_globalvar_block_read(context, bs);
            break;
          case PN_BLOCKID_VALUE_SYMTAB:
            pn_value_symtab_block_read(context, bs);
            break;
          case PN_BLOCKID_FUNCTION: {
            uint32_t old_num_values = context->num_values;
            while (pn_context_get_function(context, function_id)->is_proto) {
              function_id++;
            }

            pn_function_block_read(context, bs, function_id);
            function_id++;
            context->num_values = old_num_values;
            TRACE("resetting the number of values to %d\n", old_num_values);
            break;
          }
          default:
            FATAL("bad block id %d\n", id);
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
            context->module->version =
                pn_record_read_int32(&reader, "module version");
            context->use_relative_ids = context->module->version == 1;
            TRACE("module version: %d\n", context->module->version);
            break;
          }

          case PN_MODULE_CODE_FUNCTION: {
            PNFunctionId function_id;
            PNFunction* function =
                pn_context_append_function(context, &function_id);
            function->type_id = pn_record_read_int32(&reader, "type_id");
            function->calling_convention =
                pn_record_read_int32(&reader, "calling_convention");
            function->is_proto = pn_record_read_int32(&reader, "is_proto");
            function->linkage = pn_record_read_int32(&reader, "linkage");

            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_FUNCTION;
            value->index = function_id;

            TRACE(
                "%%%d. module function: "
                "(type:%d,cc:%d,is_proto:%d,linkage:%d)\n",
                value_id, function->type_id, function->calling_convention,
                function->is_proto, function->linkage);
            break;
          }

          default:
            FATAL("bad record code: %d.\n", code);
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
      FATAL("Expected '%c'\n", sig[i]);
    }
  }

  uint32_t num_fields = pn_bitstream_read(bs, 16);
  pn_bitstream_read(bs, 16); /* num_bytes */
  for (i = 0; i < num_fields; ++i) {
    uint32_t ftype = pn_bitstream_read(bs, 4);
    uint32_t id = pn_bitstream_read(bs, 4);
    if (id != 1) {
      FATAL("bad header id: %d\n", id);
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
        FATAL("bad ftype %d\n", ftype);
    }
  }
}

int main(int argc, char** argv) {
  --argc, ++argv;
  const char* filename = "simple.pexe";
  if (argc >= 1) {
    filename = argv[0];
  }

  FILE* f = fopen(filename, "r");
  if (!f) {
    FATAL("unable to read %s\n", filename);
  }

  fseek(f, 0, SEEK_END);
  size_t fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  uint8_t* data = malloc(fsize);

  size_t read_size = fread(data, 1, fsize, f);
  if (read_size != fsize) {
    FATAL("unable to read data from file\n");
  }

  fclose(f);

  PNBitStream bs;
  pn_bitstream_init(&bs, data, fsize);
  pn_header_read(&bs);

  PNModule* module = malloc(sizeof(PNModule));
  PNBlockInfoContext context = {};
  context.module = module;
  pn_arena_init(&context.arena, PN_ARENA_SIZE);
  pn_arena_init(&context.value_arena, PN_VALUE_ARENA_SIZE);
  pn_arena_init(&context.instruction_arena, PN_INSTRUCTION_ARENA_SIZE);

  uint32_t entry = pn_bitstream_read(&bs, 2);
  TRACE("entry: %d\n", entry);
  if (entry != PN_ENTRY_SUBBLOCK) {
    FATAL("expected subblock at top-level\n");
  }

  PNBlockId block_id = pn_bitstream_read_vbr(&bs, 8);
  assert(block_id == PN_BLOCKID_MODULE);
  pn_module_block_read(&context, &bs);
  TRACE("done\n");

  return 0;
}
