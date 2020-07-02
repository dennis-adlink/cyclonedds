/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDS__SERDATA_BUILTINTYPE_H
#define DDS__SERDATA_BUILTINTYPE_H

#include "dds/dds.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertype.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_serdata_builtintype {
  struct ddsi_serdata c;
  ddsi_guid_t key;
  dds_instance_handle_t pphandle;
  dds_qos_t xqos;
};

enum ddsi_sertype_builtintype_entity_kind {
  DSBT_PARTICIPANT,
  DSBT_READER,
  DSBT_WRITER
};

struct ddsi_sertype_builtintype {
  struct ddsi_sertype c;
  enum ddsi_sertype_builtintype_entity_kind entity_kind;
};

extern const struct ddsi_sertype_ops ddsi_sertype_ops_builtintype;
extern const struct ddsi_serdata_ops ddsi_serdata_ops_builtintype;

struct ddsi_sertype *new_sertype_builtintype (struct ddsi_domaingv *gv, enum ddsi_sertype_builtintype_entity_kind entity_kind, const char *typename);

#if defined (__cplusplus)
}
#endif

#endif /* DDS__SERDATA_BUILTINTYPE_H */
