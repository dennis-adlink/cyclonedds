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
#ifndef DDSI_TYPE_XTYPES_H
#define DDSI_TYPE_XTYPES_H

#include "dds/features.h"

#ifdef DDS_HAS_TYPE_DISCOVERY

#include <stdint.h>
#include "dds/ddsi/ddsi_type_identifier.h"
#include "dds/ddsi/ddsi_type_object.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define XT_FLAG_EXTENSIBILITY_MASK  0x4

struct xt_type;
struct ddsi_domaingv;

struct xt_applied_type_annotations {
  // FIXME
  // struct AppliedBuiltinTypeAnnotations ann_builtin;
  // struct AppliedAnnotationSeq ann_custom;
};

struct xt_applied_member_annotations {
  // FIXME
  // struct AppliedBuiltinMemberAnnotations ann_builtin;
  // struct AppliedAnnotationSeq ann_custom;
};

struct xt_type_detail {
  QualifiedTypeName type_name;
  struct xt_applied_type_annotations annotations;
};

struct xt_member_detail {
  MemberName name;
  struct xt_applied_member_annotations annotations;
};

struct xt_string {
  LBound_t bound;
};

struct xt_collection_common {
  CollectionTypeFlag flags;
  EquivalenceKind ek;
  struct xt_type_detail detail;
  struct xt_type *element_type;
  CollectionElementFlag element_flags;
  struct xt_applied_member_annotations element_annotations;
};

struct xt_seq {
  struct xt_collection_common c;
  LBound_t bound;
};

struct xt_array {
  struct xt_collection_common c;
  struct LBoundSeq bounds;
};

struct xt_map {
  struct xt_collection_common c;
  LBound_t bound;
  CollectionElementFlag key_flags;
  struct xt_type *key_type;
  struct xt_applied_member_annotations key_annotations;
};

struct xt_alias {
  struct xt_type *related_type;
  struct xt_type_detail detail;
};

struct xt_annotation_member {
  MemberName name;
  struct xt_type *member_type;
  struct AnnotationParameterValue default_value;
};
struct xt_annotation_parameter_seq {
  uint32_t length;
  struct xt_annotation_parameter *seq;
};
struct xt_annotation {
  QualifiedTypeName annotation_name;
  struct xt_annotation_parameter_seq *members;
};

struct xt_struct_member {
  MemberId id;
  StructMemberFlag flags;
  struct xt_type *type;
  struct NameHash name_hash;
  struct xt_member_detail detail;
};
struct xt_struct_member_seq {
  uint32_t length;
  struct xt_struct_member *seq;
};
struct xt_struct {
  StructTypeFlag flags;
  struct xt_type *base_type;
  struct xt_struct_member_seq members;
  struct xt_type_detail detail;
};

struct xt_union_member {
  MemberId id;
  UnionMemberFlag flags;
  struct xt_type *type;
  struct UnionCaseLabelSeq label_seq;
  struct NameHash name_hash;
  struct xt_member_detail detail;
};
struct xt_union_member_seq {
  uint32_t length;
  struct xt_union_member *seq;
};
struct xt_union {
  UnionTypeFlag flags;
  struct xt_type *disc_type;
  UnionDiscriminatorFlag disc_flags;
  struct xt_applied_type_annotations disc_annotations;
  struct xt_union_member_seq members;
  struct xt_type_detail detail;
};

struct xt_bitfield {
  uint16_t position;
  uint8_t bitcount;
  TypeKind holder_type; // Must be primitive integer type
  struct NameHash name_hash;
  struct xt_member_detail detail;
};
struct xt_bitfield_seq {
  uint32_t length;
  struct xt_bitfield *seq;
};
struct xt_bitset {
  struct xt_bitfield_seq fields;
  struct xt_type_detail detail;
};

struct xt_enum_literal {
  int32_t value;
  EnumeratedLiteralFlag flags;
  struct NameHash name_hash;
  struct xt_member_detail detail;
};
struct xt_enum_literal_seq {
  uint32_t length;
  struct xt_enum_literal *seq;
};
struct xt_enum {
  EnumTypeFlag flags;
  BitBound bit_bound;
  struct xt_enum_literal_seq literals;
  struct xt_type_detail detail;
};

struct xt_bitflag {
  uint16_t position;
  struct NameHash name_hash;
  struct xt_member_detail detail;
};
struct xt_bitflag_seq {
  uint32_t length;
  struct xt_bitflag *seq;
};
struct xt_bitmask {
  BitmaskTypeFlag flags;
  BitBound bit_bound;
  struct xt_bitflag_seq bitflags;
  struct xt_type_detail detail;
};

struct xt_type
{
  bool is_plain_collection;
  struct EquivalenceHash minimal_hash;
  struct EquivalenceHash complete_hash;
  struct StronglyConnectedComponentId sc_component_id;
  unsigned has_minimal_id : 1;
  unsigned has_minimal_obj : 1;
  unsigned has_complete_id : 1;
  unsigned has_complete_obj : 1;

  uint8_t _d;
  union {
    // case TK_NONE:
    // case TK_BOOLEAN:
    // case TK_BYTE:
    // case TK_INT16:
    // case TK_INT32:
    // case TK_INT64:
    // case TK_UINT8:
    // case TK_UINT16:
    // case TK_UINT32:
    // case TK_UINT64:
    // case TK_FLOAT32:
    // case TK_FLOAT64:
    // case TK_FLOAT128:
    // case TK_CHAR8:
    // case TK_CHAR16:
    //   <empty for primitive types>
    // case TK_STRING8:
    struct xt_string str8;
    // case TK_STRING16:
    struct xt_string str16;
    // case TK_SEQUENCE:
    struct xt_seq seq;
    // case TK_ARRAY:
    struct xt_array array;
    // case TK_MAP:
    struct xt_map map;
    // case TK_ALIAS:
    struct xt_alias alias;
    // case TK_ANNOTATION:
    struct xt_annotation annotation;
    // case TK_STRUCTURE:
    struct xt_struct structure;
    // case TK_UNION:
    struct xt_union union_type;
    // case TK_BITSET:
    struct xt_bitset bitset;
    // case TK_ENUM:
    struct xt_enum enum_type;
    // case TK_BITMASK:
    struct xt_bitmask bitmask;
  } _u;
};

struct xt_type *ddsi_xt_type_init (const struct TypeIdentifier *ti, const struct TypeObject *to);
void ddsi_xt_type_add (struct xt_type *xt, const struct TypeIdentifier *ti, const struct TypeObject *to);
void ddsi_xt_type_fini (struct xt_type *xt);
bool ddsi_xt_is_assignable_from (const struct ddsi_domaingv *gv, const struct xt_type *t1, const struct xt_type *t2);
bool ddsi_xt_has_complete_typeid (const struct xt_type *xt);

#if defined (__cplusplus)
}
#endif
#endif /* DDS_HAS_TYPE_DISCOVERY */
#endif /* DDSI_TYPE_XTYPES_H */
