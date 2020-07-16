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
#ifndef DDSI_TYPELOOKUP_H
#define DDSI_TYPELOOKUP_H
#ifdef DDSI_INCLUDE_TYPE_DISCOVERY

#include <stdint.h>
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_plist_generic.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_typeid.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_guid;
struct ddsi_domaingv;
struct thread_state1;
struct nn_xpack;
struct participant;
struct receiver_state;
struct ddsi_serdata;

typedef struct type_lookup_request {
  ddsi_guid_t writer_guid;
  seqno_t sequence_number;
  type_identifier_seq_t type_ids;
} type_lookup_request_t;

typedef struct type_lookup_reply {
  ddsi_guid_t writer_guid;
  seqno_t sequence_number;
  type_identifier_type_object_pair_seq_t types;
} type_lookup_reply_t;

enum tl_meta_state
{
  TL_META_NEW,
  TL_META_REQUESTED,
  TL_META_RESOLVED
};

struct tl_meta_endpoints {
  uint32_t count;     /* number of endpoints in eps */
  ddsi_guid_t *eps;   /* list of endpoint guids */
};

struct tl_meta {
  type_identifier_t type_id;            /* type identifier for this record */
  const struct ddsi_sertype *sertype;   /* sertype associated with the type identifier, NULL if type is unresolved */
  enum tl_meta_state state;             /* state of this record */
  seqno_t request_seqno;                /* sequence number of the last type lookup request message */
  struct tl_meta_endpoints proxy_endpoints;    /* administration for proxy endpoints that are using this type */
  uint32_t refc;                        /* refcount for this record */
};

extern const enum pserop typelookup_service_request_ops[];
extern size_t typelookup_service_request_nops;
extern const enum pserop typelookup_service_reply_ops[];
extern size_t typelookup_service_reply_nops;

/**
 * Reference the type lookup meta object identified by the provided type identifier
 * and register the proxy endpoint with this entry.
 */
void ddsi_tl_meta_proxy_ref (struct ddsi_domaingv *gv, const type_identifier_t *type_id, const ddsi_guid_t *proxy_ep_guid);

/**
 * Reference the type lookup meta object identifier by the provided type identifier
 * or the provided type object. In case a type object is provided and the type was not
 * yet registered with the type lookup meta object, this field will be set in the meta
 * object.
 */
void ddsi_tl_meta_local_ref (struct ddsi_domaingv *gv, const type_identifier_t *type_id, const struct ddsi_sertype *type);

/**
 * Dereference the type lookup meta object identified by the provided type identifier.
 * The proxy endpoint will be deregistered for this entry.
 */
void ddsi_tl_meta_proxy_unref (struct ddsi_domaingv *gv, const type_identifier_t *type_id, const ddsi_guid_t *proxy_ep_guid);

/**
 * Dereference the type lookup meta object identifier by the provided type identifier
 * or the provided type object.
 */
void ddsi_tl_meta_local_unref (struct ddsi_domaingv *gv, const type_identifier_t *type_id, const struct ddsi_sertype *type);

/**
 * Returns the type lookup meta object for the provided type identifier.
 * The caller of this functions needs to have locked gv->tl_admin_lock
 *
 * @remark The returned object from this function is not refcounted,
 *   its lifetime is at lease the lifetime of the (proxy) endpoints
 *   that are referring to it.
 */
struct tl_meta * ddsi_tl_meta_lookup_locked (struct ddsi_domaingv *gv, const type_identifier_t *type_id);

/**
 * Returns the type lookup meta object for the provided type identifier
 *
 * @remark The returned object from this function is not refcounted,
 *   its lifetime is at lease the lifetime of the (proxy) endpoints
 *   that are referring to it.
 */
struct tl_meta * ddsi_tl_meta_lookup (struct ddsi_domaingv *gv, const type_identifier_t *type_id);

/**
 * For all proxy endpoints registered with the type lookup meta object that is
 * associated with the provided type, this function references the type with
 * these endpoints.
 */
void ddsi_tl_meta_register_with_proxy_endpoints (struct ddsi_domaingv *gv, const struct ddsi_sertype *type);
bool ddsi_tl_request_type (struct ddsi_domaingv * const gv, const type_identifier_t *type_id);
void ddsi_tl_handle_request (struct ddsi_domaingv *gv, struct ddsi_serdata *sample_common);
void ddsi_tl_handle_reply (struct ddsi_domaingv *gv, struct ddsi_serdata *sample_common);

#if defined (__cplusplus)
}
#endif
#endif /* DDSI_INCLUDE_TYPE_DISCOVERY */
#endif /* DDSI_TYPELOOKUP_H */