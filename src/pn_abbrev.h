/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_ABBREV_H_
#define PN_ABBREV_H_

static PNAbbrev* pn_abbrevs_allocate(PNAllocator* allocator,
                                     PNAbbrevs* abbrevs,
                                     uint32_t num_ops) {
  /* This is more complicated than it would otherwise be, because our allocator
   * doesn't allow realloc'ing multiple allocations at the same time.
   *
   * When we add a PNAbbrev, we want to resize the PNAbbrevs.abbrevs member and
   * allocate num_ops PNAbbrevOps as well. When we add another block, we need
   * to resize PNAbbrevs.abbrevs again, but the list is no longer the last
   * allocation (the PNAbbrevOps are), so the reallocation fails.
   *
   * To work around this, we allocate all the PNAbbrev objects and PNAbbrevOps
   * objects (for each PNAbbrev object) as one allocation. When we want to add
   * a new PNAbbrev, we need to move all the PNAbbrevOps (which are all
   * allocated as a contiguous block) down, to make space for the new PNAbbrev.
   * We also need to allocate space for the new PNAbbrevOps that are referenced
   * by the new PNAbbrev.
   */
  size_t add_size = sizeof(PNAbbrev) + num_ops * sizeof(PNAbbrevOp);
  size_t last_size =
      abbrevs->abbrevs ? pn_allocator_last_alloc_size(allocator) : 0;
  pn_allocator_realloc_add(allocator, (void**)&abbrevs->abbrevs, add_size,
                           PN_DEFAULT_ALIGN);
  size_t all_ops_size = last_size - sizeof(PNAbbrev) * abbrevs->num_abbrevs;
  void* prev_ops = &abbrevs->abbrevs[abbrevs->num_abbrevs];
  void* ops = prev_ops + sizeof(PNAbbrev);
  memmove(ops, prev_ops, all_ops_size);

  /* Since all the PNAbbrevOps have moved down, we need to fix all of the
   * PNAbbrev.ops pointers by pushing them down by the same amount. */
  PNAbbrev* abbrev = &abbrevs->abbrevs[0];
  uint32_t i;
  for (i = 0; i < abbrevs->num_abbrevs; ++i) {
    abbrev->ops = ops;
    ops += abbrev->num_ops * sizeof(PNAbbrevOp);
    ++abbrev;
  }

  abbrev->num_ops = num_ops;
  abbrev->ops = ops;
  abbrevs->num_abbrevs++;
  return abbrev;
}

static void pn_abbrev_copy(PNAllocator* allocator,
                           PNAbbrevs* dest_abbrevs,
                           PNAbbrev* src_abbrev) {
  PNAbbrev* dest_abbrev =
      pn_abbrevs_allocate(allocator, dest_abbrevs, src_abbrev->num_ops);
  uint32_t i;
  for (i = 0; i < src_abbrev->num_ops; ++i) {
    dest_abbrev->ops[i] = src_abbrev->ops[i];
  }
}

static void pn_block_info_context_copy_abbrevs_for_block_id(
    PNAllocator* allocator,
    PNBlockInfoContext* context,
    PNBlockId block_id,
    PNAbbrevs* dest_abbrevs) {
  PN_CHECK(block_id < PN_MAX_BLOCK_IDS);
  PNAbbrevs* src_abbrevs = &context->block_abbrev_map[block_id];
  PNAbbrev* src_abbrev = &src_abbrevs->abbrevs[0];
  uint32_t i;
  for (i = 0; i < src_abbrevs->num_abbrevs; ++i) {
    /* TODO(binji): Could optimize this to do all allocation in one step */
    pn_abbrev_copy(allocator, dest_abbrevs, src_abbrev++);
  }
}

static uint32_t pn_block_info_context_append_abbrev(PNAllocator* allocator,
                                                    PNBlockInfoContext* context,
                                                    PNBlockId block_id,
                                                    PNAbbrev* abbrev) {
  PN_CHECK(block_id < PN_MAX_BLOCK_IDS);
  PNAbbrevs* abbrevs = &context->block_abbrev_map[block_id];
  uint32_t abbrev_id = abbrevs->num_abbrevs;
  pn_abbrev_copy(allocator, abbrevs, abbrev);
  return abbrev_id;
}

static PNAbbrev* pn_abbrev_read(PNAllocator* allocator,
                                PNBitStream* bs,
                                PNAbbrevs* abbrevs) {
  uint32_t num_ops = pn_bitstream_read_vbr(bs, 5);
  PNAbbrev* abbrev = pn_abbrevs_allocate(allocator, abbrevs, num_ops);
  abbrev->num_ops = 0;

  while (abbrev->num_ops < num_ops) {
    PNAbbrevOp* op = &abbrev->ops[abbrev->num_ops++];

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
          PNAbbrevOp* elt_op = &abbrev->ops[abbrev->num_ops++];

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
                PN_FATAL("bad encoding for array element: %d\n",
                         elt_op->encoding);
            }
          }
          break;
        }

        case PN_ENCODING_CHAR6:
        case PN_ENCODING_BLOB:
          /* Nothing */
          break;

        default:
          PN_FATAL("bad encoding: %d\n", op->encoding);
      }
    }
  }

  return abbrev;
}

#if PN_TRACING
static void pn_abbrev_trace(PNAbbrev* abbrev,
                            uint32_t abbrev_id,
                            PNBool global) {
  if (!PN_IS_TRACE(ABBREV)) {
    return;
  }
  PN_TRACE_PRINT_INDENT();
  PN_PRINT("%ca%d = abbrev <", global ? '@' : '%', abbrev_id);
  uint32_t i;
  for (i = 0; i < abbrev->num_ops; ++i) {
    PNAbbrevOp* op = &abbrev->ops[i];
    switch (op->encoding) {
      case PN_ENCODING_LITERAL: PN_PRINT("%d", op->value); break;
      case PN_ENCODING_FIXED: PN_PRINT("fixed(%d)", op->num_bits); break;
      case PN_ENCODING_VBR: PN_PRINT("vbr(%d)", op->num_bits); break;
      case PN_ENCODING_ARRAY: {
        PNAbbrevOp* elt = &abbrev->ops[i + 1];
        PN_PRINT("array(");
        switch (elt->encoding) {
          case PN_ENCODING_FIXED: PN_PRINT("fixed(%d)", elt->num_bits); break;
          case PN_ENCODING_VBR: PN_PRINT("vbr(%d)", elt->num_bits); break;
          case PN_ENCODING_CHAR6: PN_PRINT("char6"); break;
          default: PN_UNREACHABLE(); break;
        }
        PN_PRINT(")");
        ++i;
        break;
      }

      case PN_ENCODING_CHAR6: PN_PRINT("char6"); break;
      case PN_ENCODING_BLOB: PN_PRINT("blob"); break;
      default: PN_UNREACHABLE(); break;
    }
    if (i != abbrev->num_ops - 1) {
      PN_PRINT(", ");
    }
  }
  PN_PRINT(">;\n");
}
#else
static void pn_abbrev_trace(PNAbbrev* abbrev,
                            uint32_t abbrev_id,
                            PNBool global) {}
#endif /* PN_TRACING */

#endif /* PN_ABBREV_H_ */
