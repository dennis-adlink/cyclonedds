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
#include <stdint.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_xt_wrap.h"
#include "dds/ddsi/ddsi_xt.h"
#include "dds/ddsi/ddsi_type_lookup.h"

static void xt_collection_common_init (struct xt_collection_common *xtcc, const struct DDS_XTypes_PlainCollectionHeader *hdr)
{
  xtcc->ek = hdr->equiv_kind;
  xtcc->element_flags = hdr->element_flags;
}

static void xt_sbounds_to_lbounds (struct DDS_XTypes_LBoundSeq *lb, const struct DDS_XTypes_SBoundSeq *sb)
{
  lb->_length = sb->_length;
  lb->_buffer = ddsrt_malloc (sb->_length * sizeof (*lb->_buffer));
  for (uint32_t n = 0; n < sb->_length; n++)
    lb->_buffer[n] = (DDS_XTypes_LBound) sb->_buffer[n];
}

static void xt_lbounds_dup (struct DDS_XTypes_LBoundSeq *dst, const struct DDS_XTypes_LBoundSeq *src)
{
  dst->_length = src->_length;
  dst->_buffer = ddsrt_memdup (&src->_buffer, dst->_length * sizeof (*dst->_buffer));
}

static void add_minimal_typeobj (struct xt_type *xt, const ddsi_typeobj_t *to)
{
  const struct DDS_XTypes_MinimalTypeObject *mto = &to->_u.minimal;
  xt->_d = mto->_d;
  switch (to->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      xt->_u.alias.related_type = ddsi_xt_type_init (&mto->_u.alias_type.body.common.related_type, NULL);
      break;
    case DDS_XTypes_TK_ANNOTATION:
      abort (); /* FIXME: not implemented */
      break;
    case DDS_XTypes_TK_STRUCTURE:
      xt->_u.structure.flags = mto->_u.struct_type.struct_flags;
      xt->_u.structure.base_type = ddsi_xt_type_init (&mto->_u.struct_type.header.base_type, NULL);
      xt->_u.structure.members.length = mto->_u.struct_type.member_seq._length;
      xt->_u.structure.members.seq = ddsrt_malloc (xt->_u.structure.members.length * sizeof (*xt->_u.structure.members.seq));
      for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
      {
        xt->_u.structure.members.seq[n].type = ddsi_xt_type_init (&mto->_u.struct_type.member_seq._buffer[n].common.member_type_id, NULL);
        memcpy (xt->_u.structure.members.seq[n].name_hash, mto->_u.struct_type.member_seq._buffer[n].detail.name_hash,
          sizeof (xt->_u.structure.members.seq[n].name_hash));
      }
      break;
    case DDS_XTypes_TK_UNION:
      xt->_u.union_type.flags = mto->_u.union_type.union_flags;
      xt->_u.union_type.disc_type = ddsi_xt_type_init (&mto->_u.union_type.discriminator.common.type_id, NULL);
      xt->_u.union_type.disc_flags = mto->_u.union_type.discriminator.common.member_flags;
      xt->_u.union_type.members.length = mto->_u.union_type.member_seq._length;
      xt->_u.union_type.members.seq = ddsrt_malloc (xt->_u.union_type.members.length * sizeof (*xt->_u.union_type.members.seq));
      for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
      {
        xt->_u.union_type.members.seq[n].id = mto->_u.union_type.member_seq._buffer[n].common.member_id;
        xt->_u.union_type.members.seq[n].flags = mto->_u.union_type.member_seq._buffer[n].common.member_flags;
        xt->_u.union_type.members.seq[n].type = ddsi_xt_type_init (&mto->_u.union_type.member_seq._buffer[n].common.type_id, NULL);
        xt->_u.union_type.members.seq[n].label_seq._length = mto->_u.union_type.member_seq._buffer[n].common.label_seq._length;
        xt->_u.union_type.members.seq[n].label_seq._buffer = ddsrt_memdup (&mto->_u.union_type.member_seq._buffer[n].common.label_seq._buffer,
          mto->_u.union_type.member_seq._buffer[n].common.label_seq._length * sizeof (*mto->_u.union_type.member_seq._buffer[n].common.label_seq._buffer));
        memcpy (xt->_u.union_type.members.seq[n].name_hash, mto->_u.union_type.member_seq._buffer[n].detail.name_hash,
          sizeof (xt->_u.union_type.members.seq[n].name_hash));
      }
      break;
    case DDS_XTypes_TK_BITSET:
      xt->_u.bitset.fields.length = mto->_u.bitset_type.field_seq._length;
      xt->_u.bitset.fields.seq = ddsrt_malloc (xt->_u.bitset.fields.length * sizeof (*xt->_u.bitset.fields.seq));
      for (uint32_t n = 0; n < xt->_u.bitset.fields.length; n++)
      {
        xt->_u.bitset.fields.seq[n].position = mto->_u.bitset_type.field_seq._buffer[n].common.position;
        xt->_u.bitset.fields.seq[n].bitcount = mto->_u.bitset_type.field_seq._buffer[n].common.bitcount;
        xt->_u.bitset.fields.seq[n].holder_type = mto->_u.bitset_type.field_seq._buffer[n].common.holder_type;
        memcpy (xt->_u.bitset.fields.seq[n].name_hash, mto->_u.bitset_type.field_seq._buffer[n].name_hash,
          sizeof (xt->_u.bitset.fields.seq[n].name_hash));
      }
      break;
    case DDS_XTypes_TK_SEQUENCE:
      xt->_u.seq.c.element_type = ddsi_xt_type_init (&mto->_u.sequence_type.element.common.type, NULL);
      xt->_u.seq.c.element_flags = mto->_u.sequence_type.element.common.element_flags;
      xt->_u.seq.bound = mto->_u.sequence_type.header.common.bound;
      break;
    case DDS_XTypes_TK_ARRAY:
      xt->_u.array.c.element_type = ddsi_xt_type_init (&mto->_u.array_type.element.common.type, NULL);
      xt->_u.array.c.element_flags = mto->_u.array_type.element.common.element_flags;
      xt_lbounds_dup (&xt->_u.array.bounds, &mto->_u.array_type.header.common.bound_seq);
      break;
    case DDS_XTypes_TK_MAP:
      xt->_u.map.c.element_type = ddsi_xt_type_init (&mto->_u.map_type.element.common.type, NULL);
      xt->_u.map.c.element_flags = mto->_u.array_type.element.common.element_flags;
      xt->_u.map.key_type = ddsi_xt_type_init (&mto->_u.map_type.key.common.type, NULL);
      xt->_u.map.bound = mto->_u.map_type.header.common.bound;
      break;
    case DDS_XTypes_TK_ENUM:
      xt->_u.enum_type.flags = mto->_u.enumerated_type.enum_flags;
      xt->_u.enum_type.bit_bound = mto->_u.enumerated_type.header.common.bit_bound;
      xt->_u.enum_type.literals.length = mto->_u.enumerated_type.literal_seq._length;
      xt->_u.enum_type.literals.seq = ddsrt_malloc (xt->_u.enum_type.literals.length * sizeof (*xt->_u.enum_type.literals.seq));
      for (uint32_t n = 0; n < xt->_u.enum_type.literals.length; n++)
      {
        xt->_u.enum_type.literals.seq[n].value = mto->_u.enumerated_type.literal_seq._buffer[n].common.value;
        xt->_u.enum_type.literals.seq[n].flags = mto->_u.enumerated_type.literal_seq._buffer[n].common.flags;
        memcpy (xt->_u.enum_type.literals.seq[n].name_hash, mto->_u.enumerated_type.literal_seq._buffer[n].detail.name_hash,
          sizeof (xt->_u.enum_type.literals.seq[n].name_hash));
      }
      break;
    case DDS_XTypes_TK_BITMASK:
      xt->_u.bitmask.flags = mto->_u.bitmask_type.bitmask_flags;
      xt->_u.bitmask.bit_bound = mto->_u.bitmask_type.header.common.bit_bound;
      xt->_u.bitmask.bitflags.length = mto->_u.bitmask_type.flag_seq._length;
      xt->_u.bitmask.bitflags.seq = ddsrt_malloc (xt->_u.bitmask.bitflags.length * sizeof (*xt->_u.bitmask.bitflags.seq));
      for (uint32_t n = 0; n < xt->_u.bitmask.bitflags.length; n++)
      {
        xt->_u.bitmask.bitflags.seq[n].position = mto->_u.bitmask_type.flag_seq._buffer[n].common.position;
        memcpy (xt->_u.bitmask.bitflags.seq[n].name_hash, mto->_u.bitmask_type.flag_seq._buffer[n].detail.name_hash,
          sizeof (xt->_u.bitmask.bitflags.seq[n].name_hash));
      }
      break;
    default:
      abort (); /* not supported */
  }
}

static void add_complete_typeobj (struct xt_type *xt, const ddsi_typeobj_t *to)
{
  // FIXME
  (void) xt;
  (void) to;
}

struct xt_type *ddsi_xt_type_init (const ddsi_typeid_t *ti, const ddsi_typeobj_t *to)
{
  assert (ti);
  struct xt_type *xt = ddsrt_calloc (1, sizeof (*xt));

  if (ddsi_typeid_is_minimal (ti))
  {
    xt->has_minimal_id = 1;
    memcpy (xt->minimal_hash, ti->_u.equivalence_hash, sizeof (xt->minimal_hash));
    if (to)
    {
      assert (ddsi_typeobj_is_minimal (to));
      xt->has_minimal_obj = 1;
    }
  }
  else
  {
    xt->has_complete_id = 1;
    memcpy (xt->complete_hash, ti->_u.equivalence_hash, sizeof (xt->complete_hash));
    if (to)
    {
      assert (ddsi_typeobj_is_complete (to));
      xt->has_complete_obj = 1;
    }
  }

  /* Primitive types */
  if (ti->_d <= DDS_XTypes_TK_CHAR16)
  {
    assert (to == NULL);
    xt->_d = ti->_d;
    return xt;
  }

  /* Other types */
  xt->is_plain_collection = ti->_d >= DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL && ti->_d <= DDS_XTypes_TI_PLAIN_MAP_LARGE;
  switch (ti->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
      xt->_d = DDS_XTypes_TK_STRING8;
      xt->_u.str8.bound = (DDS_XTypes_LBound) ti->_u.string_sdefn.bound;
      break;
    case DDS_XTypes_TI_STRING8_LARGE:
      xt->_d = DDS_XTypes_TK_STRING8;
      xt->_u.str8.bound = ti->_u.string_ldefn.bound;
      break;
    case DDS_XTypes_TI_STRING16_SMALL:
      xt->_d = DDS_XTypes_TK_STRING16;
      xt->_u.str16.bound = (DDS_XTypes_LBound) ti->_u.string_sdefn.bound;
      break;
    case DDS_XTypes_TI_STRING16_LARGE:
      xt->_d = DDS_XTypes_TK_STRING16;
      xt->_u.str16.bound = ti->_u.string_ldefn.bound;
      break;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      xt->_d = DDS_XTypes_TK_SEQUENCE;
      xt->_u.seq.c.element_type = ddsi_xt_type_init (ti->_u.seq_sdefn.element_identifier, NULL);
      xt->_u.seq.bound = (DDS_XTypes_LBound) ti->_u.seq_sdefn.bound;
      xt_collection_common_init (&xt->_u.seq.c, &ti->_u.seq_sdefn.header);
      break;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      xt->_d = DDS_XTypes_TK_SEQUENCE;
      xt->_u.seq.c.element_type = ddsi_xt_type_init (ti->_u.seq_ldefn.element_identifier, NULL);
      xt->_u.seq.bound = ti->_u.seq_ldefn.bound;
      xt_collection_common_init (&xt->_u.seq.c, &ti->_u.seq_ldefn.header);
      break;
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      xt->_d = DDS_XTypes_TK_ARRAY;
      xt->_u.array.c.element_type = ddsi_xt_type_init (ti->_u.array_sdefn.element_identifier, NULL);
      xt_collection_common_init (&xt->_u.array.c, &ti->_u.array_sdefn.header);
      xt_sbounds_to_lbounds (&xt->_u.array.bounds, &ti->_u.array_sdefn.array_bound_seq);
      break;
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      xt->_d = DDS_XTypes_TK_ARRAY;
      xt->_u.array.c.element_type = ddsi_xt_type_init (ti->_u.array_ldefn.element_identifier, NULL);
      xt_collection_common_init (&xt->_u.array.c, &ti->_u.array_ldefn.header);
      xt_lbounds_dup (&xt->_u.array.bounds, &ti->_u.array_ldefn.array_bound_seq);
      break;
    case DDS_XTypes_TI_PLAIN_MAP_SMALL:
      xt->_d = DDS_XTypes_TK_MAP;
      xt->_u.map.c.element_type = ddsi_xt_type_init (ti->_u.map_sdefn.element_identifier, NULL);
      xt->_u.map.bound = (DDS_XTypes_LBound) ti->_u.map_sdefn.bound;
      xt_collection_common_init (&xt->_u.map.c, &ti->_u.map_sdefn.header);
      xt->_u.map.key_type = ddsi_xt_type_init (ti->_u.map_sdefn.key_identifier, NULL);
      break;
    case DDS_XTypes_TI_PLAIN_MAP_LARGE:
      xt->_d = DDS_XTypes_TK_MAP;
      xt->_u.map.c.element_type = ddsi_xt_type_init (ti->_u.map_ldefn.element_identifier, NULL);
      xt->_u.map.bound = (DDS_XTypes_LBound) ti->_u.map_ldefn.bound;
      xt_collection_common_init (&xt->_u.map.c, &ti->_u.map_ldefn.header);
      xt->_u.map.key_type = ddsi_xt_type_init (ti->_u.map_ldefn.key_identifier, NULL);
      break;
    case DDS_XTypes_EK_MINIMAL:
      if (to != NULL)
        add_minimal_typeobj (xt, to);
      break;
    case DDS_XTypes_EK_COMPLETE:
      if (to != NULL)
        add_complete_typeobj (xt, to);
      break;
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
      xt->_d = DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT;
      xt->sc_component_id = ti->_u.sc_component_id;
      break;
    default:
      abort (); /* not supported */
      break;
  }
  return xt;
}

void ddsi_xt_type_add (struct xt_type *xt, const ddsi_typeid_t *ti, const ddsi_typeobj_t *to)
{
  assert (xt);
  if (ti != NULL)
  {
    if (ddsi_typeid_is_minimal (ti) && !xt->has_minimal_id)
    {
      xt->has_minimal_id = 1;
      memcpy (xt->minimal_hash, ti->_u.equivalence_hash, sizeof (xt->minimal_hash));
    }
    else if (!xt->has_complete_id)
    {
      xt->has_complete_id = 1;
      memcpy (xt->complete_hash, ti->_u.equivalence_hash, sizeof (xt->complete_hash));
    }
  }
  if (to != NULL)
  {
    if (ddsi_typeobj_is_minimal (to) && !xt->has_minimal_obj)
    {
      xt->has_minimal_obj = 1;
      add_minimal_typeobj (xt, to);
    }
    else if (!xt->has_complete_obj)
    {
      xt->has_complete_obj = 1;
      add_complete_typeobj (xt, to);
    }
  }
}

void ddsi_xt_type_fini (struct xt_type *xt)
{
  switch (xt->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      ddsi_xt_type_fini (xt->_u.alias.related_type);
      break;
    case DDS_XTypes_TK_ANNOTATION:
      abort (); /* FIXME: not implemented */
      break;
    case DDS_XTypes_TK_STRUCTURE:
      ddsi_xt_type_fini (xt->_u.structure.base_type);
      for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
        ddsi_xt_type_fini (xt->_u.structure.members.seq[n].type);
      ddsrt_free (xt->_u.structure.members.seq);
      break;
    case DDS_XTypes_TK_UNION:
      ddsi_xt_type_fini (xt->_u.union_type.disc_type);
      for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
      {
        ddsi_xt_type_fini (xt->_u.union_type.members.seq[n].type);
        ddsrt_free (xt->_u.union_type.members.seq[n].label_seq._buffer);
      }
      ddsrt_free (xt->_u.union_type.members.seq);
      break;
    case DDS_XTypes_TK_BITSET:
      ddsrt_free (xt->_u.bitset.fields.seq);
      break;
    case DDS_XTypes_TK_SEQUENCE:
      ddsi_xt_type_fini (xt->_u.seq.c.element_type);
      break;
    case DDS_XTypes_TK_ARRAY:
      ddsi_xt_type_fini (xt->_u.array.c.element_type);
      ddsrt_free (xt->_u.array.bounds._buffer);
      break;
    case DDS_XTypes_TK_MAP:
      ddsi_xt_type_fini (xt->_u.map.c.element_type);
      ddsi_xt_type_fini (xt->_u.map.key_type);
      break;
    case DDS_XTypes_TK_ENUM:
      ddsrt_free (xt->_u.enum_type.literals.seq);
      break;
    case DDS_XTypes_TK_BITMASK:
      ddsrt_free (xt->_u.bitmask.bitflags.seq);
      break;
    default:
      abort (); /* not supported */
      break;
  }
  ddsrt_free (xt);
}

static struct xt_type *xt_clone (const struct xt_type *xt)
{
  /* FIXME */
  (void) xt;
  abort ();
  return NULL;
}

static ddsi_typeid_t * xt_minimal_typeid (const struct xt_type *xt)
{
  /* FIXME */
  (void) xt;
  abort ();
  return NULL;
}

static bool xt_has_basetype (const struct xt_type *t)
{
  assert (t->_d == DDS_XTypes_TK_STRUCTURE);
  return t->_u.structure.base_type != NULL;
}

static struct xt_type *xt_expand_basetype (const struct xt_type *t)
{
  assert (t->_d == DDS_XTypes_TK_STRUCTURE);
  assert (t->_u.structure.base_type);
  struct xt_type *b = t->_u.structure.base_type,
    *te = xt_has_basetype (b) ? xt_expand_basetype (b) : xt_clone (t);
  struct xt_struct_member_seq ms = te->_u.structure.members;
  uint32_t incr = b->_u.structure.members.length;
  ms.length += incr;
  ms.seq = ddsrt_realloc (ms.seq, ms.length * sizeof (*ms.seq));
  memmove (&ms.seq[incr], ms.seq, incr * sizeof (*ms.seq));
  return te;
}

static struct xt_type *xt_type_key_erased (const struct xt_type *t)
{
  switch (t->_d)
  {
    case DDS_XTypes_TK_STRUCTURE: {
      struct xt_type *tke = xt_clone (t);
      for (uint32_t i = 0; i < tke->_u.structure.members.length; i++)
      {
        struct xt_struct_member *m = &tke->_u.structure.members.seq[i];
        if (m->flags & DDS_XTypes_IS_KEY)
          m->flags &= (DDS_XTypes_MemberFlag) ~DDS_XTypes_IS_KEY;
      }
      return tke;
    }
    case DDS_XTypes_TK_UNION: {
      struct xt_type *tke = xt_clone (t);
      for (uint32_t i = 0; i < tke->_u.union_type.members.length; i++)
      {
        struct xt_union_member *m = &tke->_u.union_type.members.seq[i];
        if (m->flags & DDS_XTypes_IS_KEY)
          m->flags &= (DDS_XTypes_MemberFlag) ~DDS_XTypes_IS_KEY;
      }
      return tke;
    }
    default:
      assert (false);
      return NULL;
  }
}

static bool xt_struct_has_key (const struct xt_type *t)
{
  assert (t->_d == DDS_XTypes_TK_STRUCTURE);
  for (uint32_t i = 0; i < t->_u.structure.members.length; i++)
    if (t->_u.structure.members.seq[i].flags & DDS_XTypes_IS_KEY)
      return true;
  return false;
}

static struct xt_type *xt_type_keyholder (const struct xt_type *t)
{
  struct xt_type *tkh = xt_clone (t);
  switch (tkh->_d)
  {
    case DDS_XTypes_TK_STRUCTURE: {
      if (xt_struct_has_key (tkh))
      {
        /* Rule: If T has any members designated as key members see 7.2.2.4.4.4.8), then KeyHolder(T) removes any
          members of T that are not designated as key members. */
        uint32_t i = 0, l = t->_u.structure.members.length;
        while (i < l)
        {
          if (tkh->_u.structure.members.seq[i].flags & DDS_XTypes_IS_KEY)
          {
            i++;
            continue;
          }
          if (i < l - 1)
            memmove (&tkh->_u.structure.members.seq[i], &tkh->_u.structure.members.seq[i + 1], (l - i - 1) * sizeof (*tkh->_u.structure.members.seq));
          l--;
        }
        tkh->_u.structure.members.length = l;
      }
      else
      {
        /* Rule: If T is a structure with no key members, then KeyHolder(T) adds a key designator to each member. */
        for (uint32_t i = 0; i < t->_u.structure.members.length; i++)
          t->_u.structure.members.seq[i].flags |= DDS_XTypes_IS_KEY;
      }
      return tkh;
    }
    case DDS_XTypes_TK_UNION: {
      /* Rules:
         - If T has discriminator as key, then KeyHolder(T) removes any members of T that are not designated as key members.
         - If T is a union and the discriminator is not marked as key, then KeyHolder(T) is the same type T. */
      if (tkh->_u.union_type.disc_flags & DDS_XTypes_IS_KEY)
      {
        tkh->_u.union_type.members.length = 0;
        ddsrt_free (tkh->_u.union_type.members.seq);
      }
      return tkh;
    }
    default:
      assert (false);
      ddsi_xt_type_fini (tkh);
      return NULL;
  }
}

static bool xt_is_plain_collection (const struct xt_type *t)
{
  return t->is_plain_collection;
}

static bool xt_is_plain_collection_equiv_kind (const struct xt_type *t, DDS_XTypes_EquivalenceKind ek)
{
  if (!xt_is_plain_collection (t))
    return false;
  switch (t->_d)
  {
    case DDS_XTypes_TK_SEQUENCE:
      return t->_u.seq.c.ek == ek;
    case DDS_XTypes_TK_ARRAY:
      return t->_u.array.c.ek == ek;
    case DDS_XTypes_TK_MAP:
      return t->_u.map.c.ek == ek;
    default:
      abort ();
  }
}

static bool xt_is_plain_collection_fully_descriptive_typeid (const struct xt_type *t)
{
  return xt_is_plain_collection_equiv_kind (t, DDS_XTypes_EK_BOTH);
}

static bool xt_is_equiv_kind_hash_typeid (const struct xt_type *t, DDS_XTypes_EquivalenceKind ek)
{
  return t->_d == ek
    || (t->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT && t->sc_component_id.sc_component_id._d == ek)
    || xt_is_plain_collection_equiv_kind (t, ek);
}

static bool xt_is_minimal_hash_typeid (const struct xt_type *t)
{
  return xt_is_equiv_kind_hash_typeid (t, DDS_XTypes_EK_MINIMAL);
}

static bool xt_is_primitive (const struct xt_type *t)
{
  return t->_d <= DDS_XTypes_TK_CHAR16;
}

static bool xt_is_string (const struct xt_type *t)
{
  return t->_d == DDS_XTypes_TK_STRING8 || t->_d == DDS_XTypes_TK_STRING16;
}

static DDS_XTypes_LBound xt_string_bound (const struct xt_type *t)
{
  switch (t->_d)
  {
    case DDS_XTypes_TK_STRING8:
      return t->_u.str8.bound;
    case DDS_XTypes_TK_STRING16:
      return t->_u.str16.bound;
    default:
      abort (); /* not supported */
  }
}

static bool xt_is_fully_descriptive_typeid (const struct xt_type *t)
{
  return xt_is_primitive (t) || xt_is_string (t) || (t->_d != DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT && xt_is_plain_collection_fully_descriptive_typeid (t));
}

static bool xt_is_enumerated (const struct xt_type *t)
{
  return t->_d == DDS_XTypes_TK_ENUM || t->_d == DDS_XTypes_TK_BITMASK;
}

static uint32_t xt_get_extensibility (const struct xt_type *t)
{
  uint32_t ext_flags;
  switch (t->_d)
  {
    case DDS_XTypes_TK_ENUM:
      ext_flags = t->_u.enum_type.flags & XT_FLAG_EXTENSIBILITY_MASK;
      break;
    case DDS_XTypes_TK_BITMASK:
      ext_flags = t->_u.bitmask.flags & XT_FLAG_EXTENSIBILITY_MASK;
      break;
    case DDS_XTypes_TK_STRUCTURE:
      ext_flags = t->_u.structure.flags & XT_FLAG_EXTENSIBILITY_MASK;
      break;
    case DDS_XTypes_TK_UNION:
      ext_flags = t->_u.union_type.flags & XT_FLAG_EXTENSIBILITY_MASK;
      break;
    default:
      return 0;
  }
  assert (ext_flags == DDS_XTypes_IS_FINAL || ext_flags == DDS_XTypes_IS_APPENDABLE || ext_flags == DDS_XTypes_IS_MUTABLE);
  return ext_flags;
}

static bool xt_is_delimited (const struct ddsi_domaingv *gv, const struct xt_type *t)
{
  if (xt_is_primitive (t) || xt_is_string (t) || xt_is_enumerated (t))
    return true;
  switch (t->_d)
  {
    case DDS_XTypes_TK_SEQUENCE:
      return xt_is_delimited (gv, t->_u.seq.c.element_type);
    case DDS_XTypes_TK_ARRAY:
      return xt_is_delimited (gv, t->_u.array.c.element_type);
    case DDS_XTypes_TK_MAP:
      return xt_is_delimited (gv, t->_u.map.key_type) && xt_is_delimited (gv, t->_u.map.c.element_type);
  }
  uint32_t ext = xt_get_extensibility (t);
  if (ext == DDS_XTypes_IS_APPENDABLE) /* FIXME: && encoding == XCDR2 */
    return true;
  return ext == DDS_XTypes_IS_MUTABLE;
}

static bool xt_is_equivalent_minimal (const struct xt_type *t1, const struct xt_type *t2)
{
  // Minimal equivalence relation (XTypes spec v1.3 section 7.3.4.7)
  if ((xt_is_fully_descriptive_typeid (t1) || xt_is_minimal_hash_typeid (t1))
      && !ddsi_typeid_compare (xt_minimal_typeid (t1), xt_minimal_typeid (t2)))
    return true;
  return false;
}

static bool xt_is_strongly_assignable_from (const struct ddsi_domaingv *gv, const struct xt_type *t1, const struct xt_type *t2)
{
  if (xt_is_equivalent_minimal (t1, t2))
    return true;
  return xt_is_delimited (gv, t2) && ddsi_xt_is_assignable_from (gv, t1, t2);
}

static bool xt_bounds_eq (const struct DDS_XTypes_LBoundSeq *a, const struct DDS_XTypes_LBoundSeq *b)
{
  if (!a || !b)
    return false;
  if (a->_length != b->_length)
    return false;
  return !memcmp (a->_buffer, b->_buffer, a->_length * sizeof (*a->_buffer));
}

static bool xt_namehash_eq (const DDS_XTypes_NameHash *n1, const DDS_XTypes_NameHash *n2)
{
  return !memcmp (n1, n2, sizeof (*n1));
}

static bool xt_union_label_selects (const struct DDS_XTypes_UnionCaseLabelSeq *ls1, const struct DDS_XTypes_UnionCaseLabelSeq *ls2)
{
  /* UnionCaseLabelSeq is ordered by value (as noted in typeobject idl) */
  uint32_t i1 = 0, i2 = 0;
  while (i1 < ls1->_length && i2 < ls2->_length)
  {
    if (ls1->_buffer[i1] == ls2->_buffer[i2])
      return true;
    else if (ls1->_buffer[i1] < ls2->_buffer[i2])
      i1++;
    else
      i2++;
  }
  return false;
}

static bool xt_is_assignable_from_enum (const struct xt_type *t1, const struct xt_type *t2)
{
  assert (t1->_d == DDS_XTypes_TK_ENUM);
  assert (t2->_d == DDS_XTypes_TK_ENUM);
  /* Note: extensibility flags not defined, see https://issues.omg.org/issues/DDSXTY14-24 */
  if (xt_get_extensibility (t1) != xt_get_extensibility (t2))
    return false;
  /* Members are ordered by increasing value (XTypes 1.3 spec 7.3.4.5) */
  uint32_t i1 = 0, i2 = 0, i1_max = t1->_u.enum_type.literals.length, i2_max = t2->_u.enum_type.literals.length;
  while (i1 < i1_max && i2 < i2_max)
  {
    struct xt_enum_literal *l1 = &t1->_u.enum_type.literals.seq[i1], *l2 = &t2->_u.enum_type.literals.seq[i2];
    if (l1->value == l2->value)
    {
      /* FIXME: implement @ignore_literal_names */
      if (!xt_namehash_eq (&l1->name_hash, &l2->name_hash))
        return false;
      i1++;
      i2++;
    }
    else if (xt_get_extensibility (t1) == DDS_XTypes_IS_FINAL)
      return false;
    else if (l1->value < l2->value)
      i1++;
    else
      i2++;
  }
  return true;
}

static bool xt_is_assignable_from_union (const struct ddsi_domaingv *gv, const struct xt_type *t1, const struct xt_type *t2)
{
  assert (t1->_d == DDS_XTypes_TK_UNION);
  assert (t2->_d == DDS_XTypes_TK_UNION);
  if (xt_get_extensibility (t1) != xt_get_extensibility (t2))
    return false;
  if (!xt_is_strongly_assignable_from (gv, t1->_u.union_type.disc_type, t2->_u.union_type.disc_type))
    return false;

  /* Rule: Either the discriminators of both T1 and T2 are keys or neither are keys. */
  if ((t1->_u.union_type.disc_flags & DDS_XTypes_IS_KEY) != (t2->_u.union_type.disc_flags & DDS_XTypes_IS_KEY))
    return false;

  /* Note that union members are ordered by their member index (=ordering in idl) and not by their member ID */
  uint32_t i1_max = t1->_u.union_type.members.length, i2_max = t2->_u.union_type.members.length;
  bool any_match = false;
  for (uint32_t i1 = 0; i1 < i1_max; i1++)
  {
    struct xt_union_member *m1 = &t1->_u.union_type.members.seq[i1];
    bool m2_match = false;
    for (uint32_t i2 = i1; i2 < i2_max + i1; i2++)
    {
      struct xt_union_member *m2 = &t2->_u.union_type.members.seq[i2 % i2_max];
      if (m1->id == m2->id)
      {
        /* Rule: Any members in T1 and T2 that have the same name also have the same ID and any members
        with the same ID also have the same name. */
        if (!xt_namehash_eq (&m1->name_hash, &m2->name_hash))
          return false;
      }
      bool match;
      if ((match = xt_union_label_selects (&m1->label_seq, &m2->label_seq)))
        m2_match = true;
      /* Rule: For all non-default labels in T2 that select some member in T1 (including selecting the member in T1’s
        default label), the type of the selected member in T1 is assignable from the type of the T2 member. */
      if ((match || (m1->flags & DDS_XTypes_IS_DEFAULT)) && !(m2->flags & DDS_XTypes_IS_DEFAULT) && !ddsi_xt_is_assignable_from (gv, m1->type, m2->type))
        return false;
      /* Rule: If any non-default labels in T1 that select the default member in T2, the type of the member in T1 is
        assignable from the type of the T2 default member. */
      if (!match && !(m1->flags & DDS_XTypes_IS_DEFAULT) && (m2->flags & DDS_XTypes_IS_DEFAULT) && !ddsi_xt_is_assignable_from (gv, m1->type, m2->type))
        return false;
      /* Rule: If T1 and T2 both have default labels, the type associated with T1 default member is assignable from
          the type associated with T2 default member. */
      if ((m1->flags & DDS_XTypes_IS_DEFAULT) && (m2->flags & DDS_XTypes_IS_DEFAULT))
      {
        m2_match = true;
        if (!ddsi_xt_is_assignable_from (gv, m1->type, m2->type))
          return false;
      }
    }
    /* Rule: If T1 (and therefore T2) extensibility is final then the set of labels is identical. */
    if (!m2_match && xt_get_extensibility (t1) == DDS_XTypes_IS_FINAL)
      return false;
    if (m2_match)
      any_match = true;
  }
  /* Rule: [extensibility is final], otherwise, they have at least one common label other than the default label. */
  if (!any_match)
    return false;
  return true;
}

static bool xt_is_assignable_from_struct (const struct ddsi_domaingv *gv, const struct xt_type *t1, const struct xt_type *t2)
{
  assert (t1->_d == DDS_XTypes_TK_STRUCTURE);
  assert (t2->_d == DDS_XTypes_TK_STRUCTURE);
  bool result = false;
  struct xt_type *te1 = (struct xt_type *) t1, *te2 = (struct xt_type *) t2;
  if (xt_get_extensibility (t1) != xt_get_extensibility (t2))
    goto struct_failed;
  if (xt_has_basetype (t1))
    te1 = xt_expand_basetype (t1);
  if (xt_has_basetype (t2))
    te2 = xt_expand_basetype (t2);
  /* Note that struct members are ordered by their member index (=ordering in idl) and not by their member ID (although the
      TypeObject idl states that its ordered by member_id...) */
  uint32_t i1_max = te1->_u.structure.members.length, i2_max = te2->_u.structure.members.length;
  bool any_member_match = false;
  for (uint32_t i1 = 0; i1 < i1_max; i1++)
  {
    struct xt_struct_member *m1 = &te1->_u.structure.members.seq[i1];
    bool match = false,
      m1_opt = (m1->flags & DDS_XTypes_IS_OPTIONAL),
      m1_mu = (m1->flags & DDS_XTypes_IS_MUST_UNDERSTAND),
      m1_k = (m1->flags & DDS_XTypes_IS_KEY);
    for (uint32_t i2 = i1; i2 < i2_max + i1; i2++)
    {
      struct xt_struct_member *m2 = &te2->_u.structure.members.seq[i2 % i2_max];
      if (m1->id == m2->id)
      {
        bool m2_k = (m2->flags & DDS_XTypes_IS_KEY);
        any_member_match = true;
        match = true;
        /* Rule: "Any members in T1 and T2 that have the same name also have the same ID and any members with the
            same ID also have the same name." */
        if (!xt_namehash_eq (&m1->name_hash, &m2->name_hash))
          goto struct_failed;
        /* Rule: "For any member m2 in T2, if there is a member m1 in T1 with the same member ID, then the type
            KeyErased(m1.type) is-assignable from the type KeyErased(m2.type) */
        struct xt_type *m1_ke = xt_type_key_erased (m1->type),
          *m2_ke = xt_type_key_erased (m2->type);
        bool ke_assignable = ddsi_xt_is_assignable_from (gv, m1_ke, m2_ke);
        ddsi_xt_type_fini (m1_ke);
        ddsi_xt_type_fini (m2_ke);
        if (!ke_assignable)
          goto struct_failed;
        /* Rule: "For any string key member m2 in T2, the m1 member of T1 with the same member ID verifies m1.type.length >= m2.type.length. */
        if (m2_k && xt_is_string (m2->type) && xt_string_bound (m1->type) < xt_string_bound (m2->type))
          goto struct_failed;
        /* Rule: "For any enumerated key member m2 in T2, the m1 member of T1 with the same member ID verifies that all
            literals in m2.type appear as literals in m1.type" */
        if (m2_k && xt_is_enumerated (m2->type))
        {
          uint32_t ki1 = 0, ki2 = 0, ki1_max = m1->type->_u.enum_type.literals.length, ki2_max = m2->type->_u.enum_type.literals.length;
          while (ki1 < ki1_max && ki2 < ki2_max)
          {
            struct xt_enum_literal *kl1 = &m1->type->_u.enum_type.literals.seq[ki1], *kl2 = &m2->type->_u.enum_type.literals.seq[ki2];
            if (kl1->value == kl2->value)
            {
              ki1++;
              ki2++;
            }
            else if (kl1->value < kl2->value)
              ki1++;
            else
              goto struct_failed;
          }
        }

        /* Rule: "For any sequence or map key member m2 in T2, the m1 member of T1 with the same member ID verifies m1.type.length >= m2.type.length" */
        if (m2_k && m2->type->_d == DDS_XTypes_TK_SEQUENCE && m1->type->_u.seq.bound < m2->type->_u.seq.bound)
          goto struct_failed;
        if (m2_k && m2->type->_d == DDS_XTypes_TK_MAP && m1->type->_u.map.bound < m2->type->_u.map.bound)
          goto struct_failed;
        /* Rule: "For any structure or union key member m2 in T2, the m1 member of T1 with the same member ID verifies that KeyHolder(m1.type)
            isassignable-from KeyHolder(m2.type)." */
        if (m2_k && (m2->type->_d == DDS_XTypes_TK_STRUCTURE || m2->type->_d == DDS_XTypes_TK_UNION))
        {
          struct xt_type *m1_kh = xt_type_keyholder (m1->type),
            *m2_kh = xt_type_keyholder (m2->type);
          bool kh_assignable = ddsi_xt_is_assignable_from (gv, m1_kh, m2_kh);
          ddsi_xt_type_fini (m1_kh);
          ddsi_xt_type_fini (m2_kh);
          if (!kh_assignable)
            goto struct_failed;
        }
        /* Rule: "For any union key member m2 in T2, the m1 member of T1 with the same member ID verifies that: For every discriminator value of m2.type
            that selects a member m22 in m2.type, the discriminator value selects a member m11 in m1.type that verifies KeyHolder(m11.type)
            is-assignable-from KeyHolder(m22.type)." */
        if (m2_k && m2->type->_d == DDS_XTypes_TK_UNION)
        {
          uint32_t ki1_max = m1->type->_u.union_type.members.length, ki2_max = m2->type->_u.union_type.members.length;
          for (uint32_t ki1 = 0; ki1 < ki1_max; ki1++)
          {
            struct xt_union_member *km1 = &m1->type->_u.union_type.members.seq[i1];
            for (uint32_t ki2 = ki1; ki2 < ki2_max + ki1; ki2++)
            {
              struct xt_union_member *km2 = &m2->type->_u.union_type.members.seq[ki2 % ki2_max];
              if (xt_union_label_selects (&km1->label_seq, &km2->label_seq))
              {
                struct xt_type *km1_kh = xt_type_keyholder (km1->type),
                  *km2_kh = xt_type_keyholder (km2->type);
                bool kh_assignable = ddsi_xt_is_assignable_from (gv, km1_kh, km2_kh);
                ddsi_xt_type_fini (km1_kh);
                ddsi_xt_type_fini (km2_kh);
                if (!kh_assignable)
                  goto struct_failed;
              }
            }
          }
        }
      }
    }
    /* Rule (for T1 members): "Members for which both optional is false and must_understand is true in either T1 or T2 appear (i.e., have a
        corresponding member of the same member ID) in both T1 and T2. */
    if (!m1_opt && m1_mu && !match)
      goto struct_failed;
    /* Rule (for T1 members): "Members marked as key in either T1 or T2 appear (i.e., have a corresponding member of the same member ID)
        in both T1 and T2." */
    if (m1_k && !match)
      goto struct_failed;
    /* Rules:
        - if T1 is appendable, then members with the same member_index have the same member ID, the same setting for the
          optional attribute and the T1 member type is strongly assignable from the T2 member type
        - if T1 is final, then they meet the same condition as for T1 being appendable and in addition T1 and T2 have the
          same set of member IDs. */
    if ((xt_get_extensibility (te1) == DDS_XTypes_IS_APPENDABLE && i1 < i2_max) || xt_get_extensibility (te1) == DDS_XTypes_IS_FINAL)
    {
      if (i1 >= i2_max)
        goto struct_failed;
      struct xt_struct_member *m2 = &te2->_u.structure.members.seq[i1];
      if (m1->id != m2->id || (m1->flags & DDS_XTypes_IS_OPTIONAL) != (m2->flags & DDS_XTypes_IS_OPTIONAL) || !xt_is_strongly_assignable_from (gv, m1->type, m2->type))
        goto struct_failed;
    }
  }
  /* Rules (for T2 members):
      - Members for which both optional is false and must_understand is true in either T1 or T2 appear (i.e., have a corresponding member
        of the same member ID) in both T1 and T2.
      - Members marked as key in either T1 or T2 appear (i.e., have a corresponding member of the same member ID) in both T1 and T2. */
  for (uint32_t i2 = 0; i2 < i2_max; i2++)
  {
    struct xt_struct_member *m2 = &te2->_u.structure.members.seq[i2 % i2_max];
    bool match = false;
    if ((m2->flags & DDS_XTypes_IS_OPTIONAL) || !(m2->flags & DDS_XTypes_IS_MUST_UNDERSTAND))
      continue;
    if (m2->flags & DDS_XTypes_IS_KEY)
      continue;
    for (uint32_t i1 = i2; !match && i1 < i1_max + i2; i1++)
      match = (te1->_u.structure.members.seq[i1 % i1_max].id == m2->id);
    if (!match)
      goto struct_failed;
  }
  /* Rule: There is at least one member m1 of T1 and one corresponding member m2 of T2 such that m1.id == m2.id */
  if (!any_member_match)
    goto struct_failed;
  result = true;

struct_failed:
  if (te1 != t1)
    ddsi_xt_type_fini (te1);
  if (te2 != t2)
    ddsi_xt_type_fini (te2);
  return result;
}

bool ddsi_xt_is_assignable_from (const struct ddsi_domaingv *gv, const struct xt_type *t1, const struct xt_type *t2)
{
  if (xt_is_equivalent_minimal (t1, t2))
    return true;

  /* Alias types */
  if (t1->_d == DDS_XTypes_TK_ALIAS || t2->_d == DDS_XTypes_TK_ALIAS)
  {
    return ddsi_xt_is_assignable_from (gv,
      t1->_d == DDS_XTypes_TK_ALIAS ? t1->_u.alias.related_type : t1,
      t2->_d == DDS_XTypes_TK_ALIAS ? t2->_u.alias.related_type : t2);
  }

  /* Bitmask type: must be equal, except bitmask can be assigned to uint types and vv */
  if (t1->_d == DDS_XTypes_TK_BITMASK || t2->_d == DDS_XTypes_TK_BITMASK)
  {
    const struct xt_type *t_bm = t1->_d == DDS_XTypes_TK_BITMASK ? t1 : t2;
    const struct xt_type *t_other = t1->_d == DDS_XTypes_TK_BITMASK ? t2 : t1;
    DDS_XTypes_BitBound bb = t_bm->_u.bitmask.bit_bound;
    switch (t_other->_d)
    {
      case DDS_XTypes_TK_BITMASK:
        return bb == t_other->_u.bitmask.bit_bound;
      // case TK_UINT8:   /* FIXME: TK_UINT8 not defined in idl */
      //   return bb >= 1 && bb <= 8;
      case DDS_XTypes_TK_UINT16:
        return bb >= 9 && bb <= 16;
      case DDS_XTypes_TK_UINT32:
        return bb >= 17 && bb <= 32;
      case DDS_XTypes_TK_UINT64:
        return bb >= 33 && bb <= 64;
      default:
        return false;
    }
  }

  /* String types: character type must be assignable, bound not checked for assignability */
  if ((t1->_d == DDS_XTypes_TK_STRING8 && t2->_d == DDS_XTypes_TK_STRING8) || (t1->_d == DDS_XTypes_TK_STRING16 && t2->_d == DDS_XTypes_TK_STRING16))
    return true;

  /* Collection types */
  if (t1->_d == DDS_XTypes_TK_ARRAY && t2->_d == DDS_XTypes_TK_ARRAY
      && xt_bounds_eq (&t1->_u.array.bounds, &t2->_u.array.bounds)
      && xt_is_strongly_assignable_from (gv, t1->_u.array.c.element_type, t2->_u.array.c.element_type))
    return true;
  if (t1->_d == DDS_XTypes_TK_SEQUENCE && t2->_d == DDS_XTypes_TK_SEQUENCE
      && xt_is_strongly_assignable_from (gv, t1->_u.seq.c.element_type, t2->_u.seq.c.element_type))
    return true;
  if (t1->_d == DDS_XTypes_TK_MAP && t2->_d == DDS_XTypes_TK_MAP
      && xt_is_strongly_assignable_from (gv, t1->_u.map.key_type, t2->_u.map.key_type)
      && xt_is_strongly_assignable_from (gv, t1->_u.map.c.element_type, t2->_u.map.c.element_type))
    return true;

  if (t1->_d == DDS_XTypes_TK_ENUM && t2->_d == DDS_XTypes_TK_ENUM && !xt_is_assignable_from_enum (t1, t2))
    return false;

  if (t1->_d == DDS_XTypes_TK_UNION && t2->_d == DDS_XTypes_TK_UNION && !xt_is_assignable_from_union (gv, t1, t2))
    return false;

  if (t1->_d == DDS_XTypes_TK_STRUCTURE && t2->_d == DDS_XTypes_TK_STRUCTURE && !xt_is_assignable_from_struct (gv, t1, t2))
    return false;

  return true;
}

bool ddsi_xt_has_complete_typeid (const struct xt_type *xt)
{
  return xt_is_fully_descriptive_typeid (xt);
}
