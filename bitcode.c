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
#define PN_MAX_BLOCK_ABBREV_OP 10
#define PN_MAX_BLOCK_ABBREV 100
#define PN_MAX_RECORD_VALUES 100
#define PN_MAX_TYPES 1000
#define PN_MAX_FUNCTIONS 1000
#define PN_MAX_FUNCTION_ARGS 20
#define PN_MAX_FUNCTION_NAME 256
#define PN_MAX_VALUES 10000
#define PN_MAX_CONSTANTS 10000

#define PN_FALSE 0
#define PN_TRUE 1

#if 0
#define TRACE(...) (void)0
#else
#define TRACE(...) printf(__VA_ARGS__)
#endif
#define ERROR(...) fprintf(stderr, __VA_ARGS__)

#define PN_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef int PNBool;
typedef uint32_t PNTypeId;
typedef uint32_t PNValueId;
typedef uint32_t PNFunctionId;
typedef uint32_t PNConstantId;
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
} PNCmp2Op;

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

typedef struct PNFunction {
  char name[PN_MAX_FUNCTION_NAME];
  PNTypeId type_id;
  uint32_t calling_convention;
  PNBool is_proto;
  uint32_t linkage;
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

typedef struct PNModule {
  uint32_t version;
  uint32_t num_functions;
  PNFunction functions[PN_MAX_FUNCTIONS];
  uint32_t num_types;
  PNType types[PN_MAX_TYPES];
  uint32_t num_constants;
  PNConstant constants[PN_MAX_CONSTANTS];
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
  PNValue values[PN_MAX_VALUES];
  PNBool use_relative_ids;
  PNModule* module;
} PNBlockInfoContext;

static const char* pn_binop_get_name(uint32_t op) {
  const char* names[] = {
      "add", "sub", "mul", "udiv", "sdiv", "urem", "srem", "shl", "lshr",
      "ashr", "and", "or", "xor"};
  if (op >= PN_ARRAY_SIZE(names)) {
    ERROR("Invalid op: %u\n", op);
    exit(1);
  }

  return names[op];
}

static const char* pn_cast_get_name(uint32_t op) {
  const char* names[] = {
      "trunc", "zext", "sext", "fptoui", "fptosi", "uitofp", "sitofp",
      "fptrunc", "fpext", NULL, NULL, "bitcast"};
  if (op >= PN_ARRAY_SIZE(names)) {
    ERROR("Invalid op: %u\n", op);
    exit(1);
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
    ERROR("Invalid op: %u\n", op);
    exit(1);
  }

  return names[op];
}

static uint32_t pn_decode_char6(uint32_t value) {
  const char data[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._";
  if (value >= PN_ARRAY_SIZE(data)) {
    ERROR("Invalid char6 value: %u\n", value);
    exit(1);
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
    ERROR("accessing invalid type %d (max %d)", type_id,
          context->module->num_types);
    exit(1);
  }

  return &context->module->types[type_id];
}

static PNType* pn_context_append_type(PNBlockInfoContext* context,
                                      PNTypeId* out_type_id) {
  *out_type_id = context->module->num_types;
  if (*out_type_id >= PN_ARRAY_SIZE(context->module->types)) {
    ERROR("too many types: %d\n", *out_type_id);
  }

  context->module->num_types++;
  return &context->module->types[*out_type_id];
}

static PNFunction* pn_context_get_function(PNBlockInfoContext* context,
                                           PNFunctionId function_id) {
  if (function_id < 0 || function_id >= context->module->num_functions) {
    ERROR("accessing invalid function %d (max %d)", function_id,
          context->module->num_functions);
    exit(1);
  }

  return &context->module->functions[function_id];
}

static PNFunction* pn_context_append_function(PNBlockInfoContext* context,
                                              PNFunctionId* out_function_id) {
  *out_function_id = context->module->num_functions;
  if (*out_function_id >= PN_ARRAY_SIZE(context->module->functions)) {
    ERROR("too many functions: %d\n", *out_function_id);
  }

  context->module->num_functions++;
  return &context->module->functions[*out_function_id];
}

static PNConstant* pn_context_get_constant(PNBlockInfoContext* context,
                                           PNConstantId constant_id) {
  if (constant_id < 0 || constant_id >= context->module->num_constants) {
    ERROR("accessing invalid constant %d (max %d)", constant_id,
          context->module->num_constants);
    exit(1);
  }

  return &context->module->constants[constant_id];
}

static PNConstant* pn_context_append_constant(PNBlockInfoContext* context,
                                              PNConstantId* out_constant_id) {
  *out_constant_id = context->module->num_constants;
  if (*out_constant_id >= PN_ARRAY_SIZE(context->module->constants)) {
    ERROR("too many constants: %d\n", *out_constant_id);
  }

  context->module->num_constants++;
  return &context->module->constants[*out_constant_id];
}

static PNValue* pn_context_get_value(PNBlockInfoContext* context,
                                     PNValueId value_id) {
  if (value_id < 0 || value_id >= context->num_values) {
    ERROR("accessing invalid value %d (max %d)", value_id, context->num_values);
    exit(1);
  }

  return &context->values[value_id];
}

static PNValue* pn_context_append_value(PNBlockInfoContext* context,
                                        PNValueId* out_value_id) {
  *out_value_id = context->num_values;
  if (*out_value_id >= PN_ARRAY_SIZE(context->values)) {
    ERROR("too many values: %d\n", *out_value_id);
  }

  context->num_values++;
  return &context->values[*out_value_id];
}

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
          ERROR("bad encoding for array element: %d\n", elt_op->encoding);
          exit(1);
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
      ERROR("bad encoding: %d\n", op->encoding);
      exit(1);
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

static int32_t pn_record_read_value_int32(PNRecordReader* reader,
                                          const char* name) {
  int32_t value;
  if (!pn_record_try_read_int32(reader, &value)) {
    ERROR("unable to read %s.\n", name);
    exit(1);
  }

  return value;
}

static uint32_t pn_record_read_value_uint32(PNRecordReader* reader,
                                            const char* name) {
  uint32_t value;
  if (!pn_record_try_read_uint32(reader, &value)) {
    ERROR("unable to read %s.\n", name);
    exit(1);
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
                ERROR("bad encoding for array element: %d\n", elt_op->encoding);
                exit(1);
            }
          }
          break;
        }

        case PN_ENCODING_CHAR6:
        case PN_ENCODING_BLOB:
          /* Nothing */
          break;

        default:
          ERROR("bad encoding: %d\n", op->encoding);
          exit(1);
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
        ERROR("unexpected subblock in blockinfo_block\n");
        exit(1);

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
            block_id = pn_record_read_value_int32(&reader, "block id");
            TRACE("block id: %d\n", block_id);
            break;

          case PN_BLOCKINFO_CODE_BLOCKNAME:
            TRACE("block name\n");
            break;

          case PN_BLOCKINFO_CODE_SETRECORDNAME:
            TRACE("block record name\n");
            break;

          default:
            ERROR("bad record code: %d.\n", code);
            exit(1);
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
        ERROR("unexpected subblock in type_block\n");
        exit(1);

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
                pn_record_read_value_uint32(&reader, "num entries");
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
            type->width = pn_record_read_value_int32(&reader, "width");
            TRACE("%d: type integer %d\n", type_id, type->width);
            break;
          }

          case PN_TYPE_CODE_FUNCTION: {
            PNTypeId type_id;
            PNType* type = pn_context_append_type(context, &type_id);
            type->code = PN_TYPE_CODE_FUNCTION;
            type->is_varargs =
                pn_record_read_value_int32(&reader, "is_varargs");
            type->return_type =
                pn_record_read_value_int32(&reader, "return_type");
            type->num_args = 0;
            TRACE("%d: type function is_varargs:%d ret:%d ", type_id,
                  type->is_varargs, type->return_type);

            while (pn_record_try_read_uint32(
                &reader, &type->arg_types[type->num_args])) {
              TRACE("%d ", type->arg_types[type->num_args]);
              type->num_args++;
            }
            TRACE("\n");
            break;
          }

          default:
            ERROR("bad record code: %d.\n", code);
            exit(1);
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

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        TRACE("*** END BLOCK\n");
        pn_bitstream_align_32(bs);
        return;

      case PN_ENTRY_SUBBLOCK:
        ERROR("unexpected subblock in globalvar_block\n");
        exit(1);

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
            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_GLOBAL_VAR;

            int32_t alignment =
                pn_record_read_value_int32(&reader, "alignment");
            PNBool is_constant =
                pn_record_read_value_int32(&reader, "is_constant");
            TRACE("%%%d. var. alignment:%d is_constant:%d\n", value_id,
                  (1 << alignment) >> 1, is_constant != 0);
            break;
          }

          case PN_GLOBALVAR_CODE_COMPOUND: {
            int32_t num_initializers =
                pn_record_read_value_int32(&reader, "num_initializers");

            TRACE("  compound. num initializers: %d\n", num_initializers);
            break;
          }

          case PN_GLOBALVAR_CODE_ZEROFILL: {
            int32_t num_bytes =
                pn_record_read_value_int32(&reader, "num_bytes");

            TRACE("  zerofill. num_bytes: %d\n", num_bytes);
            break;
          }

          case PN_GLOBALVAR_CODE_DATA: {
            int num_bytes = 0;
            uint32_t value;
            while (pn_record_try_read_uint32(&reader, &value)) {
              num_bytes++;
            }
            TRACE("  data. num_bytes: %d\n", num_bytes);
            break;
          }

          case PN_GLOBALVAR_CODE_RELOC: {
            int32_t index = pn_record_read_value_int32(&reader, "reloc index");
            int32_t addend = 0;
            /* Optional */
            pn_record_try_read_int32(&reader, &addend);

            TRACE("  reloc. index: %d addend: %d\n", index, addend);
            break;
          }

          case PN_GLOBALVAR_CODE_COUNT: {
            int32_t count =
                pn_record_read_value_int32(&reader, "global var count");
            TRACE("global var count: %d\n", count);
            break;
          }

          default:
            ERROR("bad record code: %d.\n", code);
            exit(1);
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
        ERROR("unexpected subblock in valuesymtab_block\n");
        exit(1);

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
            PNValueId value_id =
                pn_record_read_value_int32(&reader, "value_id");
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
            PNBlockId bb_id = pn_record_read_value_int32(&reader, "bb_id");
            char buffer[1024];
            char* p = &buffer[0];
            int32_t c;

            while (pn_record_try_read_int32(&reader, &c)) {
              assert(p - &buffer[0] < 1024);
              *p++ = c;
            }
            *p = 0;

            TRACE("  bbentry: id:%d name:\"%s\"\n", bb_id, buffer);
            break;
          }

          default:
            ERROR("bad record code: %d.\n", code);
            exit(1);
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
        ERROR("unexpected subblock in constants_block\n");
        exit(1);

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
            cur_type_id = pn_record_read_value_int32(&reader, "current type");
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
                pn_record_read_value_int32(&reader, "integer value"));

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
            float data = pn_record_read_value_int32(&reader, "float value");

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
            ERROR("bad record code: %d.\n", code);
            exit(1);
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
            ERROR("bad block id %d\n", id);
            exit(1);
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
          num_bbs = pn_record_read_value_uint32(&reader, "num bbs");
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
            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;

            PNValueId value0 = pn_record_read_value_int32(&reader, "value 0");
            PNValueId value1 = pn_record_read_value_int32(&reader, "value 1");
            if (context->use_relative_ids) {
              value0 = value_id - value0;
              value1 = value_id - value1;
            }
            int32_t opcode = pn_record_read_value_int32(&reader, "opcode");
            int32_t flags = 0;
            /* optional */
            pn_record_try_read_int32(&reader, &flags);

            TRACE("  %%%d. binop op:%s(%d) %%%d %%%d (flags:%d)\n", value_id,
                  pn_binop_get_name(opcode), opcode, value0, value1, flags);
            break;
          }

          case PN_FUNCTION_CODE_INST_CAST: {
            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;

            PNValueId opval_id = pn_record_read_value_int32(&reader, "value");
            if (context->use_relative_ids) {
              opval_id = value_id - opval_id;
            }
            PNTypeId type_id = pn_record_read_value_int32(&reader, "type_id");
            int32_t opcode = pn_record_read_value_int32(&reader, "opcode");

            TRACE("  %%%d. cast op:%s(%d) %%%d type:%d\n", value_id,
                  pn_cast_get_name(opcode), opcode, opval_id, type_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_RET: {
            PNValueId value;
            if (pn_record_try_read_uint32(&reader, &value)) {
              if (context->use_relative_ids) {
                value = context->num_values - value;
              }

              TRACE("  ret %%%d\n", value);
            } else {
              TRACE("  ret\n");
            }

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_BR: {
            PNBlockId true_bb = pn_record_read_value_int32(&reader, "true_bb");
            PNBlockId false_bb;
            if (pn_record_try_read_uint32(&reader, &false_bb)) {
              PNValueId value = pn_record_read_value_int32(&reader, "value");
              if (context->use_relative_ids) {
                value = context->num_values - value;
              }
              TRACE("  br %%%d ? %d : %d\n", value, true_bb, false_bb);
            } else {
              TRACE("  br %d\n", true_bb);
            }
            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_SWITCH: {
            PNTypeId type_id = pn_record_read_value_int32(&reader, "type_id");
            PNValueId value = pn_record_read_value_int32(&reader, "value");
            if (context->use_relative_ids) {
              value = context->num_values - value;
            }
            PNBlockId default_bb =
                pn_record_read_value_int32(&reader, "default bb");
            TRACE("  switch type:%d value:%%%d [default:%d]", type_id, value,
                  default_bb);
            int32_t c = 0;
            int32_t num_cases =
                pn_record_read_value_int32(&reader, "num cases");
            for (c = 0; c < num_cases; ++c) {
              int32_t num_values =
                  pn_record_read_value_int32(&reader, "num values");
              TRACE(" [");

              int32_t i;
              for (i = 0; i < num_values; ++i) {
                PNBool is_single =
                    pn_record_read_value_int32(&reader, "is_single");
                int32_t low = pn_decode_sign_rotated_value(
                    pn_record_read_value_int32(&reader, "low"));
                if (is_single) {
                  TRACE("[%%%d] ", low);
                } else {
                  int32_t high = pn_decode_sign_rotated_value(
                      pn_record_read_value_int32(&reader, "high"));
                  TRACE("[%%%d,%%%d] ", low, high);
                }
              }
              PNBlockId bb = pn_record_read_value_int32(&reader, "bb");
              TRACE("=> bb:%d]", bb);
            }
            TRACE("\n");
            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_UNREACHABLE:
            TRACE("  unreachable\n");
            is_terminator = PN_TRUE;
            break;

          case PN_FUNCTION_CODE_INST_PHI: {
            PNTypeId type_id = pn_record_read_value_int32(&reader, "type_id");
            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;

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
                ERROR("unable to read phi bb index\n");
                exit(1);
              }
              TRACE(" bb:%d=>%%%d", bb, value);
            }
            TRACE("\n");
            break;
          }

          case PN_FUNCTION_CODE_INST_ALLOCA: {
            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;

            int32_t size = pn_record_read_value_int32(&reader, "size");
            int32_t alignment =
                pn_record_read_value_int32(&reader, "alignment");
            alignment = (1 << alignment) >> 1;
            TRACE("  %%%d. alloca %%%d align=%d\n", value_id, size, alignment);
            break;
          }

          case PN_FUNCTION_CODE_INST_LOAD: {
            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;

            PNValueId src = pn_record_read_value_int32(&reader, "src");
            if (context->use_relative_ids) {
              src = value_id - src;
            }
            PNTypeId type_id = pn_record_read_value_int32(&reader, "type_id");
            int32_t alignment =
                pn_record_read_value_int32(&reader, "alignment");
            alignment = (1 << alignment) >> 1;

            TRACE("  %%%d. load src:%%%d type:%d align=%d\n", value_id, src,
                  type_id, alignment);
            break;
          }

          case PN_FUNCTION_CODE_INST_STORE: {
            PNValueId dest = pn_record_read_value_int32(&reader, "dest");
            PNValueId value = pn_record_read_value_int32(&reader, "value");
            if (context->use_relative_ids) {
              dest = context->num_values - dest;
              value = context->num_values - value;
            }
            int32_t alignment =
                pn_record_read_value_int32(&reader, "alignment");
            alignment = (1 << alignment) >> 1;
            TRACE("  store dest:%%%d value:%%%d align=%d\n", dest, value,
                  alignment);
            break;
          }

          case PN_FUNCTION_CODE_INST_CMP2: {
            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;

            PNValueId value0 = pn_record_read_value_int32(&reader, "value 0");
            PNValueId value1 = pn_record_read_value_int32(&reader, "value 1");
            if (context->use_relative_ids) {
              value0 = value_id - value0;
              value1 = value_id - value1;
            }
            int32_t opcode = pn_record_read_value_int32(&reader, "opcode");
            TRACE("  %%%d. cmp2 op:%s(%d) %%%d %%%d\n", value_id,
                  pn_cmp2_get_name(opcode), opcode, value0, value1);
            break;
          }

          case PN_FUNCTION_CODE_INST_VSELECT: {
            PNValueId value_id;
            PNValue* value = pn_context_append_value(context, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;

            PNValueId true_value =
                pn_record_read_value_int32(&reader, "true_value");
            PNValueId false_value =
                pn_record_read_value_int32(&reader, "false_value");
            PNValueId cond = pn_record_read_value_int32(&reader, "cond");
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
            int32_t ftr1 = pn_record_read_value_int32(&reader, "ftr1");
            int32_t ftr2 = pn_record_read_value_int32(&reader, "ftr2");
            TRACE("  forwardtyperef %d %d\n", ftr1, ftr2);
            break;
          }

          case PN_FUNCTION_CODE_INST_CALL:
          case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
            PNBool is_indirect = code == PN_FUNCTION_CODE_INST_CALL_INDIRECT;
            int32_t cc_info = pn_record_read_value_int32(&reader, "cc_info");
            PNBool is_tail_call = cc_info & 1;
            int32_t calling_convention = cc_info >> 1;
            (void)is_tail_call;
            (void)calling_convention;
            PNValueId callee = pn_record_read_value_uint32(&reader, "callee");
            if (context->use_relative_ids) {
              callee = context->num_values - callee;
            }

            const char* name = NULL;
            PNTypeId type_id;
            PNTypeId return_type_id;
            if (is_indirect) {
              return_type_id =
                  pn_record_read_value_int32(&reader, "return_type");
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
            ERROR("bad record code: %d.\n", code);
            exit(1);
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
            ERROR("bad block id %d\n", id);
            exit(1);
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
                pn_record_read_value_int32(&reader, "module version");
            context->use_relative_ids = context->module->version == 1;
            TRACE("module version: %d\n", context->module->version);
            break;
          }

          case PN_MODULE_CODE_FUNCTION: {
            PNFunctionId function_id;
            PNFunction* function =
                pn_context_append_function(context, &function_id);
            function->type_id = pn_record_read_value_int32(&reader, "type_id");
            function->calling_convention =
                pn_record_read_value_int32(&reader, "calling_convention");
            function->is_proto =
                pn_record_read_value_int32(&reader, "is_proto");
            function->linkage = pn_record_read_value_int32(&reader, "linkage");

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
            ERROR("bad record code: %d.\n", code);
            exit(1);
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
      ERROR("Expected '%c'\n", sig[i]);
      exit(1);
    }
  }

  uint32_t num_fields = pn_bitstream_read(bs, 16);
  pn_bitstream_read(bs, 16); /* num_bytes */
  for (i = 0; i < num_fields; ++i) {
    uint32_t ftype = pn_bitstream_read(bs, 4);
    uint32_t id = pn_bitstream_read(bs, 4);
    if (id != 1) {
      ERROR("bad header id: %d\n", id);
      exit(1);
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
        ERROR("bad ftype %d\n", ftype);
        exit(1);
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
    ERROR("unable to read %s\n", filename);
    exit(1);
  }

  fseek(f, 0, SEEK_END);
  size_t fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  uint8_t* data = malloc(fsize);

  size_t read_size = fread(data, 1, fsize, f);
  if (read_size != fsize) {
    ERROR("unable to read data from file\n");
    exit(1);
  }

  fclose(f);

  PNBitStream bs;
  pn_bitstream_init(&bs, data, fsize);
  pn_header_read(&bs);

  PNModule module;
  PNBlockInfoContext context = {};
  context.module = &module;

  uint32_t entry = pn_bitstream_read(&bs, 2);
  TRACE("entry: %d\n", entry);
  if (entry != PN_ENTRY_SUBBLOCK) {
    ERROR("expected subblock at top-level\n");
    exit(1);
  }

  PNBlockId block_id = pn_bitstream_read_vbr(&bs, 8);
  assert(block_id == PN_BLOCKID_MODULE);
  pn_module_block_read(&context, &bs);
  TRACE("done\n");

  return 0;
}
