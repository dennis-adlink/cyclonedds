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
#ifndef DDSI_TYPE_OBJECT_H
#define DDSI_TYPE_OBJECT_H

#include "dds/features.h"

#ifdef DDS_HAS_TYPE_DISCOVERY

#include <stdbool.h>
#include <stdint.h>
#include "dds/ddsi/ddsi_type_identifier.h"

#if defined (__cplusplus)
extern "C" {
#endif

// // --- Annotation usage: -----------------------------------------------

typedef uint32_t MemberId;
#define ANNOTATION_STR_VALUE_MAX_LEN 128
#define ANNOTATION_uint8_tSEC_VALUE_MAX_LEN 128

#define MEMBER_NAME_MAX_LENGTH 256
typedef char MemberName[MEMBER_NAME_MAX_LENGTH];

#define TYPE_NAME_MAX_LENGTH 256
typedef char QualifiedTypeName[TYPE_NAME_MAX_LENGTH];

// @extensibility(MUTABLE) @nested
// struct ExtendedAnnotationParameterValue {
//     // Empty. Available for future extension
// };
struct ExtendedAnnotationParameterValue {
};

// /* Literal value of an annotation member: either the default value in its
//   * definition or the value applied in its usage.
//   */
// @extensibility(FINAL) @nested
// union AnnotationParameterValue switch (uint8_t) {
//     case TK_BOOLEAN:
//         boolean             boolean_value;
//     case TK_BYTE:
//         uint8_t               byte_value;
//     case TK_INT16:
//         short               int16_value;
//     case TK_UINT16:
//         unsigned short      uint_16_value;
//     case TK_INT32:
//         long                int32_value;
//     case TK_UINT32:
//         unsigned long       uint32_value;
//     case TK_INT64:
//         long long           int64_value;
//     case TK_UINT64:
//         unsigned long long  uint64_value;
//     case TK_FLOAT32:
//         float               float32_value;
//     case TK_FLOAT64:
//         double              float64_value;
//     case TK_FLOAT128:
//         long double         float128_value;
//     case TK_CHAR8:
//         char                char_value;
//     case TK_CHAR16:
//         wchar               wchar_value;
//     case TK_ENUM:
//         long                enumerated_value;
//     case TK_STRING8:
//         string<ANNOTATION_STR_VALUE_MAX_LEN>  string8_value;
//     case TK_STRING16:
//         wstring<ANNOTATION_STR_VALUE_MAX_LEN> string16_value;
//     default:
//         ExtendedAnnotationParameterValue      extended_value;
// };
struct AnnotationParameterValue {
  uint8_t _d;
  union {
    bool boolean_value;
    uint8_t byte_value;
    int16_t int16_value;
    uint16_t uint_16_value;
    int32_t int32_value;
    uint32_t uint32_value;
    int64_t int64_value;
    uint64_t uint64_value;
    float float32_value;
    double float64_value;
    // long double float128_value;
    char char_value;
    // wchar_t wchar_value;
    int32_t enumerated_value;
    char * string8_value;
    // wchar_t * string16_value;
    struct ExtendedAnnotationParameterValue extended_value;
  } _u;
};

// // The application of an annotation to some type or type member
// @extensibility(APPENDABLE) @nested
// struct AppliedAnnotationParameter {
//     NameHash                  paramname_hash;
//     AnnotationParameterValue  value;
// };
// // Sorted by AppliedAnnotationParameter.paramname_hash
// typedef sequence<AppliedAnnotationParameter> AppliedAnnotationParameterSeq;
struct AppliedAnnotationParameter {
  struct NameHash paramname_hash;
  struct AnnotationParameterValue value;
};
struct AppliedAnnotationParameterSeq {
  uint32_t length;
  struct AppliedAnnotationParameter * seq;
};

// @extensibility(APPENDABLE) @nested
// struct AppliedAnnotation {
//     TypeIdentifier                     annotation_typeid;
//     @optional AppliedAnnotationParameterSeq   param_seq;
// };
// // Sorted by AppliedAnnotation.annotation_typeid
// typedef sequence<AppliedAnnotation> AppliedAnnotationSeq;
struct AppliedAnnotation {
  struct TypeIdentifier annotation_typeid;
  struct AppliedAnnotationParameterSeq param_seq;
};
struct AppliedAnnotationSeq {
  uint32_t length;
  struct AppliedAnnotation * seq;
};

// // @verbatim(placement="<placement>", language="<lang>", text="<text>")
// @extensibility(FINAL) @nested
// struct AppliedVerbatimAnnotation {
//     string<32> placement;
//     string<32> language;
//     string     text;
// };
struct AppliedVerbatimAnnotation {
  char placement[32];
  char language[32];
  char * text;
};


// // --- Aggregate types: ------------------------------------------------
// @extensibility(APPENDABLE) @nested
// struct AppliedBuiltinMemberAnnotations {
//     @optional string                  unit; // @unit("<unit>")
//     @optional AnnotationParameterValue min; // @min , @range
//     @optional AnnotationParameterValue max; // @max , @range
//     @optional string               hash_id; // @hash_id("<membername>")
// };
struct AppliedBuiltinMemberAnnotations {
  char * unit; // @unit("<unit>")
  struct AnnotationParameterValue min; // @min , @range
  struct AnnotationParameterValue max; // @max , @range
  char * hash_id; // @hash_id("<membername>")
};

// @extensibility(FINAL) @nested
// struct CommonStructMember {
//     MemberId                                   member_id;
//     StructMemberFlag                           member_flags;
//     TypeIdentifier                             member_type_id;
// };
struct CommonStructMember {
  MemberId member_id;
  StructMemberFlag member_flags;
  struct TypeIdentifier member_type_id;
};

// // COMPLETE Details for a member of an aggregate type
// @extensibility(FINAL) @nested
// struct CompleteMemberDetail {
//     MemberName                                 name;
//     @optional AppliedBuiltinMemberAnnotations  ann_builtin;
//     @optional AppliedAnnotationSeq             ann_custom;
// };
struct CompleteMemberDetail {
  MemberName name;
  struct AppliedBuiltinMemberAnnotations ann_builtin;
  struct AppliedAnnotationSeq ann_custom;
};

// // MINIMAL Details for a member of an aggregate type
// @extensibility(FINAL) @nested
// struct MinimalMemberDetail {
//     NameHash                                  name_hash;
// };
struct MinimalMemberDetail {
  struct NameHash name_hash;
};

// // Member of an aggregate type
// @extensibility(APPENDABLE) @nested
// struct CompleteStructMember {
//     CommonStructMember                         common;
//     CompleteMemberDetail                       detail;
// };
// // Ordered by the member_index
// typedef sequence<CompleteStructMember> CompleteStructMemberSeq;
struct CompleteStructMember {
  struct CommonStructMember common;
  struct CompleteMemberDetail detail;
};
struct CompleteStructMemberSeq {
  uint32_t length;
  struct CompleteStructMember * seq;
};

// // Member of an aggregate type
// @extensibility(APPENDABLE) @nested
// struct MinimalStructMember {
//     CommonStructMember                         common;
//     MinimalMemberDetail                        detail;
// };
// // Ordered by common.member_id
// typedef sequence<MinimalStructMember> MinimalStructMemberSeq;
struct MinimalStructMember {
  struct CommonStructMember common;
  struct MinimalMemberDetail detail;
};
struct MinimalStructMemberSeq {
  uint32_t length;
  struct MinimalStructMember * seq;
};

// @extensibility(APPENDABLE) @nested
// struct AppliedBuiltinTypeAnnotations {
//     @optional AppliedVerbatimAnnotation verbatim;  // @verbatim(...)
// };
struct AppliedBuiltinTypeAnnotations {
  struct AppliedVerbatimAnnotation verbatim;  // @verbatim(...)
};

// @extensibility(FINAL) @nested
// struct MinimalTypeDetail {
//     // Empty. Available for future extension
// };
struct MinimalTypeDetail {
};

// @extensibility(FINAL) @nested
// struct CompleteTypeDetail {
//       @optional AppliedBuiltinTypeAnnotations  ann_builtin;
//       @optional AppliedAnnotationSeq           ann_custom;
//       QualifiedTypeName                        type_name;
// };
struct CompleteTypeDetail {
  struct AppliedBuiltinTypeAnnotations ann_builtin;
  struct AppliedAnnotationSeq ann_custom;
  QualifiedTypeName type_name;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteStructHeader {
//     TypeIdentifier                           base_type;
//     CompleteTypeDetail                       detail;
// };
struct CompleteStructHeader {
  struct TypeIdentifier base_type;
  struct CompleteTypeDetail detail;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalStructHeader {
//     TypeIdentifier                           base_type;
//     MinimalTypeDetail                        detail;
// };
struct MinimalStructHeader {
  struct TypeIdentifier base_type;
  struct MinimalTypeDetail detail;
};

// @extensibility(FINAL) @nested
// struct CompleteStructType {
//     StructTypeFlag             struct_flags;
//     CompleteStructHeader       header;
//     CompleteStructMemberSeq    member_seq;
// };
struct CompleteStructType {
  StructTypeFlag struct_flags;
  struct CompleteStructHeader header;
  struct CompleteStructMemberSeq member_seq;
};

// @extensibility(FINAL) @nested
// struct MinimalStructType {
//     StructTypeFlag             struct_flags;
//     MinimalStructHeader        header;
//     MinimalStructMemberSeq     member_seq;
// };
struct MinimalStructType {
  StructTypeFlag struct_flags;
  struct MinimalStructHeader header;
  struct MinimalStructMemberSeq member_seq;
};

// // --- Union: ----------------------------------------------------------

// // Case labels that apply to a member of a union type
// // Ordered by their values
// typedef sequence<long> UnionCaseLabelSeq;
struct UnionCaseLabelSeq {
  uint32_t length;
  int32_t * seq;
};

// @extensibility(FINAL) @nested
// struct CommonUnionMember {
//     MemberId                    member_id;
//     UnionMemberFlag             member_flags;
//     TypeIdentifier              type_id;
//     UnionCaseLabelSeq           label_seq;
// };
struct CommonUnionMember {
  MemberId member_id;
  UnionMemberFlag member_flags;
  struct TypeIdentifier type_id;
  struct UnionCaseLabelSeq label_seq;
};

// // Member of a union type
// @extensibility(APPENDABLE) @nested
// struct CompleteUnionMember {
//     CommonUnionMember      common;
//     CompleteMemberDetail   detail;
// };
// // Ordered by member_index
// typedef sequence<CompleteUnionMember> CompleteUnionMemberSeq;
struct CompleteUnionMember {
  struct CommonUnionMember common;
  struct CompleteMemberDetail detail;
};
struct CompleteUnionMemberSeq {
  uint32_t length;
  struct CompleteUnionMember * seq;
};

// // Member of a union type
// @extensibility(APPENDABLE) @nested
// struct MinimalUnionMember {
//     CommonUnionMember   common;
//     MinimalMemberDetail detail;
// };
// // Ordered by MinimalUnionMember.common.member_id
// typedef sequence<MinimalUnionMember> MinimalUnionMemberSeq;
struct MinimalUnionMember {
  struct CommonUnionMember common;
  struct MinimalMemberDetail detail;
};
struct MinimalUnionMemberSeq {
  uint32_t length;
  struct MinimalUnionMember * seq;
};

// @extensibility(FINAL) @nested
// struct CommonDiscriminatorMember {
//     UnionDiscriminatorFlag       member_flags;
//     TypeIdentifier               type_id;
// };
struct CommonDiscriminatorMember {
  UnionDiscriminatorFlag member_flags;
  struct TypeIdentifier type_id;
};

// // Member of a union type
// @extensibility(APPENDABLE) @nested
// struct CompleteDiscriminatorMember {
//     CommonDiscriminatorMember                common;
//     @optional AppliedBuiltinTypeAnnotations  ann_builtin;
//     @optional AppliedAnnotationSeq           ann_custom;
// };
struct CompleteDiscriminatorMember {
  struct CommonDiscriminatorMember common;
  struct AppliedBuiltinTypeAnnotations ann_builtin;
  struct AppliedAnnotationSeq ann_custom;
};

// // Member of a union type
// @extensibility(APPENDABLE) @nested
// struct MinimalDiscriminatorMember {
//     CommonDiscriminatorMember   common;
// };
struct MinimalDiscriminatorMember {
  struct CommonDiscriminatorMember common;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteUnionHeader {
//     CompleteTypeDetail          detail;
// };
struct CompleteUnionHeader {
  struct CompleteTypeDetail detail;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalUnionHeader {
//     MinimalTypeDetail           detail;
// };
struct MinimalUnionHeader {
  struct MinimalTypeDetail detail;
};

// @extensibility(FINAL) @nested
// struct CompleteUnionType {
//     UnionTypeFlag                union_flags;
//     CompleteUnionHeader          header;
//     CompleteDiscriminatorMember  discriminator;
//     CompleteUnionMemberSeq       member_seq;
// };
struct CompleteUnionType {
  UnionTypeFlag union_flags;
  struct CompleteUnionHeader header;
  struct CompleteDiscriminatorMember discriminator;
  struct CompleteUnionMemberSeq member_seq;
};

// @extensibility(FINAL) @nested
// struct MinimalUnionType {
//     UnionTypeFlag                union_flags;
//     MinimalUnionHeader           header;
//     MinimalDiscriminatorMember   discriminator;
//     MinimalUnionMemberSeq        member_seq;
// };
struct MinimalUnionType {
  UnionTypeFlag union_flags;
  struct MinimalUnionHeader header;
  struct MinimalDiscriminatorMember discriminator;
  struct MinimalUnionMemberSeq member_seq;
};

// // --- Annotation: ----------------------------------------------------
// @extensibility(FINAL) @nested
// struct CommonAnnotationParameter {
//     AnnotationParameterFlag      member_flags;
//     TypeIdentifier               member_type_id;
// };
struct CommonAnnotationParameter {
  AnnotationParameterFlag member_flags;
  struct TypeIdentifier member_type_id;
};

// // Member of an annotation type
// @extensibility(APPENDABLE) @nested
// struct CompleteAnnotationParameter {
//     CommonAnnotationParameter  common;
//     MemberName                 name;
//     AnnotationParameterValue   default_value;
// };
// // Ordered by CompleteAnnotationParameter.name
// typedef
// sequence<CompleteAnnotationParameter> CompleteAnnotationParameterSeq;
struct CompleteAnnotationParameter {
  struct CommonAnnotationParameter common;
  MemberName name;
  struct AnnotationParameterValue default_value;
};
struct CompleteAnnotationParameterSeq {
  uint32_t length;
  struct CompleteAnnotationParameter * seq;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalAnnotationParameter {
//     CommonAnnotationParameter  common;
//     NameHash                   name_hash;
//     AnnotationParameterValue   default_value;
// };
// // Ordered by MinimalAnnotationParameter.name_hash
// typedef
// sequence<MinimalAnnotationParameter> MinimalAnnotationParameterSeq;
struct MinimalAnnotationParameter {
  struct CommonAnnotationParameter common;
  struct NameHash name_hash;
  struct AnnotationParameterValue default_value;
};
struct MinimalAnnotationParameterSeq {
  uint32_t length;
  struct MinimalAnnotationParameter * seq;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteAnnotationHeader {
//     QualifiedTypeName         annotation_name;
// };
struct CompleteAnnotationHeader {
  QualifiedTypeName annotation_name;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalAnnotationHeader {
//     // Empty. Available for future extension
// };
struct MinimalAnnotationHeader {
};

// @extensibility(FINAL) @nested
// struct CompleteAnnotationType {
//     AnnotationTypeFlag             annotation_flag;
//     CompleteAnnotationHeader       header;
//     CompleteAnnotationParameterSeq member_seq;
// };
struct CompleteAnnotationType {
  AnnotationTypeFlag annotation_flag;
  struct CompleteAnnotationHeader header;
  struct CompleteAnnotationParameterSeq member_seq;
};

// @extensibility(FINAL) @nested
// struct MinimalAnnotationType {
//     AnnotationTypeFlag             annotation_flag;
//     MinimalAnnotationHeader        header;
//     MinimalAnnotationParameterSeq  member_seq;
// };
struct MinimalAnnotationType {
  AnnotationTypeFlag annotation_flag;
  struct MinimalAnnotationHeader header;
  struct MinimalAnnotationParameterSeq member_seq;
};

// // --- Alias: ----------------------------------------------------------

// @extensibility(FINAL) @nested
// struct CommonAliasBody {
//     AliasMemberFlag       related_flags;
//     TypeIdentifier        related_type;
// };
struct CommonAliasBody {
  AliasMemberFlag related_flags;
  struct TypeIdentifier related_type;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteAliasBody {
//     CommonAliasBody       common;
//     @optional AppliedBuiltinMemberAnnotations  ann_builtin;
//     @optional AppliedAnnotationSeq             ann_custom;
// };
struct CompleteAliasBody {
  struct CommonAliasBody common;
  struct AppliedBuiltinMemberAnnotations ann_builtin;
  struct AppliedAnnotationSeq ann_custom;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalAliasBody {
//     CommonAliasBody       common;
// };
struct MinimalAliasBody {
  struct CommonAliasBody common;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteAliasHeader {
//     CompleteTypeDetail    detail;
// };
struct CompleteAliasHeader {
  struct CompleteTypeDetail detail;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalAliasHeader {
//     // Empty. Available for future extension
// };
struct MinimalAliasHeader {
};

// @extensibility(FINAL) @nested
// struct CompleteAliasType {
//     AliasTypeFlag         alias_flags;
//     CompleteAliasHeader   header;
//     CompleteAliasBody     body;
// };
struct CompleteAliasType {
  AliasTypeFlag alias_flags;
  struct CompleteAliasHeader   header;
  struct CompleteAliasBody     body;
};

// @extensibility(FINAL) @nested
// struct MinimalAliasType {
//     AliasTypeFlag         alias_flags;
//     MinimalAliasHeader    header;
//     MinimalAliasBody      body;
// };
struct MinimalAliasType {
  AliasTypeFlag alias_flags;
  struct MinimalAliasHeader header;
  struct MinimalAliasBody body;
};

// // --- Collections: ----------------------------------------------------
// @extensibility(FINAL) @nested
// struct CompleteElementDetail {
//     @optional AppliedBuiltinMemberAnnotations  ann_builtin;
//     @optional AppliedAnnotationSeq             ann_custom;
// };
struct CompleteElementDetail {
  struct AppliedBuiltinMemberAnnotations ann_builtin;
  struct AppliedAnnotationSeq ann_custom;
};

// @extensibility(FINAL) @nested
// struct CommonCollectionElement {
//     CollectionElementFlag     element_flags;
//     TypeIdentifier            type;
// };
struct CommonCollectionElement {
  CollectionElementFlag element_flags;
  struct TypeIdentifier type;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteCollectionElement {
//     CommonCollectionElement   common;
//     CompleteElementDetail     detail;
// };
struct CompleteCollectionElement {
  struct CommonCollectionElement common;
  struct CompleteElementDetail detail;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalCollectionElement {
//     CommonCollectionElement   common;
// };
struct MinimalCollectionElement {
  struct CommonCollectionElement common;
};

// @extensibility(FINAL) @nested
// struct CommonCollectionHeader {
//     LBound                    bound;
// };
struct CommonCollectionHeader {
  LBound_t bound;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteCollectionHeader {
//     CommonCollectionHeader        common;
//     @optional CompleteTypeDetail  detail; // not present for anonymous
// };
struct CompleteCollectionHeader {
  struct CommonCollectionHeader common;
  struct CompleteTypeDetail detail; // not present for anonymous
};

// @extensibility(APPENDABLE) @nested
// struct MinimalCollectionHeader {
//     CommonCollectionHeader        common;
// };
struct MinimalCollectionHeader {
  struct CommonCollectionHeader common;
};

// // --- Sequence: ------------------------------------------------------
// @extensibility(FINAL) @nested
// struct CompleteSequenceType {
//     CollectionTypeFlag         collection_flag;
//     CompleteCollectionHeader   header;
//     CompleteCollectionElement  element;
// };
struct CompleteSequenceType {
  CollectionTypeFlag collection_flag;
  struct CompleteCollectionHeader header;
  struct CompleteCollectionElement element;
};

// @extensibility(FINAL) @nested
// struct MinimalSequenceType {
//     CollectionTypeFlag         collection_flag;
//     MinimalCollectionHeader    header;
//     MinimalCollectionElement   element;
// };
struct MinimalSequenceType {
  CollectionTypeFlag collection_flag;
  struct MinimalCollectionHeader header;
  struct MinimalCollectionElement element;
};

// // --- Array: ------------------------------------------------------
// @extensibility(FINAL) @nested
// struct CommonArrayHeader {
//     LBoundSeq           bound_seq;
// };
struct CommonArrayHeader {
  struct LBoundSeq bound_seq;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteArrayHeader {
//     CommonArrayHeader   common;
//     CompleteTypeDetail  detail;
// };
struct CompleteArrayHeader {
  struct CommonArrayHeader common;
  struct CompleteTypeDetail detail;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalArrayHeader {
//     CommonArrayHeader   common;
// };
struct MinimalArrayHeader {
  struct CommonArrayHeader common;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteArrayType  {
//     CollectionTypeFlag          collection_flag;
//     CompleteArrayHeader         header;
//     CompleteCollectionElement   element;
// };
struct CompleteArrayType  {
  CollectionTypeFlag collection_flag;
  struct CompleteArrayHeader header;
  struct CompleteCollectionElement element;
};

// @extensibility(FINAL) @nested
// struct MinimalArrayType  {
//     CollectionTypeFlag         collection_flag;
//     MinimalArrayHeader         header;
//     MinimalCollectionElement   element;
// };
struct MinimalArrayType  {
  CollectionTypeFlag collection_flag;
  struct MinimalArrayHeader header;
  struct MinimalCollectionElement element;
};

// // --- Map: ------------------------------------------------------
// @extensibility(FINAL) @nested
// struct CompleteMapType {
//     CollectionTypeFlag            collection_flag;
//     CompleteCollectionHeader      header;
//     CompleteCollectionElement     key;
//     CompleteCollectionElement     element;
// };
struct CompleteMapType {
  CollectionTypeFlag collection_flag;
  struct CompleteCollectionHeader header;
  struct CompleteCollectionElement key;
  struct CompleteCollectionElement element;
};

// @extensibility(FINAL) @nested
// struct MinimalMapType {
//     CollectionTypeFlag          collection_flag;
//     MinimalCollectionHeader     header;
//     MinimalCollectionElement    key;
//     MinimalCollectionElement    element;
// };
struct MinimalMapType {
  CollectionTypeFlag collection_flag;
  struct MinimalCollectionHeader header;
  struct MinimalCollectionElement key;
  struct MinimalCollectionElement element;
};

// // --- Enumeration: ----------------------------------------------------
typedef unsigned short BitBound;

// // Constant in an enumerated type
// @extensibility(APPENDABLE) @nested
// struct CommonEnumeratedLiteral {
//     long                     value;
//     EnumeratedLiteralFlag    flags;
// };
struct CommonEnumeratedLiteral {
  int32_t value;
  EnumeratedLiteralFlag flags;
};

// // Constant in an enumerated type
// @extensibility(APPENDABLE) @nested
// struct CompleteEnumeratedLiteral {
//     CommonEnumeratedLiteral  common;
//     CompleteMemberDetail     detail;
// };
// // Ordered by EnumeratedLiteral.common.value
// typedef sequence<CompleteEnumeratedLiteral> CompleteEnumeratedLiteralSeq;
struct CompleteEnumeratedLiteral {
  struct CommonEnumeratedLiteral common;
  struct CompleteMemberDetail detail;
};
struct CompleteEnumeratedLiteralSeq {
  uint32_t length;
  struct CompleteEnumeratedLiteral * seq;
};

// // Constant in an enumerated type
// @extensibility(APPENDABLE) @nested
// struct MinimalEnumeratedLiteral {
//     CommonEnumeratedLiteral  common;
//     MinimalMemberDetail      detail;
// };
// // Ordered by EnumeratedLiteral.common.value
// typedef sequence<MinimalEnumeratedLiteral> MinimalEnumeratedLiteralSeq;
struct MinimalEnumeratedLiteral {
  struct CommonEnumeratedLiteral common;
  struct MinimalMemberDetail detail;
};
struct MinimalEnumeratedLiteralSeq {
  uint32_t length;
  struct MinimalEnumeratedLiteral * seq;
};

// @extensibility(FINAL) @nested
// struct CommonEnumeratedHeader {
//     BitBound                bit_bound;
// };
struct CommonEnumeratedHeader {
  BitBound bit_bound;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteEnumeratedHeader {
//     CommonEnumeratedHeader  common;
//     CompleteTypeDetail      detail;
// };
struct CompleteEnumeratedHeader {
  struct CommonEnumeratedHeader common;
  struct CompleteTypeDetail detail;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalEnumeratedHeader {
//     CommonEnumeratedHeader  common;
// };
struct MinimalEnumeratedHeader {
  struct CommonEnumeratedHeader common;
};

// // Enumerated type
// @extensibility(FINAL) @nested
// struct CompleteEnumeratedType  {
//     EnumTypeFlag                    enum_flags; // unused
//     CompleteEnumeratedHeader        header;
//     CompleteEnumeratedLiteralSeq    literal_seq;
// };
struct CompleteEnumeratedType  {
  EnumTypeFlag enum_flags; // unused
  struct CompleteEnumeratedHeader header;
  struct CompleteEnumeratedLiteralSeq literal_seq;
};

// // Enumerated type
// @extensibility(FINAL) @nested
// struct MinimalEnumeratedType  {
//     EnumTypeFlag                  enum_flags; // unused
//     MinimalEnumeratedHeader       header;
//     MinimalEnumeratedLiteralSeq   literal_seq;
// };
struct MinimalEnumeratedType  {
  EnumTypeFlag enum_flags; // unused
  struct MinimalEnumeratedHeader header;
  struct MinimalEnumeratedLiteralSeq literal_seq;
};

// // --- Bitmask: --------------------------------------------------------
// // Bit in a bit mask
// @extensibility(FINAL) @nested
// struct CommonBitflag {
//     unsigned short         position;
//     BitflagFlag            flags;
// };
struct CommonBitflag {
  uint16_t position;
  BitflagFlag flags;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteBitflag {
//     CommonBitflag          common;
//     CompleteMemberDetail   detail;
// };
// // Ordered by Bitflag.position
// typedef sequence<CompleteBitflag> CompleteBitflagSeq;
struct CompleteBitflag {
  struct CommonBitflag common;
  struct CompleteMemberDetail detail;
};
struct CompleteBitflagSeq {
  uint32_t length;
  struct CompleteBitflag * seq;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalBitflag {
//     CommonBitflag        common;
//     MinimalMemberDetail  detail;
// };
// // Ordered by Bitflag.position
// typedef sequence<MinimalBitflag> MinimalBitflagSeq;
struct MinimalBitflag {
  struct CommonBitflag common;
  struct MinimalMemberDetail detail;
};
struct MinimalBitflagSeq {
  uint32_t length;
  struct MinimalBitflag * seq;
};

// @extensibility(FINAL) @nested
// struct CommonBitmaskHeader {
//     BitBound             bit_bound;
// };
struct CommonBitmaskHeader {
  BitBound bit_bound;
};

typedef struct CompleteEnumeratedHeader CompleteBitmaskHeader;
typedef struct MinimalEnumeratedHeader MinimalBitmaskHeader;

// @extensibility(APPENDABLE) @nested
// struct CompleteBitmaskType {
//     BitmaskTypeFlag          bitmask_flags; // unused
//     CompleteBitmaskHeader    header;
//     CompleteBitflagSeq       flag_seq;
// };
struct CompleteBitmaskType {
  BitmaskTypeFlag bitmask_flags; // unused
  CompleteBitmaskHeader header;
  struct CompleteBitflagSeq flag_seq;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalBitmaskType {
//     BitmaskTypeFlag          bitmask_flags; // unused
//     MinimalBitmaskHeader     header;
//     MinimalBitflagSeq        flag_seq;
// };
struct MinimalBitmaskType {
  BitmaskTypeFlag bitmask_flags; // unused
  MinimalBitmaskHeader header;
  struct MinimalBitflagSeq flag_seq;
};

// // --- Bitset: ----------------------------------------------------------
// @extensibility(FINAL) @nested
// struct CommonBitfield {
//     unsigned short        position;
//     BitsetMemberFlag      flags;
//     uint8_t                 bitcount;
//     TypeKind              holder_type; // Must be primitive integer type
// };
struct CommonBitfield {
  uint16_t position;
  BitsetMemberFlag flags;
  uint8_t bitcount;
  TypeKind holder_type; // Must be primitive integer type
};

// @extensibility(APPENDABLE) @nested
// struct CompleteBitfield {
//     CommonBitfield           common;
//     CompleteMemberDetail     detail;
// };
// // Ordered by Bitfield.position
// typedef sequence<CompleteBitfield> CompleteBitfieldSeq;
struct CompleteBitfield {
  struct CommonBitfield common;
  struct CompleteMemberDetail detail;
};
struct CompleteBitfieldSeq {
  uint32_t length;
  struct CompleteBitfield * seq;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalBitfield {
//     CommonBitfield       common;
//     NameHash             name_hash;
// };
// // Ordered by Bitfield.position
// typedef sequence<MinimalBitfield> MinimalBitfieldSeq;
struct MinimalBitfield {
  struct CommonBitfield common;
  struct NameHash name_hash;
};
struct MinimalBitfieldSeq {
  uint32_t length;
  struct MinimalBitfield * seq;
};

// @extensibility(APPENDABLE) @nested
// struct CompleteBitsetHeader {
//     CompleteTypeDetail   detail;
// };
struct CompleteBitsetHeader {
  struct CompleteTypeDetail detail;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalBitsetHeader {
//     // Empty. Available for future extension
// };
struct MinimalBitsetHeader {
};

// @extensibility(APPENDABLE) @nested
// struct CompleteBitsetType  {
//     BitsetTypeFlag         bitset_flags; // unused
//     CompleteBitsetHeader   header;
//     CompleteBitfieldSeq    field_seq;
// };
struct CompleteBitsetType  {
  BitsetTypeFlag bitset_flags; // unused
  struct CompleteBitsetHeader header;
  struct CompleteBitfieldSeq field_seq;
};

// @extensibility(APPENDABLE) @nested
// struct MinimalBitsetType  {
//     BitsetTypeFlag       bitset_flags; // unused
//     MinimalBitsetHeader  header;
//     MinimalBitfieldSeq   field_seq;
// };
struct MinimalBitsetType  {
  BitsetTypeFlag bitset_flags; // unused
  struct MinimalBitsetHeader header;
  struct MinimalBitfieldSeq field_seq;
};


// // --- Type Object: ---------------------------------------------------
// // The types associated with each case selection must have extensibility
// // kind APPENDABLE or MUTABLE so that they can be extended in the future

// @extensibility(MUTABLE) @nested
// struct CompleteExtendedType {
//     // Empty. Available for future extension
// };
struct CompleteExtendedType {
};

// @extensibility(FINAL)     @nested
// union CompleteTypeObject switch (uint8_t) {
//     case TK_ALIAS:
//         CompleteAliasType      alias_type;
//     case TK_ANNOTATION:
//         CompleteAnnotationType annotation_type;
//     case TK_STRUCTURE:
//         CompleteStructType     struct_type;
//     case TK_UNION:
//         CompleteUnionType      union_type;
//     case TK_BITSET:
//         CompleteBitsetType     bitset_type;
//     case TK_SEQUENCE:
//         CompleteSequenceType   sequence_type;
//     case TK_ARRAY:
//         CompleteArrayType      array_type;
//     case TK_MAP:
//         CompleteMapType        map_type;
//     case TK_ENUM:
//         CompleteEnumeratedType enumerated_type;
//     case TK_BITMASK:
//         CompleteBitmaskType    bitmask_type;

//     // ===================  Future extensibility  ============
//     default:
//         CompleteExtendedType   extended_type;
// };
struct CompleteTypeObject {
  uint8_t _d;
  union {
    struct CompleteAliasType alias_type;
    struct CompleteAnnotationType annotation_type;
    struct CompleteStructType struct_type;
    struct CompleteUnionType union_type;
    struct CompleteBitsetType bitset_type;
    struct CompleteSequenceType sequence_type;
    struct CompleteArrayType array_type;
    struct CompleteMapType map_type;
    struct CompleteEnumeratedType enumerated_type;
    struct CompleteBitmaskType bitmask_type;
    struct CompleteExtendedType extended_type;
  } _u;
};

// @extensibility(MUTABLE) @nested
// struct MinimalExtendedType {
//     // Empty. Available for future extension
// };
struct MinimalExtendedType {
};

// @extensibility(FINAL)     @nested
// union MinimalTypeObject switch (uint8_t) {
//     case TK_ALIAS:
//         MinimalAliasType       alias_type;
//     case TK_ANNOTATION:
//         MinimalAnnotationType  annotation_type;
//     case TK_STRUCTURE:
//         MinimalStructType      struct_type;
//     case TK_UNION:
//         MinimalUnionType       union_type;
//     case TK_BITSET:
//         MinimalBitsetType      bitset_type;
//     case TK_SEQUENCE:
//         MinimalSequenceType    sequence_type;
//     case TK_ARRAY:
//         MinimalArrayType       array_type;
//     case TK_MAP:
//         MinimalMapType         map_type;
//     case TK_ENUM:
//         MinimalEnumeratedType  enumerated_type;
//     case TK_BITMASK:
//         MinimalBitmaskType     bitmask_type;

//     // ===================  Future extensibility  ============
//     default:
//         MinimalExtendedType    extended_type;
// };
struct MinimalTypeObject {
  uint8_t _d;
  union {
    struct MinimalAliasType alias_type;
    struct MinimalAnnotationType annotation_type;
    struct MinimalStructType struct_type;
    struct MinimalUnionType union_type;
    struct MinimalBitsetType bitset_type;
    struct MinimalSequenceType sequence_type;
    struct MinimalArrayType array_type;
    struct MinimalMapType map_type;
    struct MinimalEnumeratedType enumerated_type;
    struct MinimalBitmaskType bitmask_type;
    struct MinimalExtendedType extended_type;
  } _u;
};

// @extensibility(APPENDABLE)  @nested
// union TypeObject switch (uint8_t) { // EquivalenceKind
// case EK_COMPLETE:
//     CompleteTypeObject   complete;
// case EK_MINIMAL:
//     MinimalTypeObject    minimal;
// };
struct TypeObject {
  uint8_t _d;
  union {
    struct CompleteTypeObject complete;
    struct MinimalTypeObject minimal;
  } _u;
};

#if defined (__cplusplus)
}
#endif
#endif /* DDS_HAS_TYPE_DISCOVERY */
#endif /* DDSI_TYPE_OBJECT_H */
