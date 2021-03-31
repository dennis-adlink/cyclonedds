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
#include "dds/ddsi/ddsi_type_identifier.h"
#include "dds/ddsc/dds_public_impl.h"
#include "dds/ddsc/dds_opcodes.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;

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


#define CDRSTREAM_OPS_TYPE_INFORMATION \
  /* struct TypeInformation (mutable) */ \
  DDS_OP_PLC, \
    DDS_OP_JEQ | 5u, 0x1001, \
    DDS_OP_JEQ | 7u, 0x1002, \
  DDS_OP_RTS, \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct TypeInformation, minimal), (3u << 16u) + 8u, \
  DDS_OP_RTS, \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct TypeInformation, complete), (3u << 16u) + 4u, \
  DDS_OP_RTS, \
  /* struct TypeIdentifierWithDependencies (appendable) */ \
  DDS_OP_DLC, \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct TypeIdentifierWithDependencies, typeid_with_size), (3u << 16u) + 10u, \
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN, offsetof (struct TypeIdentifierWithDependencies, dependent_typeid_count), \
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (struct TypeIdentifierWithDependencies, dependent_typeids), sizeof (struct TypeIdentifierWithSize), (4u << 16u) + 5u, \
  DDS_OP_RTS, \
  /* struct TypeIdentifierWithSize (appendable) */ \
  DDS_OP_DLC, \
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (struct TypeIdentifierWithSize, type_id), (3u << 16u) + 6u, \
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (struct TypeIdentifierWithSize, typeobject_serialized_size), \
  DDS_OP_RTS

static const uint32_t TypeInformation_ops [] =
{
  CDRSTREAM_OPS_TYPE_INFORMATION,
  CDRSTREAM_OPS_TYPE_IDENTIFIER
};

bool ddsi_type_information_equal (const struct TypeInformation *a, const struct TypeInformation *b);

#if defined (__cplusplus)
}
#endif
#endif /* DDS_HAS_TYPE_DISCOVERY */
#endif /* DDSI_TYPE_INFORMATION_H */
