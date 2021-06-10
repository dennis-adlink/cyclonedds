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
#ifndef DDSI_XT_H
#define DDSI_XT_H

#include "dds/features.h"

#ifdef DDS_HAS_TYPE_DISCOVERY

#include <stdbool.h>
#include <stdint.h>
#include "dds/ddsi/ddsi_xt.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"
#include "dds/ddsc/dds_opcodes.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define PTYPEIDFMT "%u"
#define PTYPEID(x) ((x)._d)

typedef DDS_XTypes_TypeIdentifier ddsi_typeid_t;
typedef DDS_XTypes_TypeObject ddsi_typeobj_t;
typedef DDS_XTypes_TypeInformation ddsi_typeinfo_t;

DDS_EXPORT void ddsi_typeid_copy (ddsi_typeid_t *dst, const ddsi_typeid_t *src);
DDS_EXPORT int ddsi_typeid_compare (const ddsi_typeid_t *a, const ddsi_typeid_t *b);
DDS_EXPORT void ddsi_typeid_ser (const ddsi_typeid_t *typeid, unsigned char **buf, uint32_t *sz);
DDS_EXPORT void ddsi_typeid_deser (unsigned char *buf, uint32_t sz, ddsi_typeid_t **typeid);
DDS_EXPORT bool ddsi_typeid_is_none (const ddsi_typeid_t *typeid);
DDS_EXPORT bool ddsi_typeid_is_hash (const ddsi_typeid_t *typeid);
DDS_EXPORT bool ddsi_typeid_is_minimal (const ddsi_typeid_t *typeid);
DDS_EXPORT bool ddsi_typeid_is_complete (const ddsi_typeid_t *typeid);

void ddsi_typeobj_ser (const ddsi_typeobj_t *typeobj, unsigned char **buf, uint32_t *sz);
void ddsi_typeobj_deser (unsigned char *buf, uint32_t sz, ddsi_typeobj_t **typeobj);
bool ddsi_typeobj_is_minimal (const ddsi_typeobj_t *typeobj);
bool ddsi_typeobj_is_complete (const ddsi_typeobj_t *typeobj);

bool ddsi_type_information_equal (const ddsi_typeinfo_t *a, const ddsi_typeinfo_t *b);

#if defined (__cplusplus)
}
#endif
#endif /* DDS_HAS_TYPE_DISCOVERY */
#endif /* DDSI_XT_H */
