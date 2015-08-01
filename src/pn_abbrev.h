/* Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PN_ABBREV_H_
#define PN_ABBREV_H_

static void pn_block_info_context_get_abbrev(PNBlockInfoContext* context,
                                             PNBlockId block_id,
                                             PNBlockAbbrevs* abbrevs) {
  PN_CHECK(block_id < PN_MAX_BLOCK_IDS);
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

static uint32_t pn_block_info_context_append_abbrev(PNBlockInfoContext* context,
                                                    PNBlockId block_id,
                                                    PNBlockAbbrev* abbrev) {
  PN_CHECK(block_id < PN_MAX_BLOCK_IDS);
  PNBlockAbbrevs* abbrevs = &context->block_abbrev_map[block_id];
  assert(abbrevs->num_abbrevs < PN_MAX_BLOCK_ABBREV);
  uint32_t abbrev_id = abbrevs->num_abbrevs++;
  PNBlockAbbrev* dest_abbrev = &abbrevs->abbrevs[abbrev_id];
  *dest_abbrev = *abbrev;
  return abbrev_id;
}

static PNBlockAbbrev* pn_block_abbrev_read(PNBitStream* bs,
                                           PNBlockAbbrevs* abbrevs) {
  assert(abbrevs->num_abbrevs < PN_MAX_BLOCK_ABBREV);
  PNBlockAbbrev* abbrev = &abbrevs->abbrevs[abbrevs->num_abbrevs++];

  uint32_t num_ops = pn_bitstream_read_vbr(bs, 5);
  PN_CHECK(num_ops < PN_MAX_BLOCK_ABBREV_OP);
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
static void pn_abbrev_trace(PNBlockAbbrev* abbrev,
                            uint32_t abbrev_id,
                            PNBool global) {
  if (!PN_IS_TRACE(ABBREV)) {
    return;
  }
  PN_TRACE_PRINT_INDENT();
  PN_PRINT("%ca%d = abbrev <", global ? '@' : '%', abbrev_id);
  uint32_t i;
  for (i = 0; i < abbrev->num_ops; ++i) {
    PNBlockAbbrevOp* op = &abbrev->ops[i];
    switch (op->encoding) {
      case PN_ENCODING_LITERAL: PN_PRINT("%d", op->value); break;
      case PN_ENCODING_FIXED: PN_PRINT("fixed(%d)", op->num_bits); break;
      case PN_ENCODING_VBR: PN_PRINT("vbr(%d)", op->num_bits); break;
      case PN_ENCODING_ARRAY: {
        PNBlockAbbrevOp* elt = &abbrev->ops[i + 1];
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
static void pn_abbrev_trace(PNBlockAbbrev* abbrev,
                            uint32_t abbrev_id,
                            PNBool global) {}
#endif /* PN_TRACING */

#endif /* PN_ABBREV_H_ */
