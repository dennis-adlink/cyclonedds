/*
 * Copyright(c) 2006 to 2021 ADLINK Technology Limited and others
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
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_type_lookup.h"
#include "dds/ddsi/ddsi_xt.h"
#include "dds/ddsi/ddsi_xt_wrap.h"
#include "dds/ddsc/dds_public_impl.h"


void ddsi_typeid_copy (ddsi_typeid_t *dst, const ddsi_typeid_t *src)
{
  assert (src);
  assert (dst);
  dst->_d = src->_d;
  if (src->_d <= DDS_XTypes_TK_CHAR16)
    return;
  switch (src->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING16_SMALL:
      dst->_u.string_sdefn.bound = src->_u.string_sdefn.bound;
      break;
    case DDS_XTypes_TI_STRING8_LARGE:
    case DDS_XTypes_TI_STRING16_LARGE:
      dst->_u.string_ldefn.bound = src->_u.string_ldefn.bound;
      break;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      dst->_u.seq_sdefn.header = src->_u.seq_sdefn.header;
      dst->_u.seq_sdefn.bound = src->_u.seq_sdefn.bound;
      dst->_u.seq_sdefn.element_identifier = src->_u.seq_sdefn.element_identifier ? ddsrt_memdup (src->_u.seq_sdefn.element_identifier, sizeof (*src->_u.seq_sdefn.element_identifier)) : NULL;
      break;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      dst->_u.seq_ldefn.header = src->_u.seq_ldefn.header;
      dst->_u.seq_ldefn.bound = src->_u.seq_ldefn.bound;
      dst->_u.seq_ldefn.element_identifier = src->_u.seq_ldefn.element_identifier ? ddsrt_memdup (src->_u.seq_ldefn.element_identifier, sizeof (*src->_u.seq_ldefn.element_identifier)) : NULL;
      break;
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      dst->_u.array_sdefn.header = src->_u.array_sdefn.header;
      dst->_u.array_sdefn.array_bound_seq._length = src->_u.array_sdefn.array_bound_seq._length;
      if (src->_u.array_sdefn.array_bound_seq._length > 0)
        dst->_u.array_sdefn.array_bound_seq._buffer = ddsrt_memdup (src->_u.array_sdefn.array_bound_seq._buffer, src->_u.array_sdefn.array_bound_seq._length * sizeof (*src->_u.array_sdefn.array_bound_seq._buffer));
      dst->_u.array_sdefn.element_identifier = src->_u.array_sdefn.element_identifier ? ddsrt_memdup (src->_u.array_sdefn.element_identifier, sizeof (*src->_u.array_sdefn.element_identifier)) : NULL;
      break;
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      dst->_u.array_ldefn.header = src->_u.array_ldefn.header;
      dst->_u.array_ldefn.array_bound_seq._length = src->_u.array_ldefn.array_bound_seq._length;
      if (src->_u.array_ldefn.array_bound_seq._length > 0)
        dst->_u.array_ldefn.array_bound_seq._buffer = ddsrt_memdup (src->_u.array_ldefn.array_bound_seq._buffer, src->_u.array_ldefn.array_bound_seq._length * sizeof (*src->_u.array_ldefn.array_bound_seq._buffer));
      dst->_u.array_ldefn.element_identifier = src->_u.array_ldefn.element_identifier ? ddsrt_memdup (src->_u.array_ldefn.element_identifier, sizeof (*src->_u.array_ldefn.element_identifier)) : NULL;
      break;
    case DDS_XTypes_TI_PLAIN_MAP_SMALL:
      dst->_u.map_sdefn.header = src->_u.map_sdefn.header;
      dst->_u.map_sdefn.bound = src->_u.map_sdefn.bound;
      dst->_u.map_sdefn.element_identifier = src->_u.map_sdefn.element_identifier ? ddsrt_memdup (src->_u.map_sdefn.element_identifier, sizeof (*src->_u.map_sdefn.element_identifier)) : NULL;
      dst->_u.map_sdefn.key_flags = src->_u.map_sdefn.key_flags;
      dst->_u.map_sdefn.key_identifier = src->_u.map_sdefn.key_identifier ? ddsrt_memdup (src->_u.map_sdefn.key_identifier, sizeof (*src->_u.map_sdefn.key_identifier)) : NULL;
      break;
    case DDS_XTypes_TI_PLAIN_MAP_LARGE:
      dst->_u.map_ldefn.header = src->_u.map_ldefn.header;
      dst->_u.map_ldefn.bound = src->_u.map_ldefn.bound;
      dst->_u.map_ldefn.element_identifier = src->_u.map_ldefn.element_identifier ? ddsrt_memdup (src->_u.map_ldefn.element_identifier, sizeof (*src->_u.map_ldefn.element_identifier)) : NULL;
      dst->_u.map_ldefn.key_flags = src->_u.map_ldefn.key_flags;
      dst->_u.map_ldefn.key_identifier = src->_u.map_ldefn.key_identifier ? ddsrt_memdup (src->_u.map_ldefn.key_identifier, sizeof (*src->_u.map_ldefn.key_identifier)) : NULL;
      break;
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
      dst->_u.sc_component_id.sc_component_id = src->_u.sc_component_id.sc_component_id;
      dst->_u.sc_component_id.scc_length = src->_u.sc_component_id.scc_length;
      dst->_u.sc_component_id.scc_index = src->_u.sc_component_id.scc_index;
      break;
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_EK_MINIMAL:
      memcpy (dst->_u.equivalence_hash, src->_u.equivalence_hash, sizeof (dst->_u.equivalence_hash));
      break;
    default:
      dst->_d = DDS_XTypes_TK_NONE;
      break;
  }
}

static int plain_collection_header_compare (struct DDS_XTypes_PlainCollectionHeader a, struct DDS_XTypes_PlainCollectionHeader b)
{
  if (a.equiv_kind != b.equiv_kind)
    return a.equiv_kind > b.equiv_kind ? 1 : -1;
  return a.element_flags > b.element_flags ? 1 : -1;
}

static int equivalence_hash_compare (const DDS_XTypes_EquivalenceHash *a, const DDS_XTypes_EquivalenceHash *b)
{
  return memcmp (a, b, sizeof (*a));
}

static int type_object_hashid_compare (struct DDS_XTypes_TypeObjectHashId a, struct DDS_XTypes_TypeObjectHashId b)
{
  if (a._d != b._d)
    return a._d > b._d ? 1 : -1;
  return equivalence_hash_compare (&a._u.hash, &b._u.hash);
}

static int strong_connected_component_id_compare (struct DDS_XTypes_StronglyConnectedComponentId a, struct DDS_XTypes_StronglyConnectedComponentId b)
{
  if (a.scc_length != b.scc_length)
    return a.scc_length > b.scc_length ? 1 : -1;
  if (a.scc_index != b.scc_index)
    return a.scc_index > b.scc_index ? 1 : -1;
  return type_object_hashid_compare (a.sc_component_id, b.sc_component_id);
}

int ddsi_typeid_compare (const ddsi_typeid_t *a, const ddsi_typeid_t *b)
{
  int r;
  if (a == NULL && b == NULL)
    return 0;
  if (a == NULL || b == NULL)
    return a > b ? 1 : -1;
  if (a->_d != b->_d)
    return a->_d > b->_d ? 1 : -1;
  if (a->_d <= DDS_XTypes_TK_CHAR16)
    return 0;
  switch (a->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING16_SMALL:
      return a->_u.string_sdefn.bound > b->_u.string_sdefn.bound ? 1 : -1;
    case DDS_XTypes_TI_STRING8_LARGE:
    case DDS_XTypes_TI_STRING16_LARGE:
      return a->_u.string_ldefn.bound > b->_u.string_ldefn.bound ? 1 : -1;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      if ((r = plain_collection_header_compare (a->_u.seq_sdefn.header, b->_u.seq_sdefn.header)) != 0)
        return r;
      if ((r = ddsi_typeid_compare (a->_u.seq_sdefn.element_identifier, b->_u.seq_sdefn.element_identifier)) != 0)
        return r;
      return a->_u.seq_sdefn.bound > b->_u.seq_sdefn.bound ? 1 : -1;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      if ((r = plain_collection_header_compare (a->_u.seq_ldefn.header, b->_u.seq_ldefn.header)) != 0)
        return r;
      if ((r = ddsi_typeid_compare (a->_u.seq_ldefn.element_identifier, b->_u.seq_ldefn.element_identifier)) != 0)
        return r;
      return a->_u.seq_ldefn.bound > b->_u.seq_ldefn.bound ? 1 : -1;
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      if ((r = plain_collection_header_compare (a->_u.array_sdefn.header, b->_u.array_sdefn.header)) != 0)
        return r;
      if (a->_u.array_sdefn.array_bound_seq._length != b->_u.array_sdefn.array_bound_seq._length)
        return a->_u.array_sdefn.array_bound_seq._length > b->_u.array_sdefn.array_bound_seq._length ? 1 : -1;
      if (a->_u.array_sdefn.array_bound_seq._length > 0)
        if ((r = memcmp (a->_u.array_sdefn.array_bound_seq._buffer, b->_u.array_sdefn.array_bound_seq._buffer,
                          a->_u.array_sdefn.array_bound_seq._length * sizeof (*a->_u.array_sdefn.array_bound_seq._buffer))) != 0)
          return r;
      return ddsi_typeid_compare (a->_u.array_sdefn.element_identifier, b->_u.array_sdefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      if ((r = plain_collection_header_compare (a->_u.array_ldefn.header, b->_u.array_ldefn.header)) != 0)
        return r;
      if (a->_u.array_ldefn.array_bound_seq._length != b->_u.array_ldefn.array_bound_seq._length)
        return a->_u.array_ldefn.array_bound_seq._length > b->_u.array_ldefn.array_bound_seq._length ? 1 : -1;
      if (a->_u.array_ldefn.array_bound_seq._length > 0)
        if ((r = memcmp (a->_u.array_ldefn.array_bound_seq._buffer, b->_u.array_ldefn.array_bound_seq._buffer,
                          a->_u.array_ldefn.array_bound_seq._length * sizeof (*a->_u.array_ldefn.array_bound_seq._buffer))) != 0)
          return r;
      return ddsi_typeid_compare (a->_u.array_ldefn.element_identifier, b->_u.array_ldefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_MAP_SMALL:
      if ((r = plain_collection_header_compare (a->_u.map_sdefn.header, b->_u.map_sdefn.header)) != 0)
        return r;
      if (a->_u.map_sdefn.bound != b->_u.map_sdefn.bound)
        return a->_u.map_sdefn.bound > b->_u.map_sdefn.bound ? 1 : -1;
      if ((r = ddsi_typeid_compare (a->_u.map_sdefn.element_identifier, b->_u.map_sdefn.element_identifier)) != 0)
        return r;
      if (a->_u.map_sdefn.key_flags != b->_u.map_sdefn.key_flags)
        return a->_u.map_sdefn.key_flags != b->_u.map_sdefn.key_flags ? 1 : -1;
      return ddsi_typeid_compare (a->_u.map_sdefn.key_identifier, b->_u.map_sdefn.key_identifier);
    case DDS_XTypes_TI_PLAIN_MAP_LARGE:
      if ((r = plain_collection_header_compare (a->_u.map_ldefn.header, b->_u.map_ldefn.header)) != 0)
        return r;
      if (a->_u.map_ldefn.bound != b->_u.map_ldefn.bound)
        return a->_u.map_ldefn.bound > b->_u.map_ldefn.bound ? 1 : -1;
      if ((r = ddsi_typeid_compare (a->_u.map_ldefn.element_identifier, b->_u.map_ldefn.element_identifier)) != 0)
        return r;
      if (a->_u.map_ldefn.key_flags != b->_u.map_ldefn.key_flags)
        return a->_u.map_ldefn.key_flags > b->_u.map_ldefn.key_flags ? 1 : -1;
      return ddsi_typeid_compare (a->_u.map_ldefn.key_identifier, b->_u.map_ldefn.key_identifier);
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
      return strong_connected_component_id_compare (a->_u.sc_component_id, b->_u.sc_component_id);
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_EK_MINIMAL:
      return equivalence_hash_compare (&a->_u.equivalence_hash, &b->_u.equivalence_hash);
    default:
      assert (false);
      return 1;
  }
}

void ddsi_typeid_ser (const ddsi_typeid_t *typeid, unsigned char **buf, uint32_t *sz)
{
  dds_ostream_t os = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = CDR_ENC_VERSION_2 };
  dds_stream_writeLE ((dds_ostreamLE_t *) &os, (const void *) typeid, DDS_XTypes_TypeIdentifier_desc.m_ops);
  *buf = os.m_buffer;
  *sz = os.m_index;
}

void ddsi_typeid_deser (unsigned char *buf, uint32_t sz, ddsi_typeid_t **typeid)
{
  unsigned char *data;
  uint32_t srcoff = 0;
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  if (bswap)
  {
    data = ddsrt_memdup (buf, sz);
    dds_stream_normalize1 ((char *) data, &srcoff, sz, bswap, CDR_ENC_VERSION_2, DDS_XTypes_TypeIdentifier_desc.m_ops);
  }
  else
    data = buf;

  dds_istream_t is = { .m_buffer = data, .m_index = 0, .m_size = sz, .m_xcdr_version = CDR_ENC_VERSION_2 };
  dds_stream_read (&is, (void *) *typeid, DDS_XTypes_TypeIdentifier_desc.m_ops);
}

bool ddsi_typeid_is_none (const ddsi_typeid_t *typeid)
{
  return typeid == NULL || typeid->_d == DDS_XTypes_TK_NONE;
}

bool ddsi_typeid_is_hash (const ddsi_typeid_t *typeid)
{
  return ddsi_typeid_is_minimal (typeid) || ddsi_typeid_is_complete (typeid);
}

bool ddsi_typeid_is_minimal (const ddsi_typeid_t *typeid)
{
  return typeid != NULL && typeid->_d == DDS_XTypes_EK_MINIMAL;
}

bool ddsi_typeid_is_complete (const ddsi_typeid_t *typeid)
{
  return typeid != NULL && typeid->_d == DDS_XTypes_EK_COMPLETE;
}

static bool type_id_with_size_equal (const struct DDS_XTypes_TypeIdentfierWithSize *a, const struct DDS_XTypes_TypeIdentfierWithSize *b)
{
  return !ddsi_typeid_compare (&a->type_id, &b->type_id) && a->typeobject_serialized_size == b->typeobject_serialized_size;
}

static bool type_id_with_sizeseq_equal (const struct dds_sequence_DDS_XTypes_TypeIdentfierWithSize *a, const struct dds_sequence_DDS_XTypes_TypeIdentfierWithSize *b)
{
    if (a->_length != b->_length)
      return false;
    for (uint32_t n = 0; n < a->_length; n++)
      if (!type_id_with_size_equal (&a->_buffer[n], &b->_buffer[n]))
        return false;
    return true;
}

static bool type_id_with_deps_equal (const struct DDS_XTypes_TypeIdentifierWithDependencies *a, const struct DDS_XTypes_TypeIdentifierWithDependencies *b)
{
  return type_id_with_size_equal (&a->typeid_with_size, &b->typeid_with_size)
    && a->dependent_typeid_count == b->dependent_typeid_count
    && type_id_with_sizeseq_equal (&a->dependent_typeids, &b->dependent_typeids);
}

void ddsi_typeobj_ser (const ddsi_typeobj_t *typeobj, unsigned char **buf, uint32_t *sz)
{
  dds_ostream_t os = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = CDR_ENC_VERSION_2 };
  dds_stream_writeLE ((dds_ostreamLE_t *) &os, (const void *) typeobj, DDS_XTypes_TypeObject_desc.m_ops);
  *buf = os.m_buffer;
  *sz = os.m_index;
}

void ddsi_typeobj_deser (unsigned char *buf, uint32_t sz, ddsi_typeobj_t **typeobj)
{
  unsigned char *data;
  uint32_t srcoff = 0;
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  if (bswap)
  {
    data = ddsrt_memdup (buf, sz);
    dds_stream_normalize1 ((char *) data, &srcoff, sz, bswap, CDR_ENC_VERSION_2, DDS_XTypes_TypeObject_desc.m_ops);
  }
  else
    data = buf;

  dds_istream_t is = { .m_buffer = data, .m_index = 0, .m_size = sz, .m_xcdr_version = CDR_ENC_VERSION_2 };
  dds_stream_read (&is, (void *) *typeobj, DDS_XTypes_TypeObject_desc.m_ops);
}

bool ddsi_typeobj_is_minimal (const ddsi_typeobj_t *typeobj)
{
  return typeobj != NULL && typeobj->_d == DDS_XTypes_EK_MINIMAL;
}

bool ddsi_typeobj_is_complete (const ddsi_typeobj_t *typeobj)
{
  return typeobj != NULL && typeobj->_d == DDS_XTypes_EK_COMPLETE;
}

bool ddsi_type_information_equal (const ddsi_typeinfo_t *a, const ddsi_typeinfo_t *b)
{
  if (a == NULL || b == NULL)
    return a == b;
  return type_id_with_deps_equal (&a->minimal, &b->minimal) && type_id_with_deps_equal (&a->complete, &b->complete);
}

