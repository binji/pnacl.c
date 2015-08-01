/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_MODULE_H_
#define PN_MODULE_H_

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

static PNFunction* pn_module_get_function(PNModule* module,
                                          PNFunctionId function_id) {
  if (function_id >= module->num_functions) {
    PN_FATAL("accessing invalid function %d (max %d)\n", function_id,
             module->num_functions);
  }

  return &module->functions[function_id];
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

#endif /* PN_MODULE_H_ */
