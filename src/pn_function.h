/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_FUNCTION_H_
#define PN_FUNCTION_H_

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

static uint32_t pn_function_num_values(PNModule* module, PNFunction* function) {
  return module->num_values + function->num_values;
}

#endif /* PN_FUNCTION_H_ */
