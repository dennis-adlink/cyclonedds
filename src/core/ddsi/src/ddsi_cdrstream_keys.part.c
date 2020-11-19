/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

static void dds_stream_write_key_implBO (DDS_OSTREAM_T * __restrict os, const uint32_t *insnp, const void *src, uint16_t key_offset_count, const uint32_t * key_offset_insn);
static void dds_stream_write_key_implBO (DDS_OSTREAM_T * __restrict os, const uint32_t *insnp, const void *src, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  assert (DDS_OP (*insnp) == DDS_OP_ADR);
  assert (insn_key_ok_p (*insnp));
  const void *addr = src + insnp[1];
  switch (DDS_OP_TYPE (*insnp))
  {
    case DDS_OP_VAL_1BY: dds_os_put1BO (os, *((uint8_t *) addr)); break;
    case DDS_OP_VAL_2BY: dds_os_put2BO (os, *((uint16_t *) addr)); break;
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: dds_os_put4BO (os, *((uint32_t *) addr)); break;
    case DDS_OP_VAL_8BY: dds_os_put8BO (os, *((uint64_t *) addr)); break;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BSP: dds_stream_write_stringBO (os, *(char **) addr); break;
    case DDS_OP_VAL_BST: dds_stream_write_stringBO (os, addr); break;
    case DDS_OP_VAL_ARR: {
      const uint32_t elem_size = get_type_size (DDS_OP_SUBTYPE (*insnp));
      const uint32_t num = insnp[2];
      dds_cdr_alignto_clear_and_resizeBO (os, elem_size, num * elem_size);
      void * const dst = ((struct dds_ostream *)os)->m_buffer + ((struct dds_ostream *)os)->m_index;
      dds_os_put_bytes ((struct dds_ostream *)os, addr, num * elem_size);
      dds_stream_swap_insituBO (dst, elem_size, num);
      break;
    }
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = insnp + DDS_OP_ADR_JSR (insnp[2]) + *key_offset_insn;
      dds_stream_write_key_implBO (os, jsr_ops, addr, --key_offset_count, ++key_offset_insn);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      abort ();
      break;
    }
  }
}

void dds_stream_write_keyBO (DDS_OSTREAM_T * __restrict os, const char * __restrict sample, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  for (uint32_t i = 0; i < desc->keys.nkeys; i++)
  {
    const uint32_t *insnp = desc->ops.ops + desc->keys.keys[i];
    switch (DDS_OP (*insnp))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*insnp);
        dds_stream_write_key_implBO (os, desc->ops.ops + insnp[1], sample, --n_offs, insnp + 2);
        break;
      }
      case DDS_OP_ADR: {
        dds_stream_write_key_implBO (os, insnp, sample, 0, NULL);
        break;
      }
      default:
        abort ();
        break;
    }
  }
}

static void dds_stream_extract_key_from_data1BO (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const uint32_t * __restrict ops, uint32_t * __restrict keys_remaining)
{
  uint32_t op;
  while ((op = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (op))
    {
      case DDS_OP_ADR: {
        const uint32_t type = DDS_OP_TYPE (op);
        const bool is_key = (op & DDS_OP_FLAG_KEY) && (os != NULL);
        if (type == DDS_OP_VAL_EXT)
        {
          const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
          const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
          dds_stream_extract_key_from_data1BO (is, os, jsr_ops, keys_remaining);
          ops += jmp ? jmp : 3;
        }
        else if (is_key)
        {
          dds_stream_extract_key_from_key_prim_opBO (is, os, ops);
          if (--(*keys_remaining) == 0)
            return;
          ops += 2 + (type == DDS_OP_VAL_BST || type == DDS_OP_VAL_BSP || type == DDS_OP_VAL_ARR || type == DDS_OP_VAL_ENU);
        }
        else
        {
          switch (type)
          {
            case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_ENU:
              dds_stream_extract_key_from_data_skip_subtype (is, 1, type, NULL);
              ops += 2 + (type == DDS_OP_VAL_BST || type == DDS_OP_VAL_BSP || type == DDS_OP_VAL_ARR || type == DDS_OP_VAL_ENU);
              break;
            case DDS_OP_VAL_SEQ:
              ops = dds_stream_extract_key_from_data_skip_sequence (is, ops);
              break;
            case DDS_OP_VAL_ARR:
              ops = dds_stream_extract_key_from_data_skip_array (is, ops);
              break;
            case DDS_OP_VAL_UNI:
              ops = dds_stream_extract_key_from_data_skip_union (is, ops);
              break;
            case DDS_OP_VAL_STU:
              abort (); /* op type STU only supported as subtype */
              break;
          }
        }
        break;
      }
      case DDS_OP_JSR: { /* Implies nested type */
        ops += 2;
        dds_stream_extract_key_from_data1BO (is, os, ops + DDS_OP_JUMP (op), keys_remaining);
        if (--(*keys_remaining) == 0)
          return;
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_KOF: {
        abort ();
        break;
      }
      case DDS_OP_DLC: case DDS_OP_PLC: {
        abort (); /* FIXME */
        break;
      }
    }
  }
}

void dds_stream_extract_key_from_dataBO (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  uint32_t keys_remaining = desc->keys.nkeys;
  dds_stream_extract_key_from_data1BO (is, os, desc->ops.ops, &keys_remaining);
}
