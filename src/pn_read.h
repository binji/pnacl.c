/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_READ_H_
#define PN_READ_H_

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
                   pn_type_describe_all(module, current_type_id - 1, NULL,
                                        PN_FALSE, PN_FALSE));
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
    int32_t addend;
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
            int32_t addend = 0;
            /* Optional */
            pn_record_try_read_int32(&reader, &addend);

            if (addend > 0) {
              PN_TRACE(GLOBALVAR_BLOCK, "reloc %s + %d;\n",
                       pn_value_describe_temp(module, NULL, index), addend);
            } else if (addend < 0) {
              PN_TRACE(GLOBALVAR_BLOCK, "reloc %s - %d;\n",
                       pn_value_describe_temp(module, NULL, index), -addend);
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
                    data &= 1;
                    constant->value.i8 = data;
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

              PNBool found = PN_FALSE;
              if (g_pn_dedupe_phi_nodes) {
                /* Dedupe incoming branches */
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

#endif /* PN_READ_H_ */
