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
#ifndef DDSI_TYPE_INFORMATION_H
#define DDSI_TYPE_INFORMATION_H

#include "dds/features.h"

#ifdef DDS_HAS_TYPE_DISCOVERY
#include <stdint.h>
#include "dds/ddsc/dds_public_impl.h"
#include "dds/ddsi/ddsi_type_identifier.h"

#if defined (__cplusplus)
extern "C" {
#endif

// @extensibility(APPENDABLE)
// struct TypeIdentfierWithSize {
//  TypeIdentifier type_id;
//  unsigned long typeobject_serialized_size;
// };
struct TypeIdentifierWithSize {
  struct TypeIdentifier type_id;
  uint32_t typeobject_serialized_size;
};
struct TypeIdentifierWithSizeSeq {
  uint32_t length;
  struct TypeIdentifierWithSize * seq;
};

// @extensibility(APPENDABLE)
// struct TypeIdentifierWithDependencies {
//  TypeIdentfierWithSize typeid_with_size;
//  // The total additional types related to minimal_type
//  long dependent_typeid_count;
//  sequence<TypeIdentfierWithSize> dependent_typeids;
// };
// typedef sequence<TypeIdentifierWithDependencies> TypeIdentifierWithDependenciesSeq;
struct TypeIdentifierWithDependencies {
  struct TypeIdentifierWithSize typeid_with_size;
  int32_t dependent_typeid_count;
  struct TypeIdentifierWithSizeSeq dependent_typeids;
};

// @extensibility(MUTABLE)
// struct TypeInformation {
//  @id(0x1001) TypeIdentifierWithDependencies minimal;
//  @id(0x1002) TypeIdentifierWithDependencies complete;
// };
// typedef sequence<TypeInformation> TypeInformationSeq;
struct TypeInformation {
  struct TypeIdentifierWithDependencies minimal;
  struct TypeIdentifierWithDependencies complete;
};
struct TypeInformationSeq {
  uint32_t length;
  struct TypeInformation *seq;
};


static const uint32_t TypeInformation_ops [] =
{
  /* struct TypeInformation (mutable) */
  DDS_OP_PLC,
    DDS_OP_JEQ | 5u, 0x1001,
    DDS_OP_JEQ | 7u, 0x1002,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct TypeInformation, minimal), (3u << 16u) + 8u,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct TypeInformation, complete), (3u << 16u) + 4u,
  DDS_OP_RTS,

  /* struct TypeIdentifierWithDependencies (appendable) */
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct TypeIdentifierWithDependencies, typeid_with_size), (3u << 16u) + 10u,
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN, offsetof (struct TypeIdentifierWithDependencies, dependent_typeid_count),
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (struct TypeIdentifierWithDependencies, dependent_typeids), sizeof (struct TypeIdentifierWithSize), (4u << 16u) + 5u,
  DDS_OP_RTS,

  /* struct TypeIdentifierWithSize (appendable) */
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct TypeIdentifierWithSize, type_id), (3u << 16u) + 6u,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (struct TypeIdentifierWithSize, typeobject_serialized_size),
  DDS_OP_RTS,

  /* struct TypeIdentifier */
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_UNI | DDS_OP_SUBTYPE_ENU, offsetof (struct TypeIdentifier, _d), 13u, (44u << 16) + 5u, 0xffu,
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 40u + 0u, TI_STRING8_SMALL, offsetof (struct TypeIdentifier, _u.string_sdefn),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 37u + 0u, TI_STRING16_SMALL, offsetof (struct TypeIdentifier, _u.string_sdefn),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 34u + 3u, TI_STRING8_LARGE, offsetof (struct TypeIdentifier, _u.string_ldefn),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 31u + 3u, TI_STRING16_LARGE, offsetof (struct TypeIdentifier, _u.string_ldefn),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 28u + 6u, TI_PLAIN_SEQUENCE_SMALL, offsetof (struct TypeIdentifier, _u.seq_sdefn),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 25u + 17u, TI_PLAIN_SEQUENCE_LARGE, offsetof (struct TypeIdentifier, _u.seq_ldefn),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 22u + 28u, TI_PLAIN_ARRAY_SMALL, offsetof (struct TypeIdentifier, _u.array_sdefn),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 19u + 39u, TI_PLAIN_ARRAY_LARGE, offsetof (struct TypeIdentifier, _u.array_ldefn),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 16u + 50u, TI_PLAIN_MAP_SMALL, offsetof (struct TypeIdentifier, _u.map_sdefn),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 13u + 68u, TI_PLAIN_MAP_LARGE, offsetof (struct TypeIdentifier, _u.map_ldefn),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 10u + 86u, TI_STRONGLY_CONNECTED_COMPONENT, offsetof (struct TypeIdentifier, _u.sc_component_id),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 7u + 87u, EK_COMPLETE, offsetof (struct TypeIdentifier, _u.equivalence_hash),
    DDS_OP_JEQ | DDS_OP_TYPE_STU | 4u + 87u, EK_MINIMAL, offsetof (struct TypeIdentifier, _u.equivalence_hash),
  DDS_OP_RTS,

  /* struct StringSTypeDefn */
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (struct StringSTypeDefn, bound),
  DDS_OP_RTS, /* (3) */

  /* struct StringLTypeDefn */
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (struct StringLTypeDefn, bound),
  DDS_OP_RTS, /* (3) */

  /* struct PlainSequenceSElemDefn */
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainSequenceSElemDefn, header), (3u << 16u) + 85u, /* struct PlainCollectionHeader */
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (struct PlainSequenceSElemDefn, bound),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainSequenceSElemDefn, element_identifier), (5u << 16u) + 3u,
    DDS_OP_JSR | (65536 - 59), DDS_OP_RTS, /* struct TypeIdentifier */
  DDS_OP_RTS, /* (11) */

  /* struct PlainSequenceLElemDefn */
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainSequenceLElemDefn, header), (3u << 16u) + 74u, /* struct PlainCollectionHeader */
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (struct PlainSequenceLElemDefn, bound),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainSequenceLElemDefn, element_identifier), (5u << 16u) + 3u,
    DDS_OP_JSR | (65536 - 70), DDS_OP_RTS, /* struct TypeIdentifier */
  DDS_OP_RTS, /* (11) */

  /* struct PlainArraySElemDefn */
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainArraySElemDefn, header), (3u << 16u) + 63u, /* struct PlainCollectionHeader */
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_1BY, offsetof (struct PlainArraySElemDefn, array_bound_seq),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainArraySElemDefn, element_identifier), (5u << 16u) + 3u,
    DDS_OP_JSR | (65536 - 81), DDS_OP_RTS, /* struct TypeIdentifier */
  DDS_OP_RTS, /* (11) */

  /* struct PlainArrayLElemDefn */
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainArrayLElemDefn, header), (3u << 16u) + 52u, /* struct PlainCollectionHeader */
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_4BY, offsetof (struct PlainArrayLElemDefn, array_bound_seq),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainArrayLElemDefn, element_identifier), (5u << 16u) + 3u,
    DDS_OP_JSR | (65536 - 92), DDS_OP_RTS, /* struct TypeIdentifier */
  DDS_OP_RTS, /* (11) */

  /* struct PlainMapSTypeDefn */
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapSTypeDefn, header), (3u << 16u) + 41u, /* struct PlainCollectionHeader */
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (struct PlainMapSTypeDefn, bound),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapSTypeDefn, element_identifier), (5u << 16u) + 3u,
    DDS_OP_JSR | (65536 - 103), DDS_OP_RTS, /* struct TypeIdentifier */
  DDS_OP_ADR | DDS_OP_TYPE_2BY, offsetof (struct PlainMapSTypeDefn, key_flags),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapSTypeDefn, key_identifier), (5u << 16u) + 3u,
    DDS_OP_JSR | (65536 - 110), DDS_OP_RTS, /* struct TypeIdentifier */
  DDS_OP_RTS, /* (18) */

  /* struct PlainMapLTypeDefn */
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapLTypeDefn, header), (3u << 16u) + 23u, /* struct PlainCollectionHeader */
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (struct PlainMapLTypeDefn, bound),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapLTypeDefn, element_identifier), (5u << 16u) + 3u,
    DDS_OP_JSR | (65536 - 121), DDS_OP_RTS, /* struct TypeIdentifier */
  DDS_OP_ADR | DDS_OP_TYPE_2BY, offsetof (struct PlainMapLTypeDefn, key_flags),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct PlainMapLTypeDefn, key_identifier), (5u << 16u) + 3u,
    DDS_OP_JSR | (65536 - 128), DDS_OP_RTS, /* struct TypeIdentifier */
  DDS_OP_RTS, /* (18) */

  /* struct StronglyConnectedComponentId */
  /* FIXME */
  DDS_OP_RTS, /* (1) */

  /* struct EquivalenceHash */
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_1BY, offsetof (struct EquivalenceHash, hash), 14,
  DDS_OP_RTS, /* (4) */

  /* struct PlainCollectionHeader */
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (struct PlainCollectionHeader, equiv_kind),
  DDS_OP_ADR | DDS_OP_TYPE_2BY, offsetof (struct PlainCollectionHeader, element_flags),
  DDS_OP_RTS, /* (5) */
};

bool ddsi_type_information_equal (const struct TypeInformation *a, const struct TypeInformation *b);
struct TypeInformation *ddsi_type_information_lookup (const struct ddsi_domaingv *gv, const struct TypeIdentifier *typeid);

#if defined (__cplusplus)
}
#endif
#endif /* DDS_HAS_TYPE_DISCOVERY */
#endif /* DDSI_TYPE_INFORMATION_H */
