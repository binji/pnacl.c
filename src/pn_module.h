/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_MODULE_H_
#define PN_MODULE_H_

void pn_module_init(PNModule* module, PNMemory* memory) {
  memset(module, 0, sizeof(PNModule));
  module->memory = memory;
  pn_allocator_init(&module->allocator, PN_MIN_CHUNKSIZE, "module");
  pn_allocator_init(&module->value_allocator, PN_MIN_CHUNKSIZE, "value");
  pn_allocator_init(&module->instruction_allocator, PN_MIN_CHUNKSIZE,
                    "instruction");
  pn_allocator_init(&module->temp_allocator, PN_MIN_CHUNKSIZE, "temp");
}

void pn_module_reset(PNModule* module) {
  PNModule copy = *module;
  /* Keep the allocators and memory, clear everything else */
  memset(module, 0, sizeof(PNModule));
  module->allocator = copy.allocator;
  module->value_allocator = copy.value_allocator;
  module->instruction_allocator = copy.instruction_allocator;
  module->temp_allocator = copy.temp_allocator;
  module->memory = copy.memory;

  /* Reset the allocators */
  pn_allocator_reset(&module->allocator);
  pn_allocator_reset(&module->value_allocator);
  pn_allocator_reset(&module->instruction_allocator);
  pn_allocator_reset(&module->temp_allocator);

  /* Reset the memory */
  pn_memory_reset(module->memory);
}

static void pn_type_id_check(PNModule* module, PNTypeId type_id) {
  PN_CHECK(type_id < module->num_types);
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

  PN_FATAL("No integer type found of width %d.", width);
  return PN_INVALID_TYPE_ID;
}

static PNTypeId pn_module_find_pointer_type(PNModule* module) {
  return pn_module_find_integer_type(module, 32);
}

static void pn_function_id_check(PNModule* module, PNFunctionId function_id) {
  PN_CHECK(function_id < module->num_functions);
}

static void pn_global_var_id_check(PNModule* module,
                                   PNGlobalVarId global_var_id) {
  PN_CHECK(global_var_id < module->num_global_vars);
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

#endif /* PN_MODULE_H_ */
