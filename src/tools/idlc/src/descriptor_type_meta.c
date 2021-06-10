/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "dds/ddsrt/md5.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"
#include "dds/ddsi/ddsi_xt_typemap.h"
#include "dds/ddsc/dds_opcodes.h"

#include "idl/print.h"
#include "idl/processor.h"
#include "idl/stream.h"
#include "idl/string.h"
#include "expression.h"
#include "generator.h"
#include "descriptor_type_meta.h"

struct type_meta {
  struct type_meta *admin_next;
  struct type_meta *stack_prev;
  const void *node;
  DDS_XTypes_TypeIdentifier *ti_complete;
  DDS_XTypes_TypeObject *to_complete;
  DDS_XTypes_TypeIdentifier *ti_minimal;
  DDS_XTypes_TypeObject *to_minimal;
};

struct descriptor_type_meta {
  const idl_node_t *root;
  struct type_meta *admin;
  struct type_meta *stack;
};

static idl_retcode_t
push_type (struct descriptor_type_meta *dtm, const void *node)
{
  struct type_meta *tmp, *tm = calloc (1, sizeof (*tm));
  tm->node = node;
  tm->ti_minimal = calloc (1, sizeof (*tm->ti_minimal));
  tm->to_minimal = calloc (1, sizeof (*tm->to_minimal));
  tm->ti_complete = calloc (1, sizeof (*tm->ti_complete));
  tm->to_complete = calloc (1, sizeof (*tm->to_complete));

  // Add to admin
  // FIXME: check for duplicates?
  tmp = dtm->admin;
  if (tmp == NULL)
    dtm->admin = tm;
  else {
    while (tmp->admin_next)
      tmp = tmp->admin_next;
    tmp->admin_next = tm;
  }

  // Add to stack
  tmp = dtm->stack;
  dtm->stack = tm;
  tm->stack_prev = tmp;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
pop_type (struct descriptor_type_meta *dtm, const void *node)
{
  assert (dtm->stack);
  assert (dtm->stack->node == node);
  dtm->stack = dtm->stack->stack_prev;
  return IDL_RETCODE_OK;
}

static void
xcdr2_ser (
  const void *obj,
  const dds_topic_descriptor_t *desc,
  dds_ostream_t *os)
{
  // create sertype from descriptor
  struct ddsi_sertype_default sertype;
  memset (&sertype, 0, sizeof (sertype));
  sertype.type = (struct ddsi_sertype_default_desc) {
    .size = desc->m_size,
    .align = desc->m_align,
    .flagset = desc->m_flagset,
    .keys.nkeys = 0,
    .keys.keys = NULL,
    .ops.nops = dds_stream_countops (desc->m_ops, desc->m_nkeys, desc->m_keys),
    .ops.ops = (uint32_t *) desc->m_ops
  };

  // serialize as XCDR2 LE
  os->m_buffer = NULL;
  os->m_index = 0;
  os->m_size = 0;
  os->m_xcdr_version = CDR_ENC_VERSION_2;
  dds_stream_write_sampleLE ((dds_ostreamLE_t *) os, obj, &sertype);
}

static bool
is_fully_descriptive (const idl_type_spec_t *type_spec)
{
  return idl_is_string (type_spec) || idl_is_base_type (type_spec);
}

static void
get_fully_descriptive_typeid (DDS_XTypes_TypeIdentifier *ti, const idl_type_spec_t *type_spec)
{
  assert (ti);
  assert (is_fully_descriptive (type_spec));
  switch (idl_type (type_spec))
  {
    case IDL_BOOL: ti->_d = DDS_XTypes_TK_BOOLEAN; break;
    case IDL_UINT8: case IDL_OCTET: ti->_d = DDS_XTypes_TK_BYTE; break;
    case IDL_INT16: case IDL_SHORT: ti->_d = DDS_XTypes_TK_INT16; break;
    case IDL_INT32: case IDL_LONG: ti->_d = DDS_XTypes_TK_INT32; break;
    case IDL_INT64: case IDL_LLONG: ti->_d = DDS_XTypes_TK_INT64; break;
    case IDL_UINT16: case IDL_USHORT: ti->_d = DDS_XTypes_TK_UINT16; break;
    case IDL_UINT32: case IDL_ULONG: ti->_d = DDS_XTypes_TK_UINT32; break;
    case IDL_UINT64: case IDL_ULLONG: ti->_d = DDS_XTypes_TK_UINT64; break;
    case IDL_FLOAT: ti->_d = DDS_XTypes_TK_FLOAT32; break;
    case IDL_DOUBLE: ti->_d = DDS_XTypes_TK_FLOAT64; break;
    case IDL_LDOUBLE: ti->_d = DDS_XTypes_TK_FLOAT128; break;
    case IDL_CHAR: ti->_d = DDS_XTypes_TK_CHAR8; break;
    case IDL_STRING: ti->_d = DDS_XTypes_TK_STRING8; break;
    default:
      abort ();
  }
}

static void
get_type_hash (DDS_XTypes_EquivalenceHash *hash, const DDS_XTypes_TypeObject *to)
{
  dds_ostream_t os;
  xcdr2_ser (to, &DDS_XTypes_TypeObject_desc, &os);

  // get md5 of serialized cdr and store first 14 bytes in equivalence hash parameter
  char buf[16];
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &os.m_buffer, os.m_size);
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  memcpy (hash, &buf, 14);
  dds_ostream_fini (&os);
}

static idl_retcode_t
get_hashed_typeid (struct descriptor_type_meta *dtm, DDS_XTypes_TypeIdentifier *ti, const idl_type_spec_t *type_spec, bool complete)
{
  assert (ti);
  assert (!is_fully_descriptive (type_spec));
  struct type_meta *tm = dtm->admin;
  while (tm && tm->node != type_spec)
    tm = tm->admin_next;
  if (!tm)
    return -1;
  if (complete) {
    ti->_d = DDS_XTypes_EK_COMPLETE;
    get_type_hash (&ti->_u.equivalence_hash, tm->to_complete);
  } else {
    ti->_d = DDS_XTypes_EK_MINIMAL;
    get_type_hash (&ti->_u.equivalence_hash, tm->to_minimal);
  }
  return 0;
}

static void
get_namehash (DDS_XTypes_NameHash name_hash, const char *name)
{
  // FIXME: multi byte utf8 chars?
  char buf[16];
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) name, (uint32_t) strlen (name));
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  memcpy (name_hash, &buf, sizeof (*name_hash));
}

static idl_retcode_t
add_to_seq (dds_sequence_t *seq, const void *obj, size_t sz)
{
  seq->_buffer = realloc (seq->_buffer, (seq->_length + 1) * sz);
  if (seq->_buffer == NULL)
    return -1;
  seq->_length++;
  seq->_maximum++;
  memcpy (seq->_buffer + (seq->_length - 1) * sz, obj, sz);
  return 0;
}

static DDS_XTypes_StructTypeFlag
get_struct_flags(const idl_struct_t *_struct)
{
  DDS_XTypes_StructTypeFlag flags = 0u;
  if (_struct->extensibility == IDL_EXTENSIBILITY_MUTABLE)
    flags |= DDS_XTypes_IS_MUTABLE;
  else if (_struct->extensibility == IDL_EXTENSIBILITY_APPENDABLE)
    flags |= DDS_XTypes_IS_APPENDABLE;
  else {
    assert (_struct->extensibility == IDL_EXTENSIBILITY_FINAL);
    flags |= DDS_XTypes_IS_FINAL;
  }
  if (_struct->nested.value)
    flags |= DDS_XTypes_IS_NESTED;
  if (_struct->autoid == IDL_AUTOID_HASH)
    flags |= DDS_XTypes_IS_AUTOID_HASH;
  return flags;
}

static DDS_XTypes_StructMemberFlag
get_struct_member_flags(const idl_member_t *member)
{
  DDS_XTypes_StructMemberFlag flags = 0u;
  if (member->external)
    flags |= DDS_XTypes_IS_EXTERNAL;
  if (member->key)
    flags |= DDS_XTypes_IS_KEY;
  // FIXME: support for other flags
  return flags;
}

static DDS_XTypes_UnionTypeFlag
get_union_flags(const idl_union_t *_union)
{
  DDS_XTypes_UnionTypeFlag flags = 0u;
  if (_union->extensibility == IDL_EXTENSIBILITY_MUTABLE)
    flags |= DDS_XTypes_IS_MUTABLE;
  else if (_union->extensibility == IDL_EXTENSIBILITY_APPENDABLE)
    flags |= DDS_XTypes_IS_APPENDABLE;
  else {
    assert (_union->extensibility == IDL_EXTENSIBILITY_FINAL);
    flags |= DDS_XTypes_IS_FINAL;
  }
  if (_union->nested.value)
    flags |= DDS_XTypes_IS_NESTED;
  if (_union->autoid == IDL_AUTOID_HASH)
    flags |= DDS_XTypes_IS_AUTOID_HASH;
  return flags;
}

static DDS_XTypes_UnionDiscriminatorFlag
get_union_discriminator_flags(const idl_switch_type_spec_t *switch_type_spec)
{
  DDS_XTypes_UnionDiscriminatorFlag flags = 0u;
  if (switch_type_spec->key)
    flags |= DDS_XTypes_IS_EXTERNAL;
  // FIXME: support for other flags
  return flags;
}

static DDS_XTypes_UnionMemberFlag
get_union_case_flags(const idl_case_t *_case)
{
  DDS_XTypes_UnionMemberFlag flags = 0u;
  if (_case->external)
    flags |= DDS_XTypes_IS_EXTERNAL;
  // FIXME: support for other flags
  return flags;
}

static idl_retcode_t
get_type_spec_typeid(
  struct descriptor_type_meta *dtm,
  idl_type_spec_t *type_spec,
  DDS_XTypes_TypeIdentifier *ti_minimal,
  DDS_XTypes_TypeIdentifier *ti_complete)
{
  if (is_fully_descriptive (type_spec)) {
    get_fully_descriptive_typeid (ti_minimal, type_spec);
    get_fully_descriptive_typeid (ti_complete, type_spec);
  } else {
    if (get_hashed_typeid (dtm, ti_minimal, type_spec, false) < 0)
      return -1;
    if (get_hashed_typeid (dtm, ti_complete, type_spec, true) < 0)
      return -1;
  }
  return 0;
}

static idl_retcode_t
add_struct_member (struct descriptor_type_meta *dtm, DDS_XTypes_TypeObject *to_minimal, DDS_XTypes_TypeObject *to_complete, const void *node)
{
  assert (to_minimal->_u.minimal._d == DDS_XTypes_TK_STRUCTURE);
  assert (to_complete->_u.complete._d == DDS_XTypes_TK_STRUCTURE);
  assert (idl_is_member (idl_parent (node)));

  DDS_XTypes_MinimalStructMember m;
  memset (&m, 0, sizeof (m));
  DDS_XTypes_CompleteStructMember c;
  memset (&c, 0, sizeof (c));

  const idl_member_t *member = (const idl_member_t *) idl_parent (node);
  if (get_type_spec_typeid (dtm, idl_type_spec (node), &m.common.member_type_id, &c.common.member_type_id) < 0)
    return -1;
  m.common.member_id = c.common.member_id = member->id.value;
  m.common.member_flags = c.common.member_flags = get_struct_member_flags (member);
  get_namehash (m.detail.name_hash, idl_identifier (node));
  strcpy (c.detail.name, idl_identifier (node));

  add_to_seq ((dds_sequence_t *) &to_minimal->_u.minimal._u.struct_type.member_seq, &m, sizeof (m));
  add_to_seq ((dds_sequence_t *) &to_complete->_u.complete._u.struct_type.member_seq, &c, sizeof (c));

  return IDL_RETCODE_OK;
}

static idl_retcode_t
add_union_case(struct descriptor_type_meta *dtm, DDS_XTypes_TypeObject *to_minimal, DDS_XTypes_TypeObject *to_complete, const void *node)
{
  assert (to_minimal->_u.complete._d == DDS_XTypes_TK_UNION);
  assert (to_complete->_u.complete._d == DDS_XTypes_TK_UNION);
  assert (idl_is_case (idl_parent (node)));

  DDS_XTypes_MinimalUnionMember m;
  memset (&m, 0, sizeof (m));
  DDS_XTypes_CompleteUnionMember c;
  memset (&c, 0, sizeof (c));

  const idl_case_t *_case = (const idl_case_t *) idl_parent (node);
  if (get_type_spec_typeid (dtm, idl_type_spec (node), &m.common.type_id, &c.common.type_id) < 0)
    return -1;
  m.common.member_id = c.common.member_id = _case->id.value;
  m.common.member_flags = c.common.member_flags = get_union_case_flags (_case);
  get_namehash (m.detail.name_hash, idl_identifier (node));
  strcpy (c.detail.name, idl_identifier (node));

  // case labels
  idl_case_t *case_node = (idl_case_t *) idl_parent (node);
  const idl_case_label_t *cl;
  uint32_t cnt, n;
  for (cl = case_node->labels, cnt = 0; cl; cl = idl_next (cl))
    cnt++;
  m.common.label_seq._buffer = calloc (cnt, sizeof (*m.common.label_seq._buffer));
  m.common.label_seq._length = cnt;
  for (cl = case_node->labels, n = 0; cl; cl = idl_next (cl)) {
    idl_intval_t val = idl_intval (cl->const_expr);
    m.common.label_seq._buffer[n++] = (int32_t) val.value.llng;
  }

  add_to_seq ((dds_sequence_t *) &to_minimal->_u.minimal._u.union_type.member_seq, &m, sizeof (m));
  add_to_seq ((dds_sequence_t *) &to_complete->_u.complete._u.union_type.member_seq, &c, sizeof (c));

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_hashed_type(
  uint8_t type_kind,
  const void *node,
  bool revisit,
  struct descriptor_type_meta *dtm)
{
  assert (!is_fully_descriptive (idl_type_spec (node)));
  if (revisit) {
    get_type_hash (&dtm->stack->ti_minimal->_u.equivalence_hash, dtm->stack->to_minimal);
    get_type_hash (&dtm->stack->ti_complete->_u.equivalence_hash, dtm->stack->to_complete);
    pop_type (dtm, node);
  } else {
    push_type (dtm, node);
    dtm->stack->ti_minimal->_d = DDS_XTypes_EK_MINIMAL;
    dtm->stack->to_minimal->_d = DDS_XTypes_EK_MINIMAL;
    dtm->stack->to_minimal->_u.minimal._d = type_kind;
    dtm->stack->ti_complete->_d = DDS_XTypes_EK_COMPLETE;
    dtm->stack->to_complete->_d = DDS_XTypes_EK_COMPLETE;
    dtm->stack->to_complete->_u.complete._d = type_kind;
    return IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_struct(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct descriptor_type_meta * dtm = (struct descriptor_type_meta *) user_data;
  idl_retcode_t ret;

  (void) pstate;
  (void) path;
  if ((ret = emit_hashed_type (DDS_XTypes_TK_STRUCTURE, node, revisit, dtm)) < 0)
    return ret;
  if (!revisit)
    dtm->stack->to_minimal->_u.minimal._u.struct_type.struct_flags = get_struct_flags ((const idl_struct_t *) node);
  return ret;
}

static idl_retcode_t
emit_union(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct descriptor_type_meta * dtm = (struct descriptor_type_meta *) user_data;
  idl_retcode_t ret;

  (void) pstate;
  (void) path;
  if ((ret = emit_hashed_type (DDS_XTypes_TK_UNION, node, revisit, dtm)) < 0)
    return ret;
  if (!revisit)
    dtm->stack->to_minimal->_u.minimal._u.union_type.union_flags = get_union_flags ((const idl_union_t *) node);
  return ret;
}

static idl_retcode_t
emit_switch_type_spec(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void) pstate;
  (void) revisit;
  (void) path;

  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;
  struct type_meta *tm = dtm->stack;

  assert (tm->to_complete->_u.complete._d == DDS_XTypes_TK_UNION);
  assert (idl_is_union (idl_parent (node)));
  const idl_union_t *_union = (const idl_union_t *) idl_parent (node);

  DDS_XTypes_CommonDiscriminatorMember
    *m_cdm = &tm->to_minimal->_u.minimal._u.union_type.discriminator.common,
    *c_cdm = &tm->to_complete->_u.complete._u.union_type.discriminator.common;
  m_cdm->member_flags = c_cdm->member_flags = get_union_discriminator_flags (_union->switch_type_spec);
  if (get_type_spec_typeid (dtm, idl_type_spec (_union->switch_type_spec), &m_cdm->type_id, &c_cdm->type_id) < 0)
    return -1;

  // FIXME: builtin and custom annotations

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_alias (
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void) pstate;
  (void) path;
  return emit_hashed_type (DDS_XTypes_TK_ALIAS, node, revisit, (struct descriptor_type_meta *) user_data);
}

static idl_retcode_t
emit_declarator (
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;
  struct type_meta *tm = dtm->stack;
  assert(tm);

  const idl_type_spec_t *type_spec = idl_type_spec (node);
  if (idl_is_typedef (idl_parent (type_spec))) {
    return emit_alias(pstate, revisit, path, node, user_data);
  }

  if (revisit) {
    if (tm->to_minimal->_u.minimal._d == DDS_XTypes_TK_STRUCTURE) {
      assert (tm->to_complete->_u.complete._d == DDS_XTypes_TK_STRUCTURE);
      if (add_struct_member (dtm, tm->to_minimal, tm->to_complete, node) < 0)
        return -1;
    } else if (tm->to_minimal->_u.minimal._d == DDS_XTypes_TK_UNION) {
      assert (tm->to_complete->_u.complete._d == DDS_XTypes_TK_UNION);
      if (add_union_case (dtm, tm->to_minimal, tm->to_complete, node) < 0)
        return -1;
    } else {
      abort ();
    }
  } else {
    if (is_fully_descriptive (type_spec))
      return IDL_VISIT_REVISIT;
    else
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_enum (
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void)pstate;
  (void)path;
  return emit_hashed_type (DDS_XTypes_TK_ENUM, node, revisit, (struct descriptor_type_meta *) user_data);
}

static idl_retcode_t
emit_enumerator (
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void)pstate;
  (void)revisit;
  (void)path;

  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;
  struct type_meta *tm = dtm->stack;

  assert (idl_is_enum (idl_parent (node)));
  assert (tm->to_minimal->_u.minimal._d == DDS_XTypes_TK_ENUM && tm->to_complete->_u.complete._d == DDS_XTypes_TK_ENUM);

  DDS_XTypes_MinimalEnumeratedLiteral m;
  memset (&m, 0, sizeof (m));
  DDS_XTypes_CompleteEnumeratedLiteral c;
  memset (&c, 0, sizeof (c));

  const idl_enumerator_t *enumerator = (idl_enumerator_t *) node;
  assert (enumerator->value <= INT32_MAX);
  m.common.value = c.common.value = (int32_t) enumerator->value;
  get_namehash (m.detail.name_hash, idl_identifier (enumerator));
  strcpy (c.detail.name, idl_identifier (enumerator));

  add_to_seq ((dds_sequence_t *) &tm->to_minimal->_u.minimal._u.enumerated_type.literal_seq, &m, sizeof (m));
  add_to_seq ((dds_sequence_t *) &tm->to_complete->_u.complete._u.enumerated_type.literal_seq, &c, sizeof (c));

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_bitmask(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void) pstate;
  (void) path;
  return emit_hashed_type (DDS_XTypes_TK_BITMASK, node, revisit, (struct descriptor_type_meta *) user_data);
}

static idl_retcode_t
emit_bit_value (
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void) pstate;
  (void) revisit;
  (void) path;

  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;
  struct type_meta *tm = dtm->stack;

  assert (tm->to_complete->_u.complete._d == DDS_XTypes_TK_BITMASK);
  assert (idl_is_bitmask (idl_parent (node)));

  DDS_XTypes_MinimalBitflag m;
  memset (&m, 0, sizeof (m));
  DDS_XTypes_CompleteBitflag c;
  memset (&c, 0, sizeof (c));

  const idl_bit_value_t *bit_value = (idl_bit_value_t *) node;
  m.common.position = c.common.position = bit_value->position;
  get_namehash (m.detail.name_hash, idl_identifier (bit_value));
  strcpy (c.detail.name, idl_identifier (bit_value));

  add_to_seq ((dds_sequence_t *) &tm->to_minimal->_u.minimal._u.bitmask_type.flag_seq, &m, sizeof (m));
  add_to_seq ((dds_sequence_t *) &tm->to_complete->_u.complete._u.bitmask_type.flag_seq, &c, sizeof (c));

  return IDL_RETCODE_OK;
}

static idl_retcode_t
print_ser_data(FILE *fp, const char *kind, const char *type, unsigned char *data, uint32_t sz)
{
  char *sep = ", ", *lsep = "\n  ", *fmt;

  fmt = "#define %1$s_%2$s (unsigned char []){ ";
  if (idl_fprintf(fp, fmt, kind, type) < 0)
    return -1;

  fmt = "%1$s%2$s0x%3$02"PRIx8;
  for (uint32_t n = 0; n < sz; n++)
    if (idl_fprintf(fp, fmt, n > 0 ? sep : "", !(n % 16) ? lsep : "", data[n]) < 0)
      return -1;

  fmt = "\n}\n"
        "#define %1$s_SZ_%2$s %3$"PRIu32"u\n";
  if (idl_fprintf(fp, fmt, kind, type, sz) < 0)
    return -1;

  return 0;
}

static void
get_typeid_with_size (
    DDS_XTypes_TypeIdentfierWithSize *typeid_with_size,
    DDS_XTypes_TypeIdentifier *ti,
    DDS_XTypes_TypeObject *to)
{
  memcpy (&typeid_with_size->type_id, ti, sizeof (typeid_with_size->type_id));
  if (to) {
    dds_ostream_t os;
    xcdr2_ser (to, &DDS_XTypes_TypeObject_desc, &os);
    typeid_with_size->typeobject_serialized_size = os.m_index;
    dds_ostream_fini (&os);
  }
}

idl_retcode_t
print_type_meta_ser (
  FILE *fp,
  const idl_pstate_t *pstate,
  const idl_node_t *node)
{
  char *type_name;
  idl_retcode_t ret;
  struct descriptor_type_meta dtm;
  idl_visitor_t visitor;

  memset (&dtm, 0, sizeof (dtm));
  memset (&visitor, 0, sizeof (visitor));

  visitor.visit = IDL_STRUCT | IDL_UNION | IDL_DECLARATOR | IDL_BITMASK | IDL_BIT_VALUE | IDL_ENUM | IDL_ENUMERATOR | IDL_SWITCH_TYPE_SPEC;
  visitor.accept[IDL_ACCEPT_STRUCT] = &emit_struct;
  visitor.accept[IDL_ACCEPT_UNION] = &emit_union;
  visitor.accept[IDL_ACCEPT_SWITCH_TYPE_SPEC] = &emit_switch_type_spec;
  visitor.accept[IDL_ACCEPT_DECLARATOR] = &emit_declarator;
  visitor.accept[IDL_ACCEPT_BITMASK] = &emit_bitmask;
  visitor.accept[IDL_ACCEPT_BIT_VALUE] = &emit_bit_value;
  visitor.accept[IDL_ACCEPT_ENUM] = &emit_enum;
  visitor.accept[IDL_ACCEPT_ENUMERATOR] = &emit_enumerator;

  /* must be invoked for topics only, so structs and unions */
  assert (idl_is_struct (node) || idl_is_union (node));

  dtm.root = node;
  if ((ret = idl_visit (pstate, node, &visitor, &dtm)))
    goto err_emit;

  if (IDL_PRINTA(&type_name, print_type, node) < 0)
    return -1;

  struct DDS_XTypes_TypeInformation type_information;
  memset (&type_information, 0, sizeof (type_information));

  /* typeidwithsize for top-level type */
  get_typeid_with_size (&type_information.minimal.typeid_with_size, dtm.admin->ti_minimal, dtm.admin->to_minimal);
  get_typeid_with_size (&type_information.complete.typeid_with_size, dtm.admin->ti_complete, dtm.admin->to_complete);

  /* dependent type ids, skip first (top-level) */
  for (struct type_meta *tm = dtm.admin->admin_next; tm; tm = tm->admin_next) {
    DDS_XTypes_TypeIdentfierWithSize tidws;

    get_typeid_with_size (&tidws, tm->ti_minimal, tm->to_minimal);
    add_to_seq ((dds_sequence_t *) &type_information.minimal.dependent_typeids, &tidws, sizeof (tidws));
    type_information.minimal.dependent_typeid_count++;

    get_typeid_with_size (&tidws, tm->ti_complete, tm->to_complete);
    add_to_seq ((dds_sequence_t *) &type_information.complete.dependent_typeids, &tidws, sizeof (tidws));
    type_information.complete.dependent_typeid_count++;
  }

  {
    dds_ostream_t os;
    xcdr2_ser (&type_information, &DDS_XTypes_TypeInformation_desc, &os);
    print_ser_data (fp, "TYPE_INFO_CDR", type_name, os.m_buffer, os.m_index);
    dds_ostream_fini (&os);
  }

  /* type id/obj seq for min and complete */
  DDS_XTypes_TypeMapping mapping;
  memset (&mapping, 0, sizeof (mapping));
  for (struct type_meta *tm = dtm.admin->admin_next; tm; tm = tm->admin_next) {
    DDS_XTypes_TypeIdentifierTypeObjectPair mp, cp;
    memcpy (&mp.type_identifier, tm->ti_minimal, sizeof (mp.type_identifier));
    memcpy (&mp.type_object, tm->to_minimal, sizeof (mp.type_object));
    memcpy (&cp.type_identifier, tm->ti_complete, sizeof (cp.type_identifier));
    memcpy (&cp.type_object, tm->to_complete, sizeof (cp.type_object));
    add_to_seq ((dds_sequence_t *) &mapping.identifier_object_pair_minimal, &mp, sizeof (mp));
    add_to_seq ((dds_sequence_t *) &mapping.identifier_object_pair_complete, &cp, sizeof (cp));
  }

  {
    dds_ostream_t os;
    xcdr2_ser (&mapping, &DDS_XTypes_TypeMapping_desc, &os);
    print_ser_data (fp, "TYPE_MAP_CDR", type_name, os.m_buffer, os.m_index);
    dds_ostream_fini (&os);
  }

  return ret;

err_emit:
  return ret;
}

