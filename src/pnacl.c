/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "pnacl.h"

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

/* Some crazy system where this isn't true? */
PN_STATIC_ASSERT(sizeof(float) == sizeof(uint32_t));
PN_STATIC_ASSERT(sizeof(double) == sizeof(uint64_t));

/* These asserts allow us to ensure that for all atomic ops, the types are
 * consecutive and the types are ordered i8, i16, i32, i64. We can then safely
 * do basic arithmetic on the enum value. */
#define PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(opcode)                   \
  PN_STATIC_ASSERT(                                                 \
      PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I8 <          \
          PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I16 &&    \
      PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I16 <         \
          PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I32 &&    \
      PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I32 <         \
          PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I64 &&    \
      PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I64 -         \
              PN_OPCODE_INTRINSIC_LLVM_NACL_ATOMIC_##opcode##_I8 == \
          3) /* no semicolon */
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(RMW);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(ADD);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(SUB);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(AND);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(OR);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(XOR);
PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES(EXCHANGE);
#undef PN_ASSERT_ATOMIC_SEQUENTIAL_TYPES

/**** GLOBAL VARIABLES ********************************************************/

static int g_pn_verbose;
static const char* g_pn_filename;
static char** g_pn_argv;
static char** g_pn_environ;
static uint32_t g_pn_memory_size = PN_DEFAULT_MEMORY_SIZE;
static PNBool g_pn_dedupe_phi_nodes = PN_TRUE;
static PNBool g_pn_print_named_functions;
#if PN_CALCULATE_PRED_BBS
static char* g_pn_print_block_graph_function;
#endif /* PN_CALCULATE_PRED_BBS */
static PNBool g_pn_print_stats;
static PNBool g_pn_print_opcode_counts;
static PNBool g_pn_run = PN_TRUE;
static uint32_t g_pn_opcode_count[PN_MAX_OPCODE];
static PNBool g_pn_repeat_load_times = 1;
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
static PNBool g_pn_print_time;
static PNBool g_pn_print_time_as_zero;
#endif /* PN_TIMERS */

static const char* g_pn_opcode_names[] = {
#define PN_OPCODE(e) #e,
  PN_FOREACH_OPCODE(PN_OPCODE)
#undef PN_OPCODE

#define PN_INTRINSIC_OPCODE(e, name) "INTRINSIC_" #e,
  PN_FOREACH_INTRINSIC(PN_INTRINSIC_OPCODE)
#undef PN_INTRINSIC_OPCODE
};

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
#include "pn_calculate_loops.h"
#include "pn_calculate_liveness.h"
#include "pn_read.h"
#include "pn_executor.h"
#include "pn_builtins.h"
#include "pn_ppapi.h"

/* Option parsing, environment variables */

enum {
  PN_FLAG_VERBOSE,
  PN_FLAG_HELP,
  PN_FLAG_MEMORY_SIZE,
  PN_FLAG_NO_RUN,
#if PN_PPAPI
  PN_FLAG_PPAPI,
#endif /* PN_PPAPI */
  PN_FLAG_ENV,
  PN_FLAG_USE_HOST_ENV,
  PN_FLAG_NO_DEDUPE_PHI_NODES,
#if PN_TRACING
  PN_FLAG_TRACE_ALL,
  PN_FLAG_TRACE_BLOCK,
  PN_FLAG_TRACE_BCDIS,
#define PN_TRACE_FLAGS(name, flag) PN_FLAG_TRACE_##name,
  PN_FOREACH_TRACE(PN_TRACE_FLAGS)
#undef PN_TRACE_FLAGS
  PN_FLAG_TRACE_FUNCTION_FILTER,
#endif /* PN_TRACING */
  PN_FLAG_PRINT_ALL,
  PN_FLAG_PRINT_NAMED_FUNCTIONS,
#if PN_CALCULATE_PRED_BBS
  PN_FLAG_PRINT_BLOCK_GRAPH,
#endif /* PN_CALCULATE_PRED_BBS */
#if PN_TIMERS
  PN_FLAG_PRINT_TIME,
  PN_FLAG_PRINT_TIME_AS_ZERO,
#endif /* PN_TIMERS */
  PN_FLAG_PRINT_OPCODE_COUNTS,
  PN_FLAG_PRINT_STATS,
  PN_FLAG_REPEAT_LOAD,
  PN_NUM_FLAGS
};

static struct option g_pn_long_options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {"memory-size", required_argument, NULL, 'm'},
    {"no-run", no_argument, NULL, 'n'},
#if PN_PPAPI
    {"ppapi", no_argument, NULL, 0},
#endif /* PN_PPAPI */
    {"env", required_argument, NULL, 'e'},
    {"use-host-env", no_argument, NULL, 'E'},
    {"no-dedupe-phi-nodes", no_argument, NULL, 0},
#if PN_TRACING
    {"trace-all", no_argument, NULL, 't'},
    {"trace-block", no_argument, NULL, 0},
    {"trace-bcdis", no_argument, NULL, 0},
#define PN_TRACE_FLAGS(name, flag)        \
    {"trace-" flag, no_argument, NULL, 0},
    PN_FOREACH_TRACE(PN_TRACE_FLAGS)
#undef PN_TRACE_FLAGS
    {"trace-function-filter", required_argument, NULL, 0},
#endif /* PN_TRACING */
    {"print-all", no_argument, NULL, 'p'},
    {"print-named-functions", no_argument, NULL, 0},
#if PN_CALCULATE_PRED_BBS
    {"print-block-graph", required_argument, NULL, 0},
#endif /* PN_CALCULATE_PRED_BBS */
#if PN_TIMERS
    {"print-time", no_argument, NULL, 0},
    {"print-time-as-zero", no_argument, NULL, 0},
#endif /* PN_TIMERS */
    {"print-opcode-counts", no_argument, NULL, 0},
    {"print-stats", no_argument, NULL, 0},
    {"repeat-load", required_argument, NULL, 0},
    {NULL, 0, NULL, 0},
};

PN_STATIC_ASSERT(PN_NUM_FLAGS + 1 == PN_ARRAY_SIZE(g_pn_long_options));

typedef struct PNOptionHelp {
  int flag;
  const char* metavar;
  const char* help;
} PNOptionHelp;

static PNOptionHelp g_pn_option_help[] = {
    {PN_FLAG_MEMORY_SIZE, "SIZE",
     "size of runtime memory. suffixes k=1024, m=1024*1024"},
    {PN_FLAG_ENV, "KEY=VALUE", "set runtime environment variable KEY to VALUE"},
#if PN_TRACING
    {PN_FLAG_TRACE_FUNCTION_FILTER, "NAME",
     "only trace function with given name or id"},
#endif /* PN_TRACING */
#if PN_CALCULATE_PRED_BBS
    {PN_FLAG_PRINT_BLOCK_GRAPH, "NAME",
     "print the basic-block graph of a function with given name or id"},
#endif /* PN_CALCULATE_PRED_BBS */
    {PN_FLAG_REPEAT_LOAD, "TIMES",
     "number of times to repeat loading. Useful for profiling"},
    {PN_NUM_FLAGS, NULL},
};

static void pn_usage(const char* prog) {
  PN_PRINT("usage: %s [option] filename\n", prog);
  PN_PRINT("options:\n");
  struct option* opt = &g_pn_long_options[0];
  int i = 0;
  for (; opt->name; ++i, ++opt) {
    PNOptionHelp* help = NULL;

    int n = 0;
    while (g_pn_option_help[n].help) {
      if (i == g_pn_option_help[n].flag) {
        help = &g_pn_option_help[n];
        break;
      }
      n++;
    }

    if (opt->val) {
      PN_PRINT("  -%c, ", opt->val);
    } else {
      PN_PRINT("      ");
    }

    if (help && help->metavar) {
      char buf[100];
      snprintf(buf, 100, "%s=%s", opt->name, help->metavar);
      PN_PRINT("--%-30s", buf);
    } else {
      PN_PRINT("--%-30s", opt->name);
    }

    if (help) {
      PN_PRINT("%s", help->help);
    }

    PN_PRINT("\n");
  }
  exit(0);
}

static char** pn_environ_copy(char** environ) {
  char** env;
  int num_keys = 0;
  for (env = environ; *env; ++env) {
    num_keys++;
  }

  char** result = pn_calloc(num_keys + 1, sizeof(char*));
  int i = 0;
  for (env = environ; *env; ++env, ++i) {
    result[i] = pn_strdup(*env);
  }

  return result;
}

static void pn_environ_free(char** environ) {
  char** env;
  for (env = environ; *env; ++env) {
    pn_free(*env);
  }
  pn_free(environ);
}

static void pn_environ_put(char*** environ, char* value) {
  char* equals = strchr(value, '=');
  int count = equals ? equals - value : strlen(value);
  PNBool remove = equals == NULL;

  int num_keys = 0;
  if (*environ) {
    char** env;
    for (env = *environ; *env; ++env, ++num_keys) {
      if (strncasecmp(value, *env, count) != 0) {
        continue;
      }

      pn_free(*env);
      if (remove) {
        for (; *env; ++env) {
          *env = *(env + 1);
        }
      } else {
        *env = pn_strdup(value);
      }
      return;
    }
  }

  if (!remove) {
    *environ = pn_realloc(*environ, (num_keys + 2) * sizeof(char*));
    (*environ)[num_keys] = pn_strdup(value);
    (*environ)[num_keys + 1] = 0;
  }
}

static void pn_environ_put_all(char*** environ, char** environ_to_put) {
  char** env;
  for (env = environ_to_put; *env; ++env) {
    pn_environ_put(environ, *env);
  }
}

static void pn_environ_print(char** environ) {
  if (environ) {
    char** env;
    for (env = environ; *env; ++env) {
      PN_PRINT("  %s\n", *env);
    }
  }
}

static void pn_options_parse(int argc, char** argv, char** env) {
  int c;
  int option_index;
  char** environ_copy = pn_environ_copy(env);

  while (1) {
    c = getopt_long(argc, argv, "vm:ne:Ehtp", g_pn_long_options, &option_index);
    if (c == -1) {
      break;
    }

  redo_switch:
    switch (c) {
      case 0:
        c = g_pn_long_options[option_index].val;
        if (c) {
          goto redo_switch;
        }

        switch (option_index) {
          case PN_FLAG_VERBOSE:
          case PN_FLAG_HELP:
          case PN_FLAG_MEMORY_SIZE:
          case PN_FLAG_NO_RUN:
          case PN_FLAG_ENV:
          case PN_FLAG_USE_HOST_ENV:
#if PN_TRACING
          case PN_FLAG_TRACE_ALL:
#endif /* PN_TRACING */
          case PN_FLAG_PRINT_ALL:
            /* Handled above by goto */
            PN_UNREACHABLE();

#if PN_PPAPI
          case PN_FLAG_PPAPI:
            g_pn_ppapi = PN_TRUE;
            break;
#endif /* PN_PPAPI */

          case PN_FLAG_NO_DEDUPE_PHI_NODES:
            g_pn_dedupe_phi_nodes = PN_FALSE;
            break;

#if PN_TRACING
          case PN_FLAG_TRACE_BCDIS:
#define PN_TRACE_UNSET(name, flag) g_pn_trace_##name = PN_FALSE;
            PN_FOREACH_TRACE(PN_TRACE_UNSET)
#undef PN_TRACE_UNSET
            g_pn_trace_ABBREV = PN_TRUE;
            g_pn_trace_BLOCKINFO_BLOCK = PN_TRUE;
            g_pn_trace_TYPE_BLOCK = PN_TRUE;
            g_pn_trace_GLOBALVAR_BLOCK = PN_TRUE;
            g_pn_trace_VALUE_SYMTAB_BLOCK = PN_TRUE;
            g_pn_trace_CONSTANTS_BLOCK = PN_TRUE;
            g_pn_trace_FUNCTION_BLOCK = PN_TRUE;
            g_pn_trace_MODULE_BLOCK = PN_TRUE;
            g_pn_trace_INSTRUCTIONS = PN_TRUE;
            break;

          case PN_FLAG_TRACE_BLOCK:
            g_pn_trace_BLOCKINFO_BLOCK = PN_TRUE;
            g_pn_trace_TYPE_BLOCK = PN_TRUE;
            g_pn_trace_GLOBALVAR_BLOCK = PN_TRUE;
            g_pn_trace_VALUE_SYMTAB_BLOCK = PN_TRUE;
            g_pn_trace_CONSTANTS_BLOCK = PN_TRUE;
            g_pn_trace_FUNCTION_BLOCK = PN_TRUE;
            g_pn_trace_MODULE_BLOCK = PN_TRUE;
            break;

#define PN_TRACE_OPTIONS(name, flag) \
  case PN_FLAG_TRACE_##name:         \
    g_pn_trace_##name = PN_TRUE;     \
    break;
            PN_FOREACH_TRACE(PN_TRACE_OPTIONS);
#undef PN_TRACE_OPTIONS

          case PN_FLAG_TRACE_FUNCTION_FILTER:
            g_pn_trace_function_filter = optarg;
            break;

#endif /* PN_TRACING */

          case PN_FLAG_PRINT_NAMED_FUNCTIONS:
            g_pn_print_named_functions = PN_TRUE;
            break;

#if PN_CALCULATE_PRED_BBS
          case PN_FLAG_PRINT_BLOCK_GRAPH:
            g_pn_print_block_graph_function = optarg;
            break;
#endif /* PN_CALCULATE_PRED_BBS */

#if PN_TIMERS
          case PN_FLAG_PRINT_TIME:
            g_pn_print_time = PN_TRUE;
            break;

          case PN_FLAG_PRINT_TIME_AS_ZERO:
            g_pn_print_time_as_zero = PN_TRUE;
            break;
#endif /* PN_TIMERS */

          case PN_FLAG_PRINT_STATS:
            g_pn_print_stats = PN_TRUE;
            break;

          case PN_FLAG_PRINT_OPCODE_COUNTS:
            g_pn_print_opcode_counts = PN_TRUE;
            break;

          case PN_FLAG_REPEAT_LOAD: {
            char* endptr;
            errno = 0;
            long int times = strtol(optarg, &endptr, 10);
            size_t optarg_len = strlen(optarg);

            if (errno != 0 || optarg_len != (endptr - optarg)) {
              PN_FATAL("Unable to parse repeat-times flag \"%s\".\n", optarg);
            }

            g_pn_repeat_load_times = times;
            break;
          }
        }
        break;

      case 'v':
        g_pn_verbose++;
        break;

      case 'm': {
        char* endptr;
        errno = 0;
        long int size = strtol(optarg, &endptr, 10);
        size_t optarg_len = strlen(optarg);

        if (errno != 0) {
          PN_FATAL("Unable to parse memory-size flag \"%s\".\n", optarg);
        }

        if (endptr == optarg + optarg_len - 1) {
          if (*endptr == 'k' || *endptr == 'K') {
            size *= 1024;
          } else if (*endptr == 'm' || *endptr == 'M') {
            size *= 1024 * 1024;
          } else {
            PN_FATAL("Unknown suffix on memory-size \"%s\".\n", optarg);
          }
        } else if (endptr == optarg + optarg_len) {
          /* Size in bytes, do nothing */
        } else {
          PN_FATAL("Unable to parse memory-size flag \"%s\".\n", optarg);
        }

        if (size < PN_MEMORY_GUARD_SIZE) {
          PN_FATAL(
              "Cannot set memory-size (%ld) smaller than guard size (%d).\n",
              size, PN_MEMORY_GUARD_SIZE);
        }

        size = pn_align_up(size, PN_PAGESIZE);
        PN_TRACE(FLAGS, "Setting memory-size to %ld\n", size);
        g_pn_memory_size = size;
        break;
      }

      case 'n':
        g_pn_run = PN_FALSE;
        break;

      case 'e':
        pn_environ_put(&g_pn_environ, optarg);
        break;

      case 'E':
        pn_environ_put_all(&g_pn_environ, environ_copy);
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

      case 'h':
        pn_usage(argv[0]);

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
    PN_ERROR("No filename given.\n");
    pn_usage(argv[0]);
  }

  g_pn_argv = argv + optind;

#if PN_TRACING
  /* Handle flag dependencies */
  if (g_pn_trace_INSTRUCTIONS) {
    g_pn_trace_BASIC_BLOCKS = PN_TRUE;
  }
  
  if (g_pn_trace_BASIC_BLOCK_EXTRAS) {
    g_pn_trace_BASIC_BLOCKS = PN_TRUE;
  }

  if (g_pn_trace_BASIC_BLOCKS) {
    g_pn_trace_FUNCTION_BLOCK = PN_TRUE;
  }

  if (g_pn_trace_EXECUTE) {
    g_pn_trace_IRT = PN_TRUE;
    g_pn_trace_INTRINSICS = PN_TRUE;
  }
#endif /* PN_TRACING */

  pn_environ_free(environ_copy);
#if PN_TRACING
  if (g_pn_trace_FLAGS) {
    PN_PRINT("*** ARGS:\n");
    if (g_pn_argv) {
      char** p;
      int i = 0;
      for (p = g_pn_argv; *p; ++p, ++i) {
        PN_PRINT("  [%d] %s\n", i, *p);
      }
    }
    PN_PRINT("*** ENVIRONMENT:\n");
    pn_environ_print(g_pn_environ);
  }
#endif /* PN_TRACING */
}

#define PN_MAX_NUM(name)                                \
  static uint32_t pn_max_num_##name(PNModule* module) { \
    uint32_t result = 0;                                \
    uint32_t n;                                         \
    for (n = 0; n < module->num_functions; ++n) {       \
      if (module->functions[n].num_##name > result) {   \
        result = module->functions[n].num_##name;       \
      }                                                 \
    }                                                   \
    return result;                                      \
  }

#define PN_TOTAL_NUM(name)                                \
  static uint32_t pn_total_num_##name(PNModule* module) { \
    uint32_t result = 0;                                  \
    uint32_t n;                                           \
    for (n = 0; n < module->num_functions; ++n) {         \
      result += module->functions[n].num_##name;          \
    }                                                     \
    return result;                                        \
  }

PN_MAX_NUM(constants)
PN_MAX_NUM(values)
PN_MAX_NUM(bbs)
PN_MAX_NUM(instructions)

PN_TOTAL_NUM(constants)
PN_TOTAL_NUM(values)
PN_TOTAL_NUM(bbs)
PN_TOTAL_NUM(instructions)

#undef PN_MAX_NUM
#undef PN_TOTAL_NUM

static const char* pn_human_readable_size_leaky(size_t size) {
  const size_t gig = 1024 * 1024 * 1024;
  const size_t meg = 1024 * 1024;
  const size_t kilo = 1024;
  char buffer[100];
  if (size >= gig) {
    snprintf(buffer, 100, "%.1fG", (size + gig - 1) / (double)gig);
  } else if (size >= meg) {
    snprintf(buffer, 100, "%.1fM", (size + meg - 1) / (double)meg);
  } else if (size >= kilo) {
    snprintf(buffer, 100, "%.1fK", (size + kilo - 1) / (double)kilo);
  } else {
    snprintf(buffer, 100, "%" PRIdPTR, size);
  }
  return pn_strdup(buffer);
}

static void pn_allocator_print_stats_leaky(PNAllocator* allocator) {
  PN_PRINT("%12s allocator: used: %7s frag: %7s\n", allocator->name,
           pn_human_readable_size_leaky(allocator->total_used),
           pn_human_readable_size_leaky(allocator->internal_fragmentation));
}

typedef struct PNOpcodeCountPair {
  PNOpcode opcode;
  uint32_t count;
} PNOpcodeCountPair;

static int pn_opcode_count_pair_compare(const void* a, const void* b) {
  return ((PNOpcodeCountPair*)b)->count - ((PNOpcodeCountPair*)a)->count;
}

typedef struct PNFileData {
  void* data;
  size_t size;
} PNFileData;

static PNFileData pn_read_file(const char* filename) {
  PNFileData result = {};

  PN_BEGIN_TIME(FILE_READ);
  FILE* f = fopen(g_pn_filename, "r");
  if (!f) {
    PN_FATAL("unable to read %s\n", g_pn_filename);
  }

  fseek(f, 0, SEEK_END);
  size_t fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  result.data = pn_malloc(fsize);

  result.size = fread(result.data, 1, fsize, f);
  if (result.size != fsize) {
    PN_FATAL("unable to read data from file\n");
  }

  fclose(f);
  PN_END_TIME(FILE_READ);

  return result;
}

#if PN_CALCULATE_PRED_BBS
static void pn_print_basic_block_graph(PNModule* module) {
  if (!g_pn_print_block_graph_function || !g_pn_print_block_graph_function[0]) {
    return;
  }

  PNFunctionId index = PN_INVALID_FUNCTION_ID;

  int i;
  char first = g_pn_print_block_graph_function[0];
  if (first >= '0' && first <= '9') {
    /* Filter based on function id */
    index = atoi(g_pn_print_block_graph_function);
    if (index >= module->num_functions) {
      PN_FATAL("Invalid function id: %d\n", index);
    }
  } else {
    for (i = 0; i < module->num_functions; ++i) {
      PNFunction* function = &module->functions[i];
      if (function->name) {
        if (strcmp(function->name, g_pn_print_block_graph_function) == 0) {
          index = i;
          break;
        }
      }
    }

    if (index == -1) {
      PN_FATAL("Invalid function name: \"%s\"\n",
               g_pn_print_block_graph_function);
    }
  }

  PNFunction* function = &module->functions[index];

  /* Print graph in dot format */
  PN_PRINT("digraph {\n");
  for (i = 0; i < function->num_bbs; ++i) {
    PN_PRINT("  B%d", i);
#if PN_CALCULATE_LOOPS
    PNBasicBlock* bb = &function->bbs[i];
    if (bb->is_loop_header) {
      PN_PRINT(" [style = bold]");
    }
#endif
    PN_PRINT(";\n");
  }
  for (i = 0; i < function->num_bbs; ++i) {
    PNBasicBlock* bb = &function->bbs[i];
    int j;
    for (j = 0; j < bb->num_succ_bbs; ++j) {
      PN_PRINT("  B%d -> B%d", i, bb->succ_bb_ids[j]);
#if PN_CALCULATE_LOOPS
    if (bb->succ_bb_ids[j] == bb->loop_header_id) {
      PN_PRINT(" [style = dotted]");
    }
#endif
      PN_PRINT(";\n");
    }
  }
  PN_PRINT("}\n");
}
#endif /* PN_CALCULATE_PRED_BBS */

static void pn_print_stats(PNModule* module) {
#if PN_TIMERS
  if (g_pn_print_time) {
    PN_PRINT("-----------------\n");
    double time = 0;
    double percent = 0;
#define PN_PRINT_TIMER(name)                                          \
  struct timespec* timer_##name = &g_pn_timer_times[PN_TIMER_##name]; \
  if (!g_pn_print_time_as_zero) {                                     \
    time = pn_timespec_to_double(timer_##name);                       \
    percent = 100 * pn_timespec_to_double(timer_##name) /             \
              pn_timespec_to_double(timer_TOTAL);                     \
  }                                                                   \
  PN_PRINT("timer %-30s: %f sec (%%%.0f)\n", #name, time, percent);
    PN_FOREACH_TIMER(PN_PRINT_TIMER);
#undef PN_PRINT_TIMER
  }
#endif /* PN_TIMERS */

  if (g_pn_print_named_functions) {
    PN_PRINT("-----------------\n");
    uint32_t i;
    for (i = 0; i < module->num_functions; ++i) {
      PNFunction* function = &module->functions[i];
      if (function->name) {
        PN_PRINT("%d. %s\n", i, function->name);
      }
    }
  }

  if (g_pn_print_stats) {
    PN_PRINT("-----------------\n");
    PN_PRINT("num_types: %u\n", module->num_types);
    PN_PRINT("num_functions: %u\n", module->num_functions);
    PN_PRINT("num_global_vars: %u\n", module->num_global_vars);
    PN_PRINT("max num_constants: %u\n", pn_max_num_constants(module));
    PN_PRINT("max num_values: %u\n", pn_max_num_values(module));
    PN_PRINT("max num_bbs: %u\n", pn_max_num_bbs(module));
    PN_PRINT("max num_instructions: %u\n", pn_max_num_instructions(module));
    PN_PRINT("total num_constants: %u\n", pn_total_num_constants(module));
    PN_PRINT("total num_values: %u\n", pn_total_num_values(module));
    PN_PRINT("total num_bbs: %u\n", pn_total_num_bbs(module));
    PN_PRINT("total num_instructions: %u\n", pn_total_num_instructions(module));
    PN_PRINT("global_var size : %s\n",
             pn_human_readable_size_leaky(module->memory->globalvar_end -
                                          module->memory->globalvar_start));
    PN_PRINT("startinfo size : %s\n",
             pn_human_readable_size_leaky(module->memory->startinfo_end -
                                          module->memory->startinfo_start));
    pn_allocator_print_stats_leaky(&module->allocator);
    pn_allocator_print_stats_leaky(&module->value_allocator);
    pn_allocator_print_stats_leaky(&module->instruction_allocator);
  }

  if (g_pn_print_opcode_counts) {
    PNOpcodeCountPair pairs[PN_MAX_OPCODE];
    uint32_t n;
    for (n = 0; n < PN_MAX_OPCODE; ++n) {
      pairs[n].opcode = n;
      pairs[n].count = g_pn_opcode_count[n];
    }
    qsort(&pairs, PN_MAX_OPCODE, sizeof(PNOpcodeCountPair),
          pn_opcode_count_pair_compare);

    PN_PRINT("-----------------\n");
    for (n = 0; n < PN_MAX_OPCODE; ++n) {
      if (pairs[n].count > 0) {
        PN_PRINT("%40s %d\n", g_pn_opcode_names[pairs[n].opcode],
                 pairs[n].count);
      }
    }
  }
}

void pn_read_context_init(PNReadContext* read_context) {
  memset(read_context, 0, sizeof(*read_context));

#if PN_TRACING
  read_context->define_abbrev = pn_trace_define_abbrev;
  read_context->before_blockinfo_block = pn_trace_before_blockinfo_block;
  read_context->blockinfo_setbid = pn_trace_blockinfo_setbid;
  read_context->blockinfo_blockname = pn_trace_blockinfo_blockname;
  read_context->blockinfo_setrecordname = pn_trace_blockinfo_setrecordname;
  read_context->after_blockinfo_block = pn_trace_after_blockinfo_block;
  read_context->before_type_block = pn_trace_before_type_block;
  read_context->type_num_entries = pn_trace_type_num_entries;
  read_context->type_entry = pn_trace_type_entry;
  read_context->after_type_block = pn_trace_after_type_block;
  read_context->before_globalvar_block = pn_trace_before_globalvar_block;
  read_context->globalvar_before_var = pn_trace_globalvar_before_var;
  read_context->globalvar_compound = pn_trace_globalvar_compound;
  read_context->globalvar_zerofill = pn_trace_globalvar_zerofill;
  read_context->globalvar_data = pn_trace_globalvar_data;
  read_context->globalvar_reloc = pn_trace_globalvar_reloc;
  read_context->globalvar_count = pn_trace_globalvar_count;
  read_context->globalvar_after_var = pn_trace_globalvar_after_var;
  read_context->after_globalvar_block = pn_trace_after_globalvar_block;
  read_context->before_value_symtab_block = pn_trace_before_value_symtab_block;
  read_context->value_symtab_entry = pn_trace_value_symtab_entry;
  read_context->value_symtab_intrinsic = pn_trace_value_symtab_intrinsic;
  read_context->after_value_symtab_block = pn_trace_after_value_symtab_block;
  read_context->before_constants_block = pn_trace_before_constants_block;
  read_context->constants_settype = pn_trace_constants_settype;
  read_context->constants_value = pn_trace_constants_value;
  read_context->after_constants_block = pn_trace_after_constants_block;
  read_context->before_function_block = pn_trace_before_function_block;
  read_context->function_numblocks = pn_trace_function_numblocks;
  read_context->after_function_block = pn_trace_after_function_block;
  read_context->before_module_block = pn_trace_before_module_block;
  read_context->module_version = pn_trace_module_version;
  read_context->module_function = pn_trace_module_function;
  read_context->after_module_block = pn_trace_after_module_block;
#endif
}

int main(int argc, char** argv, char** envp) {
  PN_BEGIN_TIME(TOTAL);
  pn_options_parse(argc, argv, envp);

  PNFileData file_data = pn_read_file(g_pn_filename);

  PNReadContext read_context;
  PNMemory memory;
  PNModule module;
  PNBitStream bs;

  pn_read_context_init(&read_context);
  pn_memory_init(&memory, g_pn_memory_size);
  pn_module_init(&module, &memory);
  pn_bitstream_init(&bs, file_data.data, file_data.size);

  uint32_t load_count;
  for (load_count = 0; load_count < g_pn_repeat_load_times; ++load_count) {
    pn_module_read(&read_context, &module, &bs);

    /* Reset the state so everything can be reloaded */
    if (g_pn_repeat_load_times > 1 &&
        load_count != g_pn_repeat_load_times - 1) {
      pn_bitstream_seek_bit(&bs, 0);
      pn_memory_reset(&memory);
      pn_module_reset(&module);
    }
  }

  if (g_pn_run) {
    PN_BEGIN_TIME(EXECUTE);
    PNExecutor executor = {};

    pn_memory_init_startinfo(&memory, g_pn_argv, g_pn_environ);
#if PN_PPAPI
    if (g_pn_ppapi) {
      pn_memory_init_ppapi(&memory);
    }
#endif /* PN_PPAPI */

    pn_executor_init(&executor, &module);
    pn_executor_run(&executor);
    PN_END_TIME(EXECUTE);

    if (g_pn_verbose) {
      PN_PRINT("Exit code: %d\n", executor.exit_code);
    }
  }

  PN_END_TIME(TOTAL);

#if PN_CALCULATE_PRED_BBS
  pn_print_basic_block_graph(&module);
#endif /* PN_CALCULATE_PRED_BBS */
  pn_print_stats(&module);
  return 0;
}
