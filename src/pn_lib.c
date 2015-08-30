/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "pn_lib.h"

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/**** GLOBAL VARIABLES ********************************************************/

static int g_pn_verbose;
static PNBool g_pn_dedupe_phi_nodes = PN_TRUE;
static uint32_t g_pn_opcode_count[PN_MAX_OPCODE];
#if PN_PPAPI
static PNBool g_pn_ppapi = PN_FALSE;
#endif /* PN_PPAPI */

#if PN_TRACING
static const char* g_pn_trace_function_filter;
static int g_pn_trace_indent;
#define PN_TRACE_DEFINE(name, flag) static PNBool g_pn_trace_##name;
PN_FOREACH_TRACE(PN_TRACE_DEFINE)
#undef PN_TRACE_DEFINE
#endif /* PN_TRACING */

#if PN_TIMERS
static struct timespec g_pn_timer_times[PN_NUM_TIMERS];
#endif /* PN_TIMERS */

/**** SOURCES *****************************************************************/

#include "pn_bits.h"
#include "pn_malloc.h"
#include "pn_timespec.h"
#include "pn_allocator.h"
#include "pn_bitset.h"
#include "pn_bitstream.h"
#include "pn_record.h"
#include "pn_abbrev.h"
#include "pn_memory.h"
#include "pn_module.h"
#include "pn_function.h"
#include "pn_trace.h"
#include "pn_calculate_result_value_types.h"
#include "pn_calculate_opcodes.h"
#include "pn_calculate_uses.h"
#include "pn_calculate_pred_bbs.h"
#include "pn_calculate_phi_assigns.h"
#include "pn_calculate_liveness.h"
#include "pn_read.h"
#include "pn_executor.h"
#include "pn_builtins.h"
#include "pn_ppapi.h"

static void pn_lib_dummy(void) {
  g_pn_trace_FLAGS = PN_FALSE;
}
