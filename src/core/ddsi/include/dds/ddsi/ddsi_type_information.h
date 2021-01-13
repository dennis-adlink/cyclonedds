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
#ifdef DDS_HAS_TYPE_DISCOVERY
#include <stdint.h>
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

#if defined (__cplusplus)
}
#endif
#endif /* DDS_HAS_TYPE_DISCOVERY */
#endif /* DDSI_TYPE_INFORMATION_H */
