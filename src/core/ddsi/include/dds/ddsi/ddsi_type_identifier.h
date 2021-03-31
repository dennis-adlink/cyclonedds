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
#ifndef DDSI_TYPE_IDENTIFIER_H
#define DDSI_TYPE_IDENTIFIER_H

#ifdef DDS_HAS_TYPE_DISCOVERY

#include <stdint.h>
#include <stdbool.h>
#include "dds/features.h"
#include "dds/ddsc/dds_opcodes.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_sertype;

#define PTYPEIDFMT "%u"
#define PTYPEID(x) ((x)._d)


#define TI_STRING8_SMALL                0x70
#define TI_STRING8_LARGE                0x71
#define TI_STRING16_SMALL               0x72
#define TI_STRING16_LARGE               0x73
#define TI_PLAIN_SEQUENCE_SMALL         0x80
#define TI_PLAIN_SEQUENCE_LARGE         0x81
#define TI_PLAIN_ARRAY_SMALL            0x90
#define TI_PLAIN_ARRAY_LARGE            0x91
#define TI_PLAIN_MAP_SMALL              0xA0
#define TI_PLAIN_MAP_LARGE              0xA1
#define TI_STRONGLY_CONNECTED_COMPONENT 0xB0


// Primitive TKs
#define TK_NONE       0x00
#define TK_BOOLEAN    0x01
#define TK_BYTE       0x02
#define TK_INT16      0x03
#define TK_INT32      0x04
#define TK_INT64      0x05
#define TK_UINT16     0x06
#define TK_UINT32     0x07
#define TK_UINT64     0x08
#define TK_FLOAT32    0x09
#define TK_FLOAT64    0x0A
#define TK_FLOAT128   0x0B
#define TK_CHAR8      0x10
#define TK_CHAR16     0x11

// String TKs
#define TK_STRING8    0x20
#define TK_STRING16   0x21

// Constructed/Named types
#define TK_ALIAS      0x30

// Enumerated TKs
#define TK_ENUM       0x40
#define TK_BITMASK    0x41

// Structured TKs
#define TK_ANNOTATION 0x50
#define TK_STRUCTURE  0x51
#define TK_UNION      0x52
#define TK_BITSET     0x53

// Collection TKs
#define TK_SEQUENCE   0x60
#define TK_ARRAY      0x61
#define TK_MAP        0x62

struct NameHash {
  uint8_t hash[4];
};

// @bit_bound(16)
// bitmask TypeFlag {
//     @position(0) IS_FINAL,        // F |
//     @position(1) IS_APPENDABLE,   // A |-  Struct, Union
//     @position(2) IS_MUTABLE,      // M |   (exactly one flag)

//     @position(3) IS_NESTED,       // N     Struct, Union
//     @position(4) IS_AUTOID_HASH   // H     Struct
// };
#define IS_FINAL        (1u << 0)
#define IS_APPENDABLE   (1u << 1)
#define IS_MUTABLE      (1u << 2)
#define IS_NESTED       (1u << 3)
#define IS_AUTOID_HASH  (1u << 4)

typedef uint16_t TypeFlag;
typedef TypeFlag   StructTypeFlag;      // All flags apply
typedef TypeFlag   UnionTypeFlag;       // All flags apply
typedef TypeFlag   CollectionTypeFlag;  // Unused. No flags apply
typedef TypeFlag   AnnotationTypeFlag;  // Unused. No flags apply
typedef TypeFlag   AliasTypeFlag;       // Unused. No flags apply
typedef TypeFlag   EnumTypeFlag;        // Unused. No flags apply
typedef TypeFlag   BitmaskTypeFlag;     // Unused. No flags apply
typedef TypeFlag   BitsetTypeFlag;      // Unused. No flags apply


#define INVALID_SBOUND 0
#define INVALID_LBOUND 0

typedef uint8_t SBound_t;
typedef struct SBoundSeq {
  uint32_t length;
  SBound_t * seq;
} SBoundSeq_t;

typedef uint32_t LBound_t;
struct LBoundSeq {
  uint32_t length;
  LBound_t * seq;
};

struct EquivalenceHash {
  uint8_t hash[14];
};

// @extensibility(FINAL) @nested
// union TypeObjectHashId switch (octet) {
//     case EK_COMPLETE:
//     case EK_MINIMAL:
//         EquivalenceHash  hash;
// };
struct TypeObjectHashId {
  uint8_t _d;
  union {
    struct EquivalenceHash hash;
  } _u;
};

struct TypeIdentifier;

typedef uint8_t EquivalenceKind;
#define EK_MINIMAL 0xf1  // 0x1111 0001
#define EK_COMPLETE 0xf2 // 0x1111 0010
#define EK_BOTH 0xf3     // 0x1111 0011

typedef uint8_t TypeKind;

// @bit_bound(16)
// bitmask MemberFlag {
//     @position(0)  TRY_CONSTRUCT1,     // T1 | 00 = INVALID, 01 = DISCARD
//     @position(1)  TRY_CONSTRUCT2,     // T2 | 10 = USE_DEFAULT, 11 = TRIM
//     @position(2)  IS_EXTERNAL,        // X  StructMember, UnionMember,
//                                     //    CollectionElement
//     @position(3)  IS_OPTIONAL,        // O  StructMember
//     @position(4)  IS_MUST_UNDERSTAND, // M  StructMember
//     @position(5)  IS_KEY,          // K  StructMember, UnionDiscriminator
//     @position(6)  IS_DEFAULT       // D  UnionMember, EnumerationLiteral
// };

#define TRY_CONSTRUCT1      (1u << 0)
#define TRY_CONSTRUCT2      (1u << 1)
#define IS_EXTERNAL         (1u << 2)
#define IS_OPTIONAL         (1u << 3)
#define IS_MUST_UNDERSTAND  (1u << 4)
#define IS_KEY              (1u << 5)
#define IS_DEFAULT          (1u << 6)

typedef uint16_t MemberFlag;

typedef MemberFlag CollectionElementFlag;   // T1, T2, X
typedef MemberFlag StructMemberFlag;        // T1, T2, O, M, K, X
typedef MemberFlag UnionMemberFlag;         // T1, T2, D, X
typedef MemberFlag UnionDiscriminatorFlag;  // T1, T2, K
typedef MemberFlag EnumeratedLiteralFlag;   // D
typedef MemberFlag AnnotationParameterFlag; // Unused. No flags apply
typedef MemberFlag AliasMemberFlag;         // Unused. No flags apply
typedef MemberFlag BitflagFlag;             // Unused. No flags apply
typedef MemberFlag BitsetMemberFlag;        // Unused. No flags apply

// @extensibility(FINAL) @nested
// struct StringSTypeDefn {
// 	SBound                  bound;
// };
struct StringSTypeDefn
{
  SBound_t bound;
};

// @extensibility(FINAL) @nested
// struct StringLTypeDefn {
//     LBound                  bound;
// };
struct StringLTypeDefn
{
  LBound_t bound;
};

// @extensibility(FINAL) @nested
// struct PlainCollectionHeader {
//     EquivalenceKind        equiv_kind;
//     CollectionElementFlag  element_flags;
// };
struct PlainCollectionHeader
{
  EquivalenceKind equiv_kind;
  CollectionElementFlag element_flags;
};

// @extensibility(FINAL) @nested
// struct PlainSequenceSElemDefn {
//     PlainCollectionHeader  header;
//     SBound                 bound;
//     @external TypeIdentifier element_identifier;
// };
struct PlainSequenceSElemDefn
{
  struct PlainCollectionHeader header;
  SBound_t bound;
  struct TypeIdentifier * element_identifier;
};

// @extensibility(FINAL) @nested
// struct PlainSequenceLElemDefn {
//     PlainCollectionHeader  header;
//     LBound                 bound;
//     @external TypeIdentifier element_identifier;
// };
struct PlainSequenceLElemDefn {
  struct PlainCollectionHeader header;
  LBound_t bound;
  struct TypeIdentifier * element_identifier;
};

// @extensibility(FINAL) @nested
// struct PlainArraySElemDefn {
//     PlainCollectionHeader  header;
//     SBoundSeq              array_bound_seq;
//     @external TypeIdentifier element_identifier;
// };
struct PlainArraySElemDefn {
  struct PlainCollectionHeader header;
  struct SBoundSeq array_bound_seq;
  struct TypeIdentifier * element_identifier;
};

// @extensibility(FINAL) @nested
// struct PlainArrayLElemDefn {
//     PlainCollectionHeader  header;
//     LBoundSeq              array_bound_seq;
//     @external TypeIdentifier element_identifier;
// };
struct PlainArrayLElemDefn {
  struct PlainCollectionHeader header;
  struct LBoundSeq array_bound_seq;
  struct TypeIdentifier * element_identifier;
};

// @extensibility(FINAL) @nested
// struct PlainMapSTypeDefn {
//     PlainCollectionHeader  header;
//     SBound                 bound;
//     @external TypeIdentifier element_identifier;
//     CollectionElementFlag  key_flags;
//     @external TypeIdentifier key_identifier;
// };
struct PlainMapSTypeDefn {
  struct PlainCollectionHeader header;
  SBound_t bound;
  struct TypeIdentifier * element_identifier;
  CollectionElementFlag key_flags;
  struct TypeIdentifier * key_identifier;
};

// @extensibility(FINAL) @nested
// struct PlainMapLTypeDefn {
//     PlainCollectionHeader  header;
//     LBound                 bound;
//     @external TypeIdentifier element_identifier;
//     CollectionElementFlag  key_flags;
//     @external TypeIdentifier key_identifier;
// };
struct PlainMapLTypeDefn {
  struct PlainCollectionHeader header;
  LBound_t bound;
  struct TypeIdentifier * element_identifier;
  CollectionElementFlag  key_flags;
  struct TypeIdentifier * key_identifier;
};

// // Used for Types that have cyclic depencencies with other types
// @extensibility(APPENDABLE) @nested
// struct StronglyConnectedComponentId {
//     TypeObjectHashId sc_component_id; // Hash StronglyConnectedComponent
//     long   scc_length; // StronglyConnectedComponent.length
//     long   scc_index ; // identify type in Strongly Connected Comp.
// };
struct StronglyConnectedComponentId {
  struct TypeObjectHashId sc_component_id; // Hash StronglyConnectedComponent
  long scc_length; // StronglyConnectedComponent.length
  long scc_index; // identify type in Strongly Connected Comp.
};

// // Future extensibility
// @extensibility(MUTABLE) @nested
// struct ExtendedTypeDefn {
//     // Empty. Available for future extension
// };
struct ExtendedTypeDefn {
};

// @extensibility(FINAL) @nested
// union TypeIdentifier switch (octet) {
//  // ============  Primitive types - use TypeKind ====================
//  // All primitive types fall here.
//  // Commented-out because Unions cannot have cases with no member.
//  /*
//  case TK_NONE:
//  case TK_BOOLEAN:
//  case TK_BYTE_TYPE:
//  case TK_INT16_TYPE:
//  case TK_INT32_TYPE:
//  case TK_INT64_TYPE:
//  case TK_UINT8_TYPE:
//  case TK_UINT16_TYPE:
//  case TK_UINT32_TYPE:
//  case TK_UINT64_TYPE:
//  case TK_FLOAT32_TYPE:
//  case TK_FLOAT64_TYPE:
//  case TK_FLOAT128_TYPE:
//  case TK_CHAR8_TYPE:
//  case TK_CHAR16_TYPE:
//      // No Value
//  */
//  // ============ Strings - use TypeIdentifierKind ===================
//  case TI_STRING8_SMALL:
//  case TI_STRING16_SMALL:
//      StringSTypeDefn         string_sdefn;
//  case TI_STRING8_LARGE:
//  case TI_STRING16_LARGE:
//      StringLTypeDefn         string_ldefn;
//  // ============  Plain collectios - use TypeIdentifierKind =========
//  case TI_PLAIN_SEQUENCE_SMALL:
//      PlainSequenceSElemDefn  seq_sdefn;
//  case TI_PLAIN_SEQUENCE_LARGE:
//      PlainSequenceLElemDefn  seq_ldefn;
//  case TI_PLAIN_ARRAY_SMALL:
//      PlainArraySElemDefn     array_sdefn;
//  case TI_PLAIN_ARRAY_LARGE:
//      PlainArrayLElemDefn     array_ldefn;
//  case TI_PLAIN_MAP_SMALL:
//      PlainMapSTypeDefn       map_sdefn;
//  case TI_PLAIN_MAP_LARGE:
//      PlainMapLTypeDefn       map_ldefn;
//  // ============  Types that are mutually dependent on each other ===
//  case TI_STRONGLY_CONNECTED_COMPONENT:
//      StronglyConnectedComponentId  sc_component_id;
//  // ============  The remaining cases - use EquivalenceKind =========
//  case EK_COMPLETE:
//  case EK_MINIMAL:
//      EquivalenceHash         equivalence_hash;
//  // ===================  Future extensibility  ============
//  // Future extensions
//  default:
//      ExtendedTypeDefn        extended_defn;
// };
struct TypeIdentifier
{
  uint8_t _d;
  union {
    struct StringSTypeDefn string_sdefn;
    struct StringLTypeDefn string_ldefn;
    struct PlainSequenceSElemDefn seq_sdefn;
    struct PlainSequenceLElemDefn seq_ldefn;
    struct PlainArraySElemDefn array_sdefn;
    struct PlainArrayLElemDefn array_ldefn;
    struct PlainMapSTypeDefn map_sdefn;
    struct PlainMapLTypeDefn map_ldefn;
    struct StronglyConnectedComponentId sc_component_id;
    struct EquivalenceHash equivalence_hash;
    struct ExtendedTypeDefn extended_defn;
  } _u;
};

typedef struct TypeIdentifierSeq {
  uint32_t n;
  struct TypeIdentifier * type_ids;
} TypeIdentifierSeq_t;

typedef struct TypeIdentifierTypeObjectPair {
  struct TypeIdentifier *type_identifier;
  struct TypeObject *type_object;
} TypeIdentifierTypeObjectPair_t;

typedef struct TypeIdentifierTypeObjectPairSeq {
  uint32_t n;
  struct TypeIdentifierTypeObjectPair * types;
} TypeIdentifierTypeObjectPairSeq_t;

#define CDRSTREAM_OPS_TYPE_IDENTIFIER \
  /* struct TypeIdentifier */ \
  DDS_OP_DLC, \
  DDS_OP_ADR | DDS_OP_TYPE_UNI | DDS_OP_SUBTYPE_ENU, offsetof (struct TypeIdentifier, _d), 13u, (44u << 16) + 5u, 0xffu, \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 40u + 0u, TI_STRING8_SMALL, offsetof (struct TypeIdentifier, _u.string_sdefn), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 37u + 0u, TI_STRING16_SMALL, offsetof (struct TypeIdentifier, _u.string_sdefn), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 34u + 3u, TI_STRING8_LARGE, offsetof (struct TypeIdentifier, _u.string_ldefn), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 31u + 3u, TI_STRING16_LARGE, offsetof (struct TypeIdentifier, _u.string_ldefn), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 28u + 6u, TI_PLAIN_SEQUENCE_SMALL, offsetof (struct TypeIdentifier, _u.seq_sdefn), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 25u + 17u, TI_PLAIN_SEQUENCE_LARGE, offsetof (struct TypeIdentifier, _u.seq_ldefn), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 22u + 28u, TI_PLAIN_ARRAY_SMALL, offsetof (struct TypeIdentifier, _u.array_sdefn), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 19u + 39u, TI_PLAIN_ARRAY_LARGE, offsetof (struct TypeIdentifier, _u.array_ldefn), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 16u + 50u, TI_PLAIN_MAP_SMALL, offsetof (struct TypeIdentifier, _u.map_sdefn), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 13u + 68u, TI_PLAIN_MAP_LARGE, offsetof (struct TypeIdentifier, _u.map_ldefn), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 10u + 86u, TI_STRONGLY_CONNECTED_COMPONENT, offsetof (struct TypeIdentifier, _u.sc_component_id), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 7u + 87u, EK_COMPLETE, offsetof (struct TypeIdentifier, _u.equivalence_hash), \
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 4u + 87u, EK_MINIMAL, offsetof (struct TypeIdentifier, _u.equivalence_hash), \
  DDS_OP_RTS, \
  /* struct StringSTypeDefn */ \
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (struct StringSTypeDefn, bound), \
  DDS_OP_RTS, /* (3) */ \
  /* struct StringLTypeDefn */ \
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (struct StringLTypeDefn, bound), \
  DDS_OP_RTS, /* (3) */ \
  /* struct PlainSequenceSElemDefn */ \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainSequenceSElemDefn, header), (3u << 16u) + 85u, /* struct PlainCollectionHeader */ \
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (struct PlainSequenceSElemDefn, bound), \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainSequenceSElemDefn, element_identifier), (5u << 16u) + 3u, \
    DDS_OP_JSR | (65536 - 59), DDS_OP_RTS, /* struct TypeIdentifier */ \
  DDS_OP_RTS, /* (11) */ \
  /* struct PlainSequenceLElemDefn */ \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainSequenceLElemDefn, header), (3u << 16u) + 74u, /* struct PlainCollectionHeader */ \
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (struct PlainSequenceLElemDefn, bound), \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainSequenceLElemDefn, element_identifier), (5u << 16u) + 3u, \
    DDS_OP_JSR | (65536 - 70), DDS_OP_RTS, /* struct TypeIdentifier */ \
  DDS_OP_RTS, /* (11) */ \
  /* struct PlainArraySElemDefn */ \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainArraySElemDefn, header), (3u << 16u) + 63u, /* struct PlainCollectionHeader */ \
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_1BY, offsetof (struct PlainArraySElemDefn, array_bound_seq), \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainArraySElemDefn, element_identifier), (5u << 16u) + 3u, \
    DDS_OP_JSR | (65536 - 81), DDS_OP_RTS, /* struct TypeIdentifier */ \
  DDS_OP_RTS, /* (11) */ \
  /* struct PlainArrayLElemDefn */ \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainArrayLElemDefn, header), (3u << 16u) + 52u, /* struct PlainCollectionHeader */ \
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_4BY, offsetof (struct PlainArrayLElemDefn, array_bound_seq), \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainArrayLElemDefn, element_identifier), (5u << 16u) + 3u, \
    DDS_OP_JSR | (65536 - 92), DDS_OP_RTS, /* struct TypeIdentifier */ \
  DDS_OP_RTS, /* (11) */ \
  /* struct PlainMapSTypeDefn */ \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapSTypeDefn, header), (3u << 16u) + 41u, /* struct PlainCollectionHeader */ \
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (struct PlainMapSTypeDefn, bound), \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapSTypeDefn, element_identifier), (5u << 16u) + 3u, \
    DDS_OP_JSR | (65536 - 103), DDS_OP_RTS, /* struct TypeIdentifier */ \
  DDS_OP_ADR | DDS_OP_TYPE_2BY, offsetof (struct PlainMapSTypeDefn, key_flags), \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapSTypeDefn, key_identifier), (5u << 16u) + 3u, \
    DDS_OP_JSR | (65536 - 110), DDS_OP_RTS, /* struct TypeIdentifier */ \
  DDS_OP_RTS, /* (18) */ \
  /* struct PlainMapLTypeDefn */ \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapLTypeDefn, header), (3u << 16u) + 23u, /* struct PlainCollectionHeader */ \
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (struct PlainMapLTypeDefn, bound), \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapLTypeDefn, element_identifier), (5u << 16u) + 3u, \
    DDS_OP_JSR | (65536 - 121), DDS_OP_RTS, /* struct TypeIdentifier */ \
  DDS_OP_ADR | DDS_OP_TYPE_2BY, offsetof (struct PlainMapLTypeDefn, key_flags), \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapLTypeDefn, key_identifier), (5u << 16u) + 3u, \
    DDS_OP_JSR | (65536 - 128), DDS_OP_RTS, /* struct TypeIdentifier */ \
  DDS_OP_RTS, /* (18) */ \
  /* struct StronglyConnectedComponentId */ \
  /* FIXME */ \
  DDS_OP_RTS, /* (1) */ \
  /* struct EquivalenceHash */ \
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_1BY, offsetof (struct EquivalenceHash, hash), 14, \
  DDS_OP_RTS, /* (4) */ \
  /* struct PlainCollectionHeader */ \
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (struct PlainCollectionHeader, equiv_kind), \
  DDS_OP_ADR | DDS_OP_TYPE_2BY, offsetof (struct PlainCollectionHeader, element_flags), \
  DDS_OP_RTS, /* (5) */

static const uint32_t TypeIdentifier_ops [] =
{
  CDRSTREAM_OPS_TYPE_IDENTIFIER
};

DDS_EXPORT void ddsi_typeid_copy (struct TypeIdentifier *dst, const struct TypeIdentifier *src);
DDS_EXPORT int ddsi_typeid_compare (const struct TypeIdentifier *a, const struct TypeIdentifier *b);
DDS_EXPORT void ddsi_typeid_ser (const struct TypeIdentifier *typeid, unsigned char **buf, uint32_t *sz);
DDS_EXPORT void ddsi_typeid_deser (unsigned char *buf, uint32_t sz, struct TypeIdentifier **typeid);
DDS_EXPORT bool ddsi_typeid_is_none (const struct TypeIdentifier *typeid);
DDS_EXPORT bool ddsi_typeid_is_hash (const struct TypeIdentifier *typeid);
DDS_EXPORT bool ddsi_typeid_is_minimal (const struct TypeIdentifier *typeid);
DDS_EXPORT bool ddsi_typeid_is_complete (const struct TypeIdentifier *typeid);

#if defined (__cplusplus)
}
#endif
#endif /* DDS_HAS_TYPE_DISCOVERY */
#endif /* DDSI_TYPE_IDENTIFIER_H */
