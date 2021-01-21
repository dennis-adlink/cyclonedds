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
#ifndef DDSI_TYPEID_H
#define DDSI_TYPEID_H

#include "dds/features.h"
#ifdef DDS_HAS_TYPE_DISCOVERY

#include <stdint.h>
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_type_xtypes.h"

#define PFMT4B "%02x%02x%02x%02x"
#define PTYPEIDFMT PFMT4B "-" PFMT4B "-" PFMT4B "-" PFMT4B

#define PTYPEID4B(x, n) ((x).hash[n]), ((x).hash[n + 1]), ((x).hash[n + 2]), ((x).hash[n + 3])
#define PTYPEID(x) PTYPEID4B(x, 0), PTYPEID4B(x, 4), PTYPEID4B(x, 8), PTYPEID4B(x, 12)

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_sertype;
struct TypeIdentifier;
struct TypeObject;

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

DDS_EXPORT struct TypeIdentifier * ddsi_typeid_from_sertype (const struct ddsi_sertype * type);
DDS_EXPORT struct TypeIdentifier * ddsi_typeid_dup (const struct TypeIdentifier *type_id);
DDS_EXPORT bool ddsi_typeid_equal (const struct TypeIdentifier *a, const struct TypeIdentifier *b);
DDS_EXPORT bool ddsi_typeid_none (const struct TypeIdentifier *typeid);

#if defined (__cplusplus)
}
#endif
#endif /* DDS_HAS_TYPE_DISCOVERY */
#endif /* DDSI_TYPEID_H */
