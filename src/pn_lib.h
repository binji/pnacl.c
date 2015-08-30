/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_LIB_H_
#define PN_LIB_H_

#include "pnacl.h"

void pn_memory_init(PNMemory* memory, uint32_t size);
void pn_memory_init_startinfo(PNMemory* memory, char** argv, char** envp);
void pn_memory_reset(PNMemory* memory);

void pn_module_init(PNModule* module, PNMemory* memory);
void pn_module_reset(PNModule* module);
void pn_module_read(PNModule* module, PNBitStream* bs);

void pn_executor_init(PNExecutor* executor, PNModule* module);
PNThread* pn_executor_run_step(PNExecutor* executor, PNThread* thread);
void pn_executor_run(PNExecutor* executor);

#endif /* PN_LIB_H_ */
