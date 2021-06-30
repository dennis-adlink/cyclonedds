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

#include "dds/features.h"
#ifdef DDS_HAS_TYPE_DISCOVERY

#include <stdint.h>
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_plist_generic.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"
#include "dds/ddsi/ddsi_xt.h"
#include "dds/ddsi/ddsi_xt_wrap.h"
#include "dds/ddsi/ddsi_list_tmpl.h"


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
struct ddsi_sertype;

#define NOARG
DDSI_LIST_TYPES_TMPL(tlm_proxy_guid_list, ddsi_guid_t, NOARG, 32)
#undef NOARG

extern const ddsrt_avl_treedef_t ddsi_tl_meta_minimal_treedef;
extern const ddsrt_avl_treedef_t ddsi_tl_meta_treedef;

struct tl_meta {
  ddsrt_avl_node_t avl_node_minimal;
  ddsrt_avl_node_t avl_node;
  const char *type_name;
  struct xt_type *xt;                     /* type wrapper */
  const struct ddsi_sertype *sertype;     /* sertype associated with the type identifier, NULL if type is unresolved or not a top-level type */
  seqno_t request_seqno;                  /* sequence number of the last type lookup request message */
  struct tlm_proxy_guid_list proxy_guids; /* administration for proxy endpoints (not proxy topics) that are using this type */
  uint32_t refc;                          /* refcount for this record */
};

/**
 * Reference the type lookup meta object identified by the provided type identifier
 * and register the proxy endpoint with this entry.
 */
struct tl_meta * ddsi_tl_meta_proxy_ref (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id_minimal, const ddsi_typeid_t *type_id, const char *type_name, const ddsi_guid_t *proxy_guid);

/**
 * Reference the type lookup meta object identifier by the provided type identifier
 * or the provided type object. In case a type object is provided and the type was not
 * yet registered with the type lookup meta object, this field will be set in the meta
 * object.
 */
struct tl_meta * ddsi_tl_meta_local_ref (struct ddsi_domaingv *gv, const struct ddsi_sertype *type);

/**
 * Dereference the type lookup meta object identified by the provided type identifier.
 * The proxy endpoint will be deregistered for this entry.
 */
void ddsi_tl_meta_proxy_unref (struct ddsi_domaingv *gv, struct tl_meta *tlm, const ddsi_guid_t *proxy_ep_guid);

/**
 * Dereference the type lookup meta object identifier by the provided type identifier
 * or the provided type object.
 */
void ddsi_tl_meta_local_unref (struct ddsi_domaingv *gv, struct tl_meta *tlm, const struct ddsi_sertype *type);

/**
 * Returns the type lookup meta object for the provided type identifier.
 * The caller of this functions needs to have locked gv->tl_admin_lock
 *
 * @remark The returned object from this function is not refcounted,
 *   its lifetime is at lease the lifetime of the (proxy) endpoints
 *   that are referring to it.
 */
struct tl_meta * ddsi_tl_meta_lookup_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id, const char *type_name);

/**
 * Returns the type lookup meta object for the provided type identifier
 *
 * @remark The returned object from this function is not refcounted,
 *   its lifetime is at lease the lifetime of the (proxy) endpoints
 *   that are referring to it.
 */
struct tl_meta * ddsi_tl_meta_lookup (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id, const char *type_name);

/**
 * For all proxy endpoints registered with the type lookup meta object that is
 * associated with the provided type, this function references the sertype
 * for these endpoints.
 */
void ddsi_tl_meta_register_with_proxy_endpoints (struct ddsi_domaingv *gv, const struct ddsi_sertype *type);

/**
 * Send a type lookup request message in order to request type information for the
 * provided type identifier.
 */
bool ddsi_tl_request_type (struct ddsi_domaingv * const gv, const ddsi_typeid_t *type_id, const char *type_name);

/**
 * Handle an incoming type lookup request message. For all types requested
 * that are known in this node, the serialized sertype is send in a type
 * lookup reply message. In case none of the requested types is known,
 * an empty reply message will be sent.
 */
void ddsi_tl_handle_request (struct ddsi_domaingv *gv, struct ddsi_serdata *sample_common);

/**
 * Handle an incoming type lookup reply message. The sertypes from this
 * reply are registered in the local type administation and referenced
 * from the corresponding proxy endpoints.
 */
void ddsi_tl_handle_reply (struct ddsi_domaingv *gv, struct ddsi_serdata *sample_common);

/**
 * Compares the provided type lookup meta objects.
 */
int ddsi_tl_meta_compare_minimal (const struct tl_meta *a, const struct tl_meta *b);
int ddsi_tl_meta_compare (const struct tl_meta *a, const struct tl_meta *b);

/**
 * Returns a ddsi_typeinfo_t from the provided tl_meta
 */
ddsi_typeinfo_t *ddsi_tl_meta_to_typeinfo (const struct tl_meta *tlm);

#if defined (__cplusplus)
}
#endif
#endif /* DDS_HAS_TYPE_DISCOVERY */
#endif /* DDSI_TYPELOOKUP_H */
