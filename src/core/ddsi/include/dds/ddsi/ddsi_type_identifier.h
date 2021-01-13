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

#if defined (__cplusplus)
extern "C" {
#endif

typedef uint8_t SBound;
typedef SBound * SBoundSeq;
#define INVALID_SBOUND 0

typedef uint32_t LBound;
#define INVALID_LBOUND 0
struct LBoundSeq {
  uint32_t length;
  LBound * seq;
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
  SBound bound;
};

// @extensibility(FINAL) @nested
// struct StringLTypeDefn {
//     LBound                  bound;
// };
struct StringLTypeDefn
{
  LBound bound;
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
  SBound bound;
  struct TypeIdentifier *element_identifier;
};

// @extensibility(FINAL) @nested
// struct PlainSequenceLElemDefn {
//     PlainCollectionHeader  header;
//     LBound                 bound;
//     @external TypeIdentifier element_identifier;
// };
struct PlainSequenceLElemDefn {
  struct PlainCollectionHeader header;
  LBound bound;
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
  SBoundSeq array_bound_seq;
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
  SBound bound;
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
  LBound bound;
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
//     ...
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

#if defined (__cplusplus)
}
#endif
#endif /* DDS_HAS_TYPE_DISCOVERY */
#endif /* DDSI_TYPE_IDENTIFIER_H */
