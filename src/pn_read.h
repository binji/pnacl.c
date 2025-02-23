/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_READ_H_
#define PN_READ_H_

static void pn_blockinfo_block_read(PNReadContext* read_context,
                                    PNModule* module,
                                    PNBlockInfoContext* context,
                                    PNBitStream* bs) {
  PN_BEGIN_TIME(BLOCKINFO_BLOCK_READ);
  PN_CALLBACK(read_context, before_blockinfo_block,
              (module, read_context->user_data));

  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  PN_CHECK(codelen <= 32);
  pn_bitstream_align_32(bs);
  (void)pn_bitstream_read(bs, 32); /* num words */

  PNAbbrevs abbrevs = {};
  PNBlockId block_id = -1;

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_CALLBACK(read_context, after_blockinfo_block,
                    (module, read_context->user_data));
        pn_bitstream_align_32(bs);
        PN_END_TIME(BLOCKINFO_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in blockinfo_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        PNAbbrev* abbrev =
            pn_abbrev_read(&module->temp_allocator, bs, &abbrevs);
        PNAbbrevId abbrev_id = pn_block_info_context_append_abbrev(
            &module->allocator, context, block_id, abbrev);
        PN_CALLBACK(
            read_context, define_abbrev,
            (module, abbrev_id, abbrev, PN_TRUE, read_context->user_data));
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
            PN_CALLBACK(read_context, blockinfo_setbid,
                        (module, block_id, read_context->user_data));
            break;

          case PN_BLOCKINFO_CODE_BLOCKNAME:
            PN_CALLBACK(read_context, blockinfo_blockname,
                        (module, read_context->user_data));
            break;

          case PN_BLOCKINFO_CODE_SETRECORDNAME:
            PN_CALLBACK(read_context, blockinfo_setrecordname,
                        (module, read_context->user_data));
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

static void pn_type_block_read(PNReadContext* read_context,
                               PNModule* module,
                               PNBlockInfoContext* context,
                               PNBitStream* bs) {
  PN_BEGIN_TIME(TYPE_BLOCK_READ);
  PN_CALLBACK(read_context, before_type_block,
              (module, read_context->user_data));

  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  PN_CHECK(codelen <= 32);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num_words */

  PNAbbrevs abbrevs = {};
  pn_block_info_context_copy_abbrevs_for_block_id(
      &module->temp_allocator, context, PN_BLOCKID_TYPE, &abbrevs);

  PNTypeId current_type_id = 0;
  PNType* type = NULL;

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_CHECK(current_type_id == module->num_types);
        PN_CALLBACK(read_context, after_type_block,
                    (module, read_context->user_data));
        pn_bitstream_align_32(bs);
        PN_END_TIME(TYPE_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in type_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        PNAbbrev* abbrev =
            pn_abbrev_read(&module->temp_allocator, bs, &abbrevs);
        PNAbbrevId abbrev_id = abbrev - abbrevs.abbrevs;
        PN_CALLBACK(
            read_context, define_abbrev,
            (module, abbrev_id, abbrev, PN_FALSE, read_context->user_data));
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
            PN_CALLBACK(read_context, type_num_entries,
                        (module, module->num_types, read_context->user_data));
            break;
          }

          case PN_TYPE_CODE_VOID: {
            PN_CHECK(current_type_id < module->num_types);
            PNTypeId type_id = current_type_id++;
            type = &module->types[type_id];
            type->code = PN_TYPE_CODE_VOID;
            type->basic_type = PN_BASIC_TYPE_VOID;
            break;
          }

          case PN_TYPE_CODE_FLOAT: {
            PN_CHECK(current_type_id < module->num_types);
            PNTypeId type_id = current_type_id++;
            type = &module->types[type_id];
            type->code = PN_TYPE_CODE_FLOAT;
            type->basic_type = PN_BASIC_TYPE_FLOAT;
            break;
          }

          case PN_TYPE_CODE_DOUBLE: {
            PN_CHECK(current_type_id < module->num_types);
            PNTypeId type_id = current_type_id++;
            type = &module->types[type_id];
            type->code = PN_TYPE_CODE_DOUBLE;
            type->basic_type = PN_BASIC_TYPE_DOUBLE;
            break;
          }

          case PN_TYPE_CODE_INTEGER: {
            PN_CHECK(current_type_id < module->num_types);
            PNTypeId type_id = current_type_id++;
            type = &module->types[type_id];
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
            type = &module->types[type_id];
            type->code = PN_TYPE_CODE_FUNCTION;
            type->basic_type = PN_BASIC_TYPE_INT32;
            type->is_varargs = pn_record_read_int32(&reader, "is_varargs");
            type->return_type = pn_record_read_int32(&reader, "return_type");
            pn_type_id_check(module, type->return_type);
            type->num_args = pn_record_num_values_left(&reader);
            type->arg_types = pn_allocator_alloc(
                &module->allocator, type->num_args * sizeof(PNTypeId),
                sizeof(PNTypeId));
            uint32_t n;
            for (n = 0; n < type->num_args; ++n) {
              type->arg_types[n] = pn_record_read_uint16(&reader, "arg type");
              pn_type_id_check(module, type->arg_types[n]);
            }
            break;
          }

          default:
            PN_FATAL("bad record code: %d.\n", code);
        }

        if (code != PN_TYPE_CODE_NUMENTRY) {
          PN_CALLBACK(read_context, type_entry,
                      (module, current_type_id, type, read_context->user_data));
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
  PNValue* value = &module->values[value_id];
  uint32_t reloc_value;
  switch (value->code) {
    case PN_VALUE_CODE_GLOBAL_VAR: {
      PNGlobalVar* var = &module->global_vars[value->index];
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

  pn_memory_write_u32(module->memory, offset, reloc_value);
}

static void pn_globalvar_block_read(PNReadContext* read_context,
                                    PNModule* module,
                                    PNBlockInfoContext* context,
                                    PNBitStream* bs) {
  PN_BEGIN_TIME(GLOBALVAR_BLOCK_READ);
  PN_CALLBACK(read_context, before_globalvar_block,
              (module, read_context->user_data));

  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  PN_CHECK(codelen <= 32);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNAbbrevs abbrevs = {};
  pn_block_info_context_copy_abbrevs_for_block_id(
      &module->temp_allocator, context, PN_BLOCKID_GLOBALVAR, &abbrevs);

  PNGlobalVarId global_var_id = 0;
  PNGlobalVar* global_var = NULL;

  uint32_t num_global_vars = 0;
  uint32_t initializer_id = 0;

  PNMemory* memory = module->memory;
  uint8_t* data8 = memory->data;
  uint32_t data_offset = PN_MEMORY_GUARD_SIZE;

  memory->globalvar_start = data_offset;

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
          PN_CALLBACK(
              read_context, globalvar_after_var,
              (module, global_var_id, global_var, read_context->user_data));
        }

        PN_CALLBACK(read_context, after_globalvar_block,
                    (module, read_context->user_data));

        pn_bitstream_align_32(bs);

        uint32_t i;
        for (i = 0; i < num_reloc_infos; ++i) {
          PN_CHECK(reloc_infos[i].index < module->num_values);
          pn_globalvar_write_reloc(module, reloc_infos[i].index,
                                   reloc_infos[i].offset,
                                   reloc_infos[i].addend);
        }

        memory->globalvar_end = data_offset;
        PN_END_TIME(GLOBALVAR_BLOCK_READ);
        return;
      }

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in globalvar_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        PNAbbrev* abbrev =
            pn_abbrev_read(&module->temp_allocator, bs, &abbrevs);
        PNAbbrevId abbrev_id = abbrev - abbrevs.abbrevs;
        PN_CALLBACK(
            read_context, define_abbrev,
            (module, abbrev_id, abbrev, PN_FALSE, read_context->user_data));
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
            if (global_var) {
              PN_CALLBACK(
                  read_context, globalvar_after_var,
                  (module, global_var_id, global_var, read_context->user_data));
            }

            global_var_id = module->num_global_vars++;
            PN_CHECK(global_var_id < num_global_vars);

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

            PN_CALLBACK(read_context, globalvar_before_var,
                        (module, global_var_id, global_var, value_id,
                         read_context->user_data));
            break;
          }

          case PN_GLOBALVAR_CODE_COMPOUND: {
            PN_CHECK(global_var);
            global_var->num_initializers =
                pn_record_read_int32(&reader, "num_initializers");
            PN_CALLBACK(
                read_context, globalvar_compound,
                (module, global_var_id, global_var,
                 global_var->num_initializers, read_context->user_data));
            break;
          }

          case PN_GLOBALVAR_CODE_ZEROFILL: {
            PN_CHECK(global_var);
            PN_CHECK(initializer_id < global_var->num_initializers);
            initializer_id++;
            uint32_t num_bytes = pn_record_read_uint32(&reader, "num_bytes");

            pn_memory_zerofill(memory, data_offset, num_bytes);
            data_offset += num_bytes;

            PN_CALLBACK(read_context, globalvar_zerofill,
                        (module, global_var_id, global_var, num_bytes,
                         read_context->user_data));
            break;
          }

          case PN_GLOBALVAR_CODE_DATA: {
            PN_CHECK(global_var);
            PN_CHECK(initializer_id < global_var->num_initializers);
            initializer_id++;
            uint32_t num_bytes = 0;
            uint32_t data_start = data_offset;
            uint32_t value;

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
              data8[data_offset++] = value;
              num_bytes++;
            }

            PN_CALLBACK(read_context, globalvar_data,
                        (module, global_var_id, global_var, &data8[data_start],
                         num_bytes, read_context->user_data));
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

            PN_CALLBACK(read_context, globalvar_reloc,
                        (module, global_var_id, global_var, index, addend,
                         read_context->user_data));

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

            PN_CALLBACK(read_context, globalvar_count,
                        (module, num_global_vars, read_context->user_data));
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

static void pn_value_symtab_block_read(PNReadContext* read_context,
                                       PNModule* module,
                                       PNBlockInfoContext* context,
                                       PNBitStream* bs) {
  PN_BEGIN_TIME(VALUE_SYMTAB_BLOCK_READ);
  PN_CALLBACK(read_context, before_value_symtab_block,
              (module, read_context->user_data));
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  PN_CHECK(codelen <= 32);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNAbbrevs abbrevs = {};
  pn_block_info_context_copy_abbrevs_for_block_id(
      &module->temp_allocator, context, PN_BLOCKID_VALUE_SYMTAB, &abbrevs);

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_CALLBACK(read_context, after_value_symtab_block,
                    (module, read_context->user_data));
        pn_bitstream_align_32(bs);
        PN_END_TIME(VALUE_SYMTAB_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in valuesymtab_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        PNAbbrev* abbrev =
            pn_abbrev_read(&module->temp_allocator, bs, &abbrevs);
        PNAbbrevId abbrev_id = abbrev - abbrevs.abbrevs;
        PN_CALLBACK(
            read_context, define_abbrev,
            (module, abbrev_id, abbrev, PN_FALSE, read_context->user_data));
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
            pn_module_value_id_check(module, value_id);
            uint32_t len = pn_record_num_values_left(&reader);
            char* name = NULL;
            if (len) {
              name = pn_allocator_alloc(&module->allocator, len + 1, 1);
              uint32_t i;
              for (i = 0; i < len; ++i) {
                name[i] = pn_record_read_int32(&reader, "name char");
              }
              name[len] = 0;
            }

            PN_CALLBACK(read_context, value_symtab_entry,
                        (module, value_id, name, read_context->user_data));

            PNValue* value = &module->values[value_id];
            if (value->code == PN_VALUE_CODE_FUNCTION) {
              PNFunctionId function_id = value->index;
              PNFunction* function = &module->functions[function_id];
              function->name = name;

#define PN_INTRINSIC_CHECK(i_enum, i_name)                                 \
  if (strcmp(name, i_name) == 0) {                                         \
    module->known_functions[PN_INTRINSIC_##i_enum] = function_id;          \
    function->intrinsic_id = PN_INTRINSIC_##i_enum;                        \
    PN_CALLBACK(                                                           \
        read_context, value_symtab_intrinsic,                              \
        (module, PN_INTRINSIC_##i_enum, i_name, read_context->user_data)); \
  } else

              PN_FOREACH_INTRINSIC(
                  PN_INTRINSIC_CHECK) { /* Unknown function name */
              }

#undef PN_INTRINSIC_CHECK
            }

            break;
          }

          case PN_VALUESYMBTAB_CODE_BBENTRY:
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

static void pn_constants_block_read(PNReadContext* read_context,
                                    PNModule* module,
                                    PNFunction* function,
                                    PNBlockInfoContext* context,
                                    PNBitStream* bs) {
  PN_BEGIN_TIME(CONSTANTS_BLOCK_READ);
  PN_CALLBACK(read_context, before_constants_block,
              (module, function, read_context->user_data));
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  PN_CHECK(codelen <= 32);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNAbbrevs abbrevs = {};
  pn_block_info_context_copy_abbrevs_for_block_id(
      &module->temp_allocator, context, PN_BLOCKID_CONSTANTS, &abbrevs);

  PNTypeId cur_type_id = -1;
  PNBasicType cur_basic_type = PN_BASIC_TYPE_VOID;
  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_CALLBACK(read_context, after_constants_block,
                    (module, function, read_context->user_data));
        pn_bitstream_align_32(bs);
        PN_END_TIME(CONSTANTS_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK:
        PN_FATAL("unexpected subblock in constants_block\n");

      case PN_ENTRY_DEFINE_ABBREV: {
        PNAbbrev* abbrev =
            pn_abbrev_read(&module->temp_allocator, bs, &abbrevs);
        PNAbbrevId abbrev_id = abbrev - abbrevs.abbrevs;
        PN_CALLBACK(
            read_context, define_abbrev,
            (module, abbrev_id, abbrev, PN_FALSE, read_context->user_data));
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
            pn_type_id_check(module, cur_type_id);
            cur_basic_type = module->types[cur_type_id].basic_type;
            PN_CALLBACK(
                read_context, constants_settype,
                (module, function, cur_type_id, read_context->user_data));
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

            PN_CALLBACK(read_context, constants_value,
                        (module, function, constant_id, constant, value_id,
                         read_context->user_data));
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

                PN_CALLBACK(read_context, constants_value,
                            (module, function, constant_id, constant, value_id,
                             read_context->user_data));
                break;
              }

              case PN_BASIC_TYPE_INT64: {
                int64_t data =
                    pn_record_read_decoded_int64(&reader, "integer64 value");
                constant->value.i64 = data;

                PN_CALLBACK(read_context, constants_value,
                            (module, function, constant_id, constant, value_id,
                             read_context->user_data));
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
                break;
              }

              case PN_BASIC_TYPE_DOUBLE: {
                double data = pn_record_read_double(&reader, "double value");
                constant->value.f64 = data;
                break;
              }

              default:
                PN_UNREACHABLE();
                break;
            }

            PN_CALLBACK(read_context, constants_value,
                        (module, function, constant_id, constant, value_id,
                         read_context->user_data));

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

static void* pn_basic_block_append_instruction(PNModule* module,
                                               PNBasicBlock* bb,
                                               uint32_t instruction_size) {
  void* p = pn_allocator_allocz(&module->temp_allocator, instruction_size,
                                PN_DEFAULT_ALIGN);
  if (bb->last_instruction) {
    bb->last_instruction->next = p;
  } else {
    bb->instructions = p;
  }

  bb->last_instruction = p;
  bb->num_instructions++;
  return p;
}

#define PN_BASIC_BLOCK_APPEND_INSTRUCTION(type, module, bb) \
  (type*) pn_basic_block_append_instruction(module, bb, sizeof(type))

#if PN_CALCULATE_PRED_BBS

#define PN_BASIC_BLOCK_LIST_APPEND(module, bb_list, num_els, bb_id) \
  pn_basic_block_list_append(module, bb_list, num_els, bb_id)

static void pn_basic_block_list_append(PNModule* module,
                                       PNBasicBlockId** bb_list,
                                       uint32_t* num_els,
                                       PNBasicBlockId bb_id) {
  pn_allocator_realloc_add(&module->allocator, (void**)bb_list,
                           sizeof(PNBasicBlockId), sizeof(PNBasicBlockId));
  (*bb_list)[(*num_els)++] = bb_id;
}

#else

#define PN_BASIC_BLOCK_LIST_APPEND(module, bb_list, num_els, bb_id) (void)(0)

#endif /* PN_CALCULATE_PRED_BBS */

static void pn_function_block_read(PNReadContext* read_context,
                                   PNModule* module,
                                   PNBlockInfoContext* context,
                                   PNBitStream* bs,
                                   PNFunctionId function_id) {
  PN_BEGIN_TIME(FUNCTION_BLOCK_READ);
  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  PN_CHECK(codelen <= 32);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNAbbrevs abbrevs = {};
  pn_block_info_context_copy_abbrevs_for_block_id(
      &module->temp_allocator, context, PN_BLOCKID_FUNCTION, &abbrevs);

  PNFunction* function = &module->functions[function_id];
  PN_CALLBACK(read_context, before_function_block,
              (module, function_id, function, read_context->user_data));

  PNType* type = &module->types[function->type_id];
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
        pn_function_calculate_uses(module, function);
        pn_function_calculate_phi_assigns(module, function);
#if PN_CALCULATE_PRED_BBS
        pn_function_calculate_pred_bbs(module, function);
#endif /* PN_CALCULATE_PRED_BBS */
#if PN_CALCULATE_LOOPS
        pn_function_calculate_loops(module, function);
#endif /* PN_CALCULATE_LOOPS */
#if PN_CALCULATE_LIVENESS
        pn_function_calculate_liveness(module, function);
#endif /* PN_CALCULATE_LIVENESS */
        pn_function_calculate_opcodes(module, function);

        PN_CALLBACK(read_context, after_function_block,
                    (module, function_id, function, read_context->user_data));
        pn_bitstream_align_32(bs);
        PN_END_TIME(FUNCTION_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK: {
        uint32_t id = pn_bitstream_read_vbr(bs, 8);
        PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);
        switch (id) {
          case PN_BLOCKID_CONSTANTS:
            pn_constants_block_read(read_context, module, function, context,
                                    bs);
            break;

          case PN_BLOCKID_VALUE_SYMTAB:
            pn_value_symtab_block_read(read_context, module, context, bs);
            break;

          default:
            PN_FATAL("bad block id %d\n", id);
        }
        pn_allocator_reset_to_mark(&module->temp_allocator, mark);
        break;
      }

      case PN_ENTRY_DEFINE_ABBREV: {
        PNAbbrev* abbrev =
            pn_abbrev_read(&module->temp_allocator, bs, &abbrevs);
        PNAbbrevId abbrev_id = abbrev - abbrevs.abbrevs;
        PN_CALLBACK(
            read_context, define_abbrev,
            (module, abbrev_id, abbrev, PN_FALSE, read_context->user_data));
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
          PN_CALLBACK(read_context, function_numblocks,
                      (module, function_id, function, function->num_bbs,
                       read_context->user_data));
          break;
        }

        if (prev_bb_id != cur_bb_id) {
          PN_CHECK(cur_bb_id < function->num_bbs);
          prev_bb_id = cur_bb_id;
          cur_bb = &function->bbs[cur_bb_id];
#if PN_CALCULATE_LOOPS
          cur_bb->loop_header_id = PN_INVALID_BB_ID;
#endif /* PN_CALCULATE_LOOPS */
#if PN_CALCULATE_LIVENESS
          cur_bb->first_def_id = PN_INVALID_VALUE_ID;
          cur_bb->last_def_id = PN_INVALID_VALUE_ID;
#endif /* PN_CALCULATE_LIVENESS */

          first_bb_value_id = pn_function_num_values(module, function);
          num_bbs++;
        }

        PNInstruction* instruction = NULL;

        switch (code) {
          case PN_FUNCTION_CODE_DECLAREBLOCKS:
            /* Handled above so we only print the basic block index when listing
             * instructions */
            break;

          case PN_FUNCTION_CODE_INST_BINOP: {
            PNInstructionBinop* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionBinop, module, cur_bb);
            instruction = &inst->base;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            /* Fix later, when all values are defined. */
            value->type_id = PN_INVALID_TYPE_ID;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->value0_id = pn_record_read_uint32(&reader, "value 0");
            inst->value1_id = pn_record_read_uint32(&reader, "value 1");
            inst->binop_opcode = pn_record_read_int32(&reader, "opcode");
            inst->flags = 0;

            if (context->use_relative_ids) {
              inst->value0_id = rel_id - inst->value0_id;
              inst->value1_id = rel_id - inst->value1_id;
            }

            /* optional */
            pn_record_try_read_int32(&reader, &inst->flags);
            break;
          }

          case PN_FUNCTION_CODE_INST_CAST: {
            PNInstructionCast* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionCast, module, cur_bb);
            instruction = &inst->base;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->value_id = pn_record_read_uint32(&reader, "value");
            inst->type_id = pn_record_read_uint32(&reader, "type_id");
            inst->cast_opcode = pn_record_read_int32(&reader, "opcode");
            pn_type_id_check(module, inst->type_id);

            value->type_id = inst->type_id;

            if (context->use_relative_ids) {
              inst->value_id = rel_id - inst->value_id;
            }
            break;
          }

          case PN_FUNCTION_CODE_INST_RET: {
            PNInstructionRet* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionRet, module, cur_bb);
            inst->base.code = code;
            inst->value_id = PN_INVALID_VALUE_ID;

            uint32_t value_id;
            if (pn_record_try_read_uint32(&reader, &value_id)) {
              inst->value_id = value_id;
              if (context->use_relative_ids) {
                inst->value_id = rel_id - inst->value_id;
              }
            }

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_BR: {
            PNInstructionBr* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionBr, module, cur_bb);
            instruction = &inst->base;
            inst->base.code = code;
            inst->true_bb_id = pn_record_read_uint32(&reader, "true_bb");
            inst->false_bb_id = PN_INVALID_BB_ID;

            PN_BASIC_BLOCK_LIST_APPEND(module, &cur_bb->succ_bb_ids,
                                       &cur_bb->num_succ_bbs, inst->true_bb_id);

            if (pn_record_try_read_uint16(&reader, &inst->false_bb_id)) {
              inst->value_id = pn_record_read_uint32(&reader, "value");
              if (context->use_relative_ids) {
                inst->value_id = rel_id - inst->value_id;
              }

              PN_BASIC_BLOCK_LIST_APPEND(module, &cur_bb->succ_bb_ids,
                                         &cur_bb->num_succ_bbs,
                                         inst->false_bb_id);
            } else {
              inst->value_id = PN_INVALID_VALUE_ID;
            }

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_SWITCH: {
            PNInstructionSwitch* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionSwitch, module, cur_bb);
            instruction = &inst->base;

            inst->type_id = pn_record_read_uint32(&reader, "type_id");
            inst->value_id = pn_record_read_uint32(&reader, "value");
            inst->default_bb_id = pn_record_read_uint32(&reader, "default bb");
            pn_type_id_check(module, inst->type_id);

            PN_BASIC_BLOCK_LIST_APPEND(module, &cur_bb->succ_bb_ids,
                                       &cur_bb->num_succ_bbs,
                                       inst->default_bb_id);

            if (context->use_relative_ids) {
              inst->value_id = rel_id - inst->value_id;
            }

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
                    &module->temp_allocator, (void**)&inst->cases,
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

              PN_BASIC_BLOCK_LIST_APPEND(module, &cur_bb->succ_bb_ids,
                                         &cur_bb->num_succ_bbs, bb_id);
            }

            inst->num_cases = total_values;

            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_UNREACHABLE: {
            PNInstructionUnreachable* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionUnreachable, module, cur_bb);
            instruction = &inst->base;
            inst->base.code = code;
            is_terminator = PN_TRUE;
            break;
          }

          case PN_FUNCTION_CODE_INST_PHI: {
            PNInstructionPhi* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionPhi, module, cur_bb);
            instruction = &inst->base;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->type_id = pn_record_read_int32(&reader, "type_id");
            inst->num_incoming = 0;
            pn_type_id_check(module, inst->type_id);

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
                    &module->temp_allocator, (void**)&inst->incoming,
                    sizeof(PNPhiIncoming), PN_DEFAULT_ALIGN);
                PNPhiIncoming* incoming = &inst->incoming[inst->num_incoming++];
                incoming->bb_id = bb;
                incoming->value_id = value;
              }
            }
            break;
          }

          case PN_FUNCTION_CODE_INST_ALLOCA: {
            PNInstructionAlloca* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionAlloca, module, cur_bb);
            instruction = &inst->base;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->type_id = pn_module_find_pointer_type(module);

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->size_id = pn_record_read_uint32(&reader, "size");
            inst->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;
            PN_CHECK(pn_is_power_of_two(inst->alignment));

            if (context->use_relative_ids) {
              inst->size_id = rel_id - inst->size_id;
            }
            break;
          }

          case PN_FUNCTION_CODE_INST_LOAD: {
            PNInstructionLoad* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionLoad, module, cur_bb);
            instruction = &inst->base;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->src_id = pn_record_read_uint32(&reader, "src");
            inst->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;
            inst->type_id = pn_record_read_int32(&reader, "type_id");
            PN_CHECK(pn_is_power_of_two(inst->alignment));

            value->type_id = inst->type_id;

            if (context->use_relative_ids) {
              inst->src_id = rel_id - inst->src_id;
            }
            break;
          }

          case PN_FUNCTION_CODE_INST_STORE: {
            PNInstructionStore* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionStore, module, cur_bb);
            instruction = &inst->base;

            inst->base.code = code;
            inst->dest_id = pn_record_read_uint32(&reader, "dest");
            inst->value_id = pn_record_read_uint32(&reader, "value");
            inst->alignment =
                (1 << pn_record_read_int32(&reader, "alignment")) >> 1;
            PN_CHECK(pn_is_power_of_two(inst->alignment));

            if (context->use_relative_ids) {
              inst->dest_id = rel_id - inst->dest_id;
              inst->value_id = rel_id - inst->value_id;
            }
            break;
          }

          case PN_FUNCTION_CODE_INST_CMP2: {
            PNInstructionCmp2* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionCmp2, module, cur_bb);
            instruction = &inst->base;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            value->type_id = pn_module_find_integer_type(module, 1);

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->value0_id = pn_record_read_uint32(&reader, "value 0");
            inst->value1_id = pn_record_read_uint32(&reader, "value 1");
            inst->cmp2_opcode = pn_record_read_int32(&reader, "opcode");

            if (context->use_relative_ids) {
              inst->value0_id = rel_id - inst->value0_id;
              inst->value1_id = rel_id - inst->value1_id;
            }
            break;
          }

          case PN_FUNCTION_CODE_INST_VSELECT: {
            PNInstructionVselect* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionVselect, module, cur_bb);
            instruction = &inst->base;

            PNValueId value_id;
            PNValue* value =
                pn_function_append_value(module, function, &value_id);
            value->code = PN_VALUE_CODE_LOCAL_VAR;
            /* Fix later, when all values are defined. */
            value->type_id = PN_INVALID_TYPE_ID;

            inst->base.code = code;
            inst->result_value_id = value_id;
            inst->true_value_id = pn_record_read_uint32(&reader, "true_value");
            inst->false_value_id =
                pn_record_read_uint32(&reader, "false_value");
            inst->cond_id = pn_record_read_uint32(&reader, "cond");

            if (context->use_relative_ids) {
              inst->true_value_id = rel_id - inst->true_value_id;
              inst->false_value_id = rel_id - inst->false_value_id;
              inst->cond_id = rel_id - inst->cond_id;
            }
            break;
          }

          case PN_FUNCTION_CODE_INST_FORWARDTYPEREF: {
            PNInstructionForwardtyperef* inst =
                PN_BASIC_BLOCK_APPEND_INSTRUCTION(PNInstructionForwardtyperef,
                                                  module, cur_bb);
            instruction = &inst->base;

            inst->base.code = code;
            inst->value_id = pn_record_read_int32(&reader, "value");
            inst->type_id = pn_record_read_int32(&reader, "type");
            pn_type_id_check(module, inst->type_id);
            break;
          }

          case PN_FUNCTION_CODE_INST_CALL:
          case PN_FUNCTION_CODE_INST_CALL_INDIRECT: {
            PNInstructionCall* inst = PN_BASIC_BLOCK_APPEND_INSTRUCTION(
                PNInstructionCall, module, cur_bb);
            instruction = &inst->base;

            inst->base.code = code;
            inst->is_indirect = code == PN_FUNCTION_CODE_INST_CALL_INDIRECT;
            int32_t cc_info = pn_record_read_int32(&reader, "cc_info");
            inst->is_tail_call = cc_info & 1;
            inst->calling_convention = cc_info >> 1;
            inst->callee_id = pn_record_read_uint32(&reader, "callee");

            if (context->use_relative_ids) {
              inst->callee_id = rel_id - inst->callee_id;
            }

            PNTypeId type_id;
            if (inst->is_indirect) {
              inst->return_type_id =
                  pn_record_read_int32(&reader, "return_type");
              pn_type_id_check(module, inst->return_type_id);
            } else {
              pn_module_value_id_check(module, inst->callee_id);
              PNValue* function_value = &module->values[inst->callee_id];
              assert(function_value->code == PN_VALUE_CODE_FUNCTION);
              PNFunction* called_function =
                  &module->functions[function_value->index];
              type_id = called_function->type_id;
              PNType* function_type = &module->types[type_id];
              assert(function_type->code == PN_TYPE_CODE_FUNCTION);
              inst->return_type_id = function_type->return_type;
            }

            PNType* return_type = &module->types[inst->return_type_id];
            PNBool is_return_type_void = return_type->code == PN_TYPE_CODE_VOID;
            PNValueId value_id;
            if (!is_return_type_void) {
              PNValue* value =
                  pn_function_append_value(module, function, &value_id);
              value->code = PN_VALUE_CODE_LOCAL_VAR;
              value->type_id = inst->return_type_id;

              inst->result_value_id = value_id;
            } else {
              value_id = pn_function_num_values(module, function);
              inst->result_value_id = PN_INVALID_VALUE_ID;
            }

            inst->num_args = pn_record_num_values_left(&reader);
            inst->arg_ids = pn_allocator_alloc(
                &module->temp_allocator, inst->num_args * sizeof(PNValueId),
                sizeof(PNValueId));

            uint32_t n;
            for (n = 0; n < inst->num_args; ++n) {
              int32_t arg = pn_record_read_int32(&reader, "arg");
              if (context->use_relative_ids) {
                arg = value_id - arg;
              }
              inst->arg_ids[n] = arg;
            }
            break;
          }

          default:
            PN_FATAL("bad record code: %d.\n", code);
        }

        if (code != PN_FUNCTION_CODE_DECLAREBLOCKS) {
          PN_CALLBACK(read_context, function_instruction,
                      (module, function_id, function, instruction,
                       read_context->user_data));
          function->num_instructions++;
        }

        if (is_terminator) {
          PNValueId last_bb_value_id = pn_function_num_values(module, function);
          if (last_bb_value_id != first_bb_value_id) {
#if PN_CALCULATE_LIVENESS
            cur_bb->first_def_id = first_bb_value_id;
            cur_bb->last_def_id = last_bb_value_id - 1;
#endif /* PN_CALCULATE_LIVENESS */
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

static void pn_module_block_read(PNReadContext* read_context,
                                 PNModule* module,
                                 PNBlockInfoContext* context,
                                 PNBitStream* bs) {
  PN_BEGIN_TIME(MODULE_BLOCK_READ);
  PN_CALLBACK(read_context, before_module_block,
              (module, read_context->user_data));

  uint32_t codelen = pn_bitstream_read_vbr(bs, 4);
  PN_CHECK(codelen <= 32);
  pn_bitstream_align_32(bs);
  pn_bitstream_read(bs, 32); /* num words */

  PNAbbrevs abbrevs = {};
  PNFunctionId function_id = 0;

  while (!pn_bitstream_at_end(bs)) {
    uint32_t entry = pn_bitstream_read(bs, codelen);
    switch (entry) {
      case PN_ENTRY_END_BLOCK:
        PN_CALLBACK(read_context, after_module_block,
                    (module, read_context->user_data));
        pn_bitstream_align_32(bs);
        PN_END_TIME(MODULE_BLOCK_READ);
        return;

      case PN_ENTRY_SUBBLOCK: {
        uint32_t id = pn_bitstream_read_vbr(bs, 8);

        PNAllocatorMark mark = pn_allocator_mark(&module->temp_allocator);
        switch (id) {
          case PN_BLOCKID_BLOCKINFO:
            pn_blockinfo_block_read(read_context, module, context, bs);
            break;
          case PN_BLOCKID_TYPE:
            pn_type_block_read(read_context, module, context, bs);
            break;
          case PN_BLOCKID_GLOBALVAR:
            pn_globalvar_block_read(read_context, module, context, bs);
            break;
          case PN_BLOCKID_VALUE_SYMTAB:
            pn_value_symtab_block_read(read_context, module, context, bs);
            break;
          case PN_BLOCKID_FUNCTION: {
            while (function_id < module->num_functions &&
                   module->functions[function_id].is_proto) {
              function_id++;
            }

            pn_function_id_check(module, function_id);
            pn_function_block_read(read_context, module, context, bs,
                                   function_id);
            function_id++;
            break;
          }
          default:
            PN_FATAL("bad block id %d\n", id);
        }
        pn_allocator_reset_to_mark(&module->temp_allocator, mark);
        break;
      }

      case PN_ENTRY_DEFINE_ABBREV: {
        PNAbbrev* abbrev =
            pn_abbrev_read(&module->temp_allocator, bs, &abbrevs);
        PNAbbrevId abbrev_id = abbrev - abbrevs.abbrevs;
        PN_CALLBACK(
            read_context, define_abbrev,
            (module, abbrev_id, abbrev, PN_FALSE, read_context->user_data));
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
            PN_CALLBACK(read_context, module_version,
                        (module, module->version, read_context->user_data));
            break;
          }

          case PN_MODULE_CODE_FUNCTION: {
            PNFunction* function = pn_allocator_realloc_add(
                &module->allocator, (void**)&module->functions,
                sizeof(PNFunction), PN_DEFAULT_ALIGN);
            PNFunctionId function_id = module->num_functions++;

            function->name = NULL;
            function->type_id = pn_record_read_int32(&reader, "type_id");
            pn_type_id_check(module, function->type_id);
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
            function->num_instructions = 0;
#if PN_CALCULATE_LIVENESS
            function->value_liveness_range = NULL;
#endif /* PN_CALCULATE_LIVENESS */

            /* Cache number of arguments to function */
            PNType* function_type = &module->types[function->type_id];
            PN_CHECK(function_type->code == PN_TYPE_CODE_FUNCTION);
            function->num_args = function_type->num_args;

            PNValueId value_id;
            PNValue* value = pn_module_append_value(module, &value_id);
            value->code = PN_VALUE_CODE_FUNCTION;
            value->type_id = function->type_id;
            value->index = function_id;

            PN_CALLBACK(read_context, module_function,
                        (module, function_id, function, value_id,
                         read_context->user_data));
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

void pn_module_read(PNReadContext* read_context,
                    PNModule* module,
                    PNBitStream* bs) {
  pn_header_read(bs);
  uint32_t entry = pn_bitstream_read(bs, 2);
  if (entry != PN_ENTRY_SUBBLOCK) {
    PN_FATAL("expected subblock at top-level\n");
  }

  PNBlockId block_id = pn_bitstream_read_vbr(bs, 8);
  PN_CHECK(block_id == PN_BLOCKID_MODULE);

  PNBlockInfoContext context = {};
  pn_module_block_read(read_context, module, &context, bs);
}

#endif /* PN_READ_H_ */
