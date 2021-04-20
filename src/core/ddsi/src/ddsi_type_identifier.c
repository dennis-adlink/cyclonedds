/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <string.h>
#include <stdlib.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_type_xtypes.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_cdrstream.h"

void ddsi_typeid_copy (struct TypeIdentifier *dst, const struct TypeIdentifier *src)
{
  assert (src);
  assert (dst);
  dst->_d = src->_d;
  if (src->_d <= TK_CHAR16)
    return;
  switch (src->_d)
  {
    case TI_STRING8_SMALL:
    case TI_STRING16_SMALL:
      dst->_u.string_sdefn.bound = src->_u.string_sdefn.bound;
      break;
    case TI_STRING8_LARGE:
    case TI_STRING16_LARGE:
      dst->_u.string_ldefn.bound = src->_u.string_ldefn.bound;
      break;
    case TI_PLAIN_SEQUENCE_SMALL:
      dst->_u.seq_sdefn.header = src->_u.seq_sdefn.header;
      dst->_u.seq_sdefn.bound = src->_u.seq_sdefn.bound;
      dst->_u.seq_sdefn.element_identifier = src->_u.seq_sdefn.element_identifier ? ddsrt_memdup (src->_u.seq_sdefn.element_identifier, sizeof (*src->_u.seq_sdefn.element_identifier)) : NULL;
      break;
    case TI_PLAIN_SEQUENCE_LARGE:
      dst->_u.seq_ldefn.header = src->_u.seq_ldefn.header;
      dst->_u.seq_ldefn.bound = src->_u.seq_ldefn.bound;
      dst->_u.seq_ldefn.element_identifier = src->_u.seq_ldefn.element_identifier ? ddsrt_memdup (src->_u.seq_ldefn.element_identifier, sizeof (*src->_u.seq_ldefn.element_identifier)) : NULL;
      break;
    case TI_PLAIN_ARRAY_SMALL:
      dst->_u.array_sdefn.header = src->_u.array_sdefn.header;
      dst->_u.array_sdefn.array_bound_seq.length = src->_u.array_sdefn.array_bound_seq.length;
      if (src->_u.array_sdefn.array_bound_seq.length > 0)
        dst->_u.array_sdefn.array_bound_seq.seq = ddsrt_memdup (src->_u.array_sdefn.array_bound_seq.seq, src->_u.array_sdefn.array_bound_seq.length * sizeof (*src->_u.array_sdefn.array_bound_seq.seq));
      dst->_u.array_sdefn.element_identifier = src->_u.array_sdefn.element_identifier ? ddsrt_memdup (src->_u.array_sdefn.element_identifier, sizeof (*src->_u.array_sdefn.element_identifier)) : NULL;
      break;
    case TI_PLAIN_ARRAY_LARGE:
      dst->_u.array_ldefn.header = src->_u.array_ldefn.header;
      dst->_u.array_ldefn.array_bound_seq.length = src->_u.array_ldefn.array_bound_seq.length;
      if (src->_u.array_ldefn.array_bound_seq.length > 0)
        dst->_u.array_ldefn.array_bound_seq.seq = ddsrt_memdup (src->_u.array_ldefn.array_bound_seq.seq, src->_u.array_ldefn.array_bound_seq.length * sizeof (*src->_u.array_ldefn.array_bound_seq.seq));
      dst->_u.array_ldefn.element_identifier = src->_u.array_ldefn.element_identifier ? ddsrt_memdup (src->_u.array_ldefn.element_identifier, sizeof (*src->_u.array_ldefn.element_identifier)) : NULL;
      break;
    case TI_PLAIN_MAP_SMALL:
      dst->_u.map_sdefn.header = src->_u.map_sdefn.header;
      dst->_u.map_sdefn.bound = src->_u.map_sdefn.bound;
      dst->_u.map_sdefn.element_identifier = src->_u.map_sdefn.element_identifier ? ddsrt_memdup (src->_u.map_sdefn.element_identifier, sizeof (*src->_u.map_sdefn.element_identifier)) : NULL;
      dst->_u.map_sdefn.key_flags = src->_u.map_sdefn.key_flags;
      dst->_u.map_sdefn.key_identifier = src->_u.map_sdefn.key_identifier ? ddsrt_memdup (src->_u.map_sdefn.key_identifier, sizeof (*src->_u.map_sdefn.key_identifier)) : NULL;
      break;
    case TI_PLAIN_MAP_LARGE:
      dst->_u.map_ldefn.header = src->_u.map_ldefn.header;
      dst->_u.map_ldefn.bound = src->_u.map_ldefn.bound;
      dst->_u.map_ldefn.element_identifier = src->_u.map_ldefn.element_identifier ? ddsrt_memdup (src->_u.map_ldefn.element_identifier, sizeof (*src->_u.map_ldefn.element_identifier)) : NULL;
      dst->_u.map_ldefn.key_flags = src->_u.map_ldefn.key_flags;
      dst->_u.map_ldefn.key_identifier = src->_u.map_ldefn.key_identifier ? ddsrt_memdup (src->_u.map_ldefn.key_identifier, sizeof (*src->_u.map_ldefn.key_identifier)) : NULL;
      break;
    case TI_STRONGLY_CONNECTED_COMPONENT:
      dst->_u.sc_component_id.sc_component_id = src->_u.sc_component_id.sc_component_id;
      dst->_u.sc_component_id.scc_length = src->_u.sc_component_id.scc_length;
      dst->_u.sc_component_id.scc_index = src->_u.sc_component_id.scc_index;
      break;
    case EK_COMPLETE:
    case EK_MINIMAL:
      dst->_u.equivalence_hash = src->_u.equivalence_hash;
      break;
    default:
      dst->_d = TK_NONE;
      break;
  }
}

static int plain_collection_header_compare (struct PlainCollectionHeader a, struct PlainCollectionHeader b)
{
  if (a.equiv_kind != b.equiv_kind)
    return a.equiv_kind > b.equiv_kind ? 1 : -1;
  return a.element_flags > b.element_flags ? 1 : -1;
}

static int equivalence_hash_compare (const struct EquivalenceHash *a, const struct EquivalenceHash *b)
{
  return memcmp (a, b, sizeof (*a));
}

static int type_object_hashid_compare (struct TypeObjectHashId a, struct TypeObjectHashId b)
{
  if (a._d != b._d)
    return a._d > b._d ? 1 : -1;
  return equivalence_hash_compare (&a._u.hash, &b._u.hash);
}

static int strong_connected_component_id_compare (struct StronglyConnectedComponentId a, struct StronglyConnectedComponentId b)
{
  if (a.scc_length != b.scc_length)
    return a.scc_length > b.scc_length ? 1 : -1;
  if (a.scc_index != b.scc_index)
    return a.scc_index > b.scc_index ? 1 : -1;
  return type_object_hashid_compare (a.sc_component_id, b.sc_component_id);
}

int ddsi_typeid_compare (const struct TypeIdentifier *a, const struct TypeIdentifier *b)
{
  int r;
  if (a == NULL && b == NULL)
    return 0;
  if (a == NULL || b == NULL)
    return a > b ? 1 : -1;
  if (a->_d != b->_d)
    return a->_d > b->_d ? 1 : -1;
  if (a->_d <= TK_CHAR16)
    return 0;
  switch (a->_d)
  {
    case TI_STRING8_SMALL:
    case TI_STRING16_SMALL:
      return a->_u.string_sdefn.bound > b->_u.string_sdefn.bound ? 1 : -1;
    case TI_STRING8_LARGE:
    case TI_STRING16_LARGE:
      return a->_u.string_ldefn.bound > b->_u.string_ldefn.bound ? 1 : -1;
    case TI_PLAIN_SEQUENCE_SMALL:
      if ((r = plain_collection_header_compare (a->_u.seq_sdefn.header, b->_u.seq_sdefn.header)) != 0)
        return r;
      if ((r = ddsi_typeid_compare (a->_u.seq_sdefn.element_identifier, b->_u.seq_sdefn.element_identifier)) != 0)
        return r;
      return a->_u.seq_sdefn.bound > b->_u.seq_sdefn.bound ? 1 : -1;
    case TI_PLAIN_SEQUENCE_LARGE:
      if ((r = plain_collection_header_compare (a->_u.seq_ldefn.header, b->_u.seq_ldefn.header)) != 0)
        return r;
      if ((r = ddsi_typeid_compare (a->_u.seq_ldefn.element_identifier, b->_u.seq_ldefn.element_identifier)) != 0)
        return r;
      return a->_u.seq_ldefn.bound > b->_u.seq_ldefn.bound ? 1 : -1;
    case TI_PLAIN_ARRAY_SMALL:
      if ((r = plain_collection_header_compare (a->_u.array_sdefn.header, b->_u.array_sdefn.header)) != 0)
        return r;
      if (a->_u.array_sdefn.array_bound_seq.length != b->_u.array_sdefn.array_bound_seq.length)
        return a->_u.array_sdefn.array_bound_seq.length > b->_u.array_sdefn.array_bound_seq.length ? 1 : -1;
      if (a->_u.array_sdefn.array_bound_seq.length > 0)
        if ((r = memcmp (a->_u.array_sdefn.array_bound_seq.seq, b->_u.array_sdefn.array_bound_seq.seq,
                          a->_u.array_sdefn.array_bound_seq.length * sizeof (*a->_u.array_sdefn.array_bound_seq.seq))) != 0)
          return r;
      return ddsi_typeid_compare (a->_u.array_sdefn.element_identifier, b->_u.array_sdefn.element_identifier);
    case TI_PLAIN_ARRAY_LARGE:
      if ((r = plain_collection_header_compare (a->_u.array_ldefn.header, b->_u.array_ldefn.header)) != 0)
        return r;
      if (a->_u.array_ldefn.array_bound_seq.length != b->_u.array_ldefn.array_bound_seq.length)
        return a->_u.array_ldefn.array_bound_seq.length > b->_u.array_ldefn.array_bound_seq.length ? 1 : -1;
      if (a->_u.array_ldefn.array_bound_seq.length > 0)
        if ((r = memcmp (a->_u.array_ldefn.array_bound_seq.seq, b->_u.array_ldefn.array_bound_seq.seq,
                          a->_u.array_ldefn.array_bound_seq.length * sizeof (*a->_u.array_ldefn.array_bound_seq.seq))) != 0)
          return r;
      return ddsi_typeid_compare (a->_u.array_ldefn.element_identifier, b->_u.array_ldefn.element_identifier);
    case TI_PLAIN_MAP_SMALL:
      if ((r = plain_collection_header_compare (a->_u.map_sdefn.header, b->_u.map_sdefn.header)) != 0)
        return r;
      if (a->_u.map_sdefn.bound != b->_u.map_sdefn.bound)
        return a->_u.map_sdefn.bound > b->_u.map_sdefn.bound ? 1 : -1;
      if ((r = ddsi_typeid_compare (a->_u.map_sdefn.element_identifier, b->_u.map_sdefn.element_identifier)) != 0)
        return r;
      if (a->_u.map_sdefn.key_flags != b->_u.map_sdefn.key_flags)
        return a->_u.map_sdefn.key_flags != b->_u.map_sdefn.key_flags ? 1 : -1;
      return ddsi_typeid_compare (a->_u.map_sdefn.key_identifier, b->_u.map_sdefn.key_identifier);
    case TI_PLAIN_MAP_LARGE:
      if ((r = plain_collection_header_compare (a->_u.map_ldefn.header, b->_u.map_ldefn.header)) != 0)
        return r;
      if (a->_u.map_ldefn.bound != b->_u.map_ldefn.bound)
        return a->_u.map_ldefn.bound > b->_u.map_ldefn.bound ? 1 : -1;
      if ((r = ddsi_typeid_compare (a->_u.map_ldefn.element_identifier, b->_u.map_ldefn.element_identifier)) != 0)
        return r;
      if (a->_u.map_ldefn.key_flags != b->_u.map_ldefn.key_flags)
        return a->_u.map_ldefn.key_flags > b->_u.map_ldefn.key_flags ? 1 : -1;
      return ddsi_typeid_compare (a->_u.map_ldefn.key_identifier, b->_u.map_ldefn.key_identifier);
    case TI_STRONGLY_CONNECTED_COMPONENT:
      return strong_connected_component_id_compare (a->_u.sc_component_id, b->_u.sc_component_id);
    case EK_COMPLETE:
    case EK_MINIMAL:
      return equivalence_hash_compare (&a->_u.equivalence_hash, &b->_u.equivalence_hash);
    default:
      assert (false);
      return 1;
  }
}

void ddsi_typeid_ser (const struct TypeIdentifier *typeid, unsigned char **buf, uint32_t *sz)
{
  dds_ostream_t os = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = CDR_ENC_VERSION_2 };
  dds_stream_writeLE ((dds_ostreamLE_t *) &os, (const void *) typeid, TypeIdentifier_ops);
  *buf = os.m_buffer;
  *sz = os.m_index;
}

void ddsi_typeid_deser (unsigned char *buf, uint32_t sz, struct TypeIdentifier **typeid)
{
  unsigned char *data;
  uint32_t srcoff = 0;
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  if (bswap)
  {
    data = ddsrt_memdup (buf, sz);
    dds_stream_normalize1 ((char *) data, &srcoff, sz, bswap, CDR_ENC_VERSION_2, TypeIdentifier_ops);
  }
  else
    data = buf;

  dds_istream_t is = { .m_buffer = data, .m_index = 0, .m_size = sz, .m_xcdr_version = CDR_ENC_VERSION_2 };
  dds_stream_read (&is, (void *) *typeid, TypeIdentifier_ops);
}

bool ddsi_typeid_is_none (const struct TypeIdentifier *typeid)
{
  return typeid == NULL || typeid->_d == TK_NONE;
}

bool ddsi_typeid_is_hash (const struct TypeIdentifier *typeid)
{
  return ddsi_typeid_is_minimal (typeid) || ddsi_typeid_is_complete (typeid);
}

bool ddsi_typeid_is_minimal (const struct TypeIdentifier *typeid)
{
  return typeid != NULL && typeid->_d == EK_MINIMAL;
}

bool ddsi_typeid_is_complete (const struct TypeIdentifier *typeid)
{
  return typeid != NULL && typeid->_d == EK_COMPLETE;
}
