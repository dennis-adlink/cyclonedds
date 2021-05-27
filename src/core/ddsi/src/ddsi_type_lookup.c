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
#include <string.h>
#include <stdlib.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_serdata_pserop.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_plist_generic.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_type_lookup.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_type_identifier.h"
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_misc.h"

const enum pserop typelookup_service_request_ops[] = { XG, Xl, XQ, XK, XSTOP, XSTOP };
size_t typelookup_service_request_nops = sizeof (typelookup_service_request_ops) / sizeof (typelookup_service_request_ops[0]);

const enum pserop typelookup_service_reply_ops[] = { XG, Xl, XQ, XK, XO, XSTOP, XSTOP };
size_t typelookup_service_reply_nops = sizeof (typelookup_service_reply_ops) / sizeof (typelookup_service_reply_ops[0]);

DDSI_LIST_DECLS_TMPL(static, tlm_proxy_guid_list, ddsi_guid_t, ddsrt_attribute_unused)
DDSI_LIST_CODE_TMPL(static, tlm_proxy_guid_list, ddsi_guid_t, nullguid, ddsrt_malloc, ddsrt_free)

static bool tlm_proxy_guid_exists (struct tl_meta *tlm, const ddsi_guid_t *proxy_guid)
{
  struct tlm_proxy_guid_list_iter it;
  for (ddsi_guid_t guid = tlm_proxy_guid_list_iter_first (&tlm->proxy_guids, &it); !is_null_guid (&guid); guid = tlm_proxy_guid_list_iter_next (&it))
  {
    if (guid_eq (&guid, proxy_guid))
      return true;
  }
  return false;
}

static int tlm_proxy_guids_eq (const struct ddsi_guid a, const struct ddsi_guid b)
{
  return guid_eq (&a, &b);
}

int ddsi_tl_meta_compare_minimal (const struct tl_meta *a, const struct tl_meta *b)
{
  int r;
  if ((r = ddsi_typeid_compare (&a->type_id_minimal, &b->type_id_minimal)) != 0)
    return r;
  /* consider types equal if the type name is not filled,
     which can only occur when searching for a type
     because all entries in the administration are required
     to have to type name */
  if (a->type_name == NULL || b->type_name == NULL)
    return 0;
  return strcmp (a->type_name, b->type_name);
}

int ddsi_tl_meta_compare (const struct tl_meta *a, const struct tl_meta *b)
{
  /* don't take the type name into account as it is in the complete
     type object and therefore hashed in the complete type identifier */
  return ddsi_typeid_compare (&a->type_id, &b->type_id);
}

static int tl_meta_compare_minimal_wrap (const void *tlm_a, const void *tlm_b)
{
  return ddsi_tl_meta_compare_minimal (tlm_a, tlm_b);
}

static int tl_meta_compare_wrap (const void *tlm_a, const void *tlm_b)
{
  return ddsi_tl_meta_compare (tlm_a, tlm_b);
}

const ddsrt_avl_treedef_t ddsi_tl_meta_minimal_treedef = DDSRT_AVL_TREEDEF_INITIALIZER_ALLOWDUPS (offsetof (struct tl_meta, avl_node_minimal), 0, tl_meta_compare_minimal_wrap, 0);
const ddsrt_avl_treedef_t ddsi_tl_meta_treedef = DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct tl_meta, avl_node), 0, tl_meta_compare_wrap, 0);

static void tlm_fini (struct tl_meta *tlm)
{
  if (tlm->sertype != NULL)
    ddsi_sertype_unref ((struct ddsi_sertype *) tlm->sertype);
  bool ep_empty = tlm_proxy_guid_list_count (&tlm->proxy_guids) == 0;
  assert (ep_empty);
  (void) ep_empty;
  ddsrt_free (tlm);
}

struct tl_meta * ddsi_tl_meta_lookup_locked (struct ddsi_domaingv *gv, const struct TypeIdentifier *type_id, const char *type_name)
{
  assert (type_id);
  struct tl_meta templ, *tlm = NULL;
  memset (&templ, 0, sizeof (templ));
  if (ddsi_typeid_is_minimal (type_id))
  {
    ddsi_typeid_copy (&templ.type_id_minimal, type_id);
    templ.type_name = type_name;
    tlm = ddsrt_avl_lookup (&ddsi_tl_meta_minimal_treedef, &gv->tl_admin_minimal, &templ);
  }
  else if (ddsi_typeid_is_complete (type_id))
  {
    /* don't include type name in search, as it is part of the
       complete type object and therefore in the hash in the
       type identifier */
    ddsi_typeid_copy (&templ.type_id, type_id);
    tlm = ddsrt_avl_lookup (&ddsi_tl_meta_treedef, &gv->tl_admin, &templ);
  }
  return tlm;
}

struct tl_meta * ddsi_tl_meta_lookup (struct ddsi_domaingv *gv, const struct TypeIdentifier *type_id, const char *type_name)
{
  struct tl_meta *tlm;
  ddsrt_mutex_lock (&gv->tl_admin_lock);
  tlm = ddsi_tl_meta_lookup_locked (gv, type_id, type_name);
  ddsrt_mutex_unlock (&gv->tl_admin_lock);
  return tlm;
}

static struct tl_meta * tlm_ref_impl (struct ddsi_domaingv *gv, const struct TypeIdentifier *type_id, const char *type_name, const struct ddsi_sertype *type, const ddsi_guid_t *proxy_guid)
{
  bool resolved = false;
  assert (type_id || type);
  GVTRACE (" ref tl_meta");
  const struct TypeIdentifier *tid = NULL, *tid_min = NULL;
  struct tl_meta *tlm = NULL;
  const char *tname;

  ddsrt_mutex_lock (&gv->tl_admin_lock);
  if (type != NULL)
  {
    GVTRACE (" sertype %p", type);
    tname = type->type_name;
    if (type->tlm != NULL)
    {
      tlm = type->tlm;
      tid = &type->tlm->type_id;
      tid_min = &type->tlm->type_id_minimal;
    }
    else
    {
      tid = ddsi_sertype_typeid (type, false);
      tid_min = ddsi_sertype_typeid (type, true);
    }
    if (ddsi_typeid_is_none (tid))
    {
      if (!ddsi_typeid_is_none (tid_min))
        tid = tid_min;
      else
      {
        ddsrt_mutex_unlock (&gv->tl_admin_lock);
        return NULL;
      }
    }
  }
  else
  {
    assert (type_name);
    tname = type_name;
    if (ddsi_typeid_is_complete (type_id))
      tid = type_id;
    else if (ddsi_typeid_is_minimal (type_id))
      tid_min = type_id;
  }

  if (!tlm
    && (!tid || !(tlm = ddsi_tl_meta_lookup_locked (gv, tid, tname)))
    && (!tid_min || !(tlm = ddsi_tl_meta_lookup_locked (gv, tid_min, tname))))
  {
    tlm = ddsrt_calloc (1, sizeof (*tlm));
    tlm->state = TL_META_NEW;
    tlm->type_name = ddsrt_strdup (tname);
    if (tid)
    {
      ddsi_typeid_copy (&tlm->type_id, tid);
      ddsrt_avl_insert (&ddsi_tl_meta_treedef, &gv->tl_admin, tlm);
    }
    if (tid_min)
    {
      ddsi_typeid_copy (&tlm->type_id_minimal, tid_min);
      ddsrt_avl_insert (&ddsi_tl_meta_minimal_treedef, &gv->tl_admin_minimal, tlm);
    }
    GVTRACE (" new %p", tlm);
  }
  else
  {
    assert (tlm);
    assert (!strcmp (tlm->type_name, tname));
  }
  if (proxy_guid != NULL)
  {
    if (!tlm_proxy_guid_exists (tlm, proxy_guid))
    {
      tlm_proxy_guid_list_insert (&tlm->proxy_guids, *proxy_guid);
      GVTRACE (" add ep "PGUIDFMT, PGUID (*proxy_guid));
    }
  }
  if (tlm->sertype == NULL && type != NULL)
  {
    tlm->sertype = ddsi_sertype_ref (type);
    tlm->state = TL_META_RESOLVED;
    GVTRACE (" resolved");
    resolved = true;
  }
  if (tlm->xt == NULL)
  {
    struct TypeObject *tobj = ddsi_sertype_typeobj (tlm->sertype, ddsi_typeid_is_minimal (tid), NULL);
    tlm->xt = ddsi_xt_type_init (tid, tobj); // tobj can be null, in that case only the type identifier will be added to xt
    if (tobj != NULL)
      ddsrt_free (tobj);
  }
  else
  {
    if (ddsi_typeid_is_complete (tid) && !tlm->xt->has_complete_obj)
    {
      struct TypeObject *tobj = ddsi_sertype_typeobj (tlm->sertype, false, NULL);
      ddsi_xt_type_add (tlm->xt, tid, tobj);
      if (tobj)
        ddsrt_free (tobj);
    }
  }
  if (tid_min != NULL && tid_min != tid && !tlm->xt->has_minimal_id)
  {
    struct TypeObject *tobj_min = ddsi_sertype_typeobj (tlm->sertype, true, NULL);
    ddsi_xt_type_add (tlm->xt, tid_min, tobj_min);
    if (tobj_min)
      ddsrt_free (tobj_min);
  }

  tlm->refc++;
  GVTRACE (" state %d refc %u\n", tlm->state, tlm->refc);

  if (resolved)
    ddsrt_cond_broadcast (&gv->tl_resolved_cond);

  ddsrt_mutex_unlock (&gv->tl_admin_lock);
  return tlm;
}

struct tl_meta * ddsi_tl_meta_local_ref (struct ddsi_domaingv *gv, const struct ddsi_sertype *type)
{
  assert (type != NULL);
  return tlm_ref_impl (gv, NULL, NULL, type, NULL);
}

struct tl_meta * ddsi_tl_meta_proxy_ref (struct ddsi_domaingv *gv, const struct TypeIdentifier *type_id, const char *type_name, const ddsi_guid_t *proxy_guid)
{
  assert (type_id != NULL);
  assert (type_name != NULL);
  return tlm_ref_impl (gv, type_id, type_name, NULL, proxy_guid);
}

static void tlm_unref_impl_locked (struct ddsi_domaingv *gv, struct tl_meta *tlm, const ddsi_guid_t *proxy_guid)
{
  assert (tlm->refc > 0);
  if (proxy_guid != NULL)
  {
    tlm_proxy_guid_list_remove (&tlm->proxy_guids, *proxy_guid, tlm_proxy_guids_eq);
    GVTRACE (" remove ep "PGUIDFMT, PGUID (*proxy_guid));
  }
  if (--tlm->refc == 0)
  {
    GVTRACE (" remove tl_meta\n");
    ddsrt_avl_delete (&ddsi_tl_meta_minimal_treedef, &gv->tl_admin_minimal, tlm);
    ddsrt_avl_delete (&ddsi_tl_meta_treedef, &gv->tl_admin, tlm);
    tlm_fini (tlm);
  }
}

static void tlm_unref_impl (struct ddsi_domaingv *gv, const struct tl_meta *tlm, const struct ddsi_sertype *type, const ddsi_guid_t *proxy_guid)
{
  assert (tlm || type);
  GVTRACE ("unref tl_meta");
  struct TypeIdentifier *tid = NULL;
  if (tlm == NULL)
  {
    GVTRACE (" sertype %p", type);
    if (type->tlm == NULL)
    {
      GVTRACE (" no tlm\n");
      return;
    }
    if (!ddsi_typeid_is_none (&type->tlm->type_id))
      tid = &type->tlm->type_id;
    else if (!ddsi_typeid_is_none (&type->tlm->type_id_minimal))
      tid = &type->tlm->type_id_minimal;
    else
    {
      GVTRACE (" no typeid\n");
      return;
    }
  }

  ddsrt_mutex_lock (&gv->tl_admin_lock);
  const char *tname = tlm ? tlm->type_name : type->type_name;
  struct tl_meta *tlm1 = tlm ? (struct tl_meta *) tlm : ddsi_tl_meta_lookup_locked (gv, tid, tname);
  assert (tlm1 != NULL);
  tlm_unref_impl_locked (gv, tlm1, proxy_guid);
  ddsrt_mutex_unlock (&gv->tl_admin_lock);
  if (tlm == NULL)
    ddsrt_free (tid);
  GVTRACE ("\n");
}

void ddsi_tl_meta_proxy_unref (struct ddsi_domaingv *gv, const struct tl_meta *tlm, const ddsi_guid_t *proxy_guid)
{
  if (tlm != NULL)
    tlm_unref_impl (gv, tlm, NULL, proxy_guid);
}

void ddsi_tl_meta_local_unref (struct ddsi_domaingv *gv, const struct tl_meta *tlm, const struct ddsi_sertype *type)
{
  if (tlm != NULL || type != NULL)
    tlm_unref_impl (gv, tlm, type, NULL);
}

static struct writer *get_typelookup_writer (const struct ddsi_domaingv *gv, uint32_t wr_eid)
{
  struct participant *pp;
  struct writer *wr = NULL;
  struct entidx_enum_participant est;
  thread_state_awake (lookup_thread_state (), gv);
  entidx_enum_participant_init (&est, gv->entity_index);
  while (wr == NULL && (pp = entidx_enum_participant_next (&est)) != NULL)
    wr = get_builtin_writer (pp, wr_eid);
  entidx_enum_participant_fini (&est);
  thread_state_asleep (lookup_thread_state ());
  return wr;
}

bool ddsi_tl_request_type (struct ddsi_domaingv * const gv, const struct TypeIdentifier *type_id, const char *type_name)
{
  ddsrt_mutex_lock (&gv->tl_admin_lock);
  struct tl_meta *tlm = ddsi_tl_meta_lookup_locked (gv, type_id, type_name);
  GVTRACE ("tl-req ");
  if (!tlm || tlm->state != TL_META_NEW)
  {
    // type lookup is pending or the type is already resolved, so we'll return true
    // to indicate that the type request is done (or not required)
    // FIXME: GVTRACE ("state not-new (%u) for "PTYPEIDFMT"\n", tlm->state, PTYPEID (*type_id));
    ddsrt_mutex_unlock (&gv->tl_admin_lock);
    return true;
  }

  struct writer *wr = get_typelookup_writer (gv, NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER);
  if (wr == NULL)
  {
    GVTRACE ("no pp found with tl request writer");
    ddsrt_mutex_unlock (&gv->tl_admin_lock);
    return false;
  }

  type_lookup_request_t request;
  memcpy (&request.writer_guid, &wr->e.guid, sizeof (wr->e.guid));
  request.sequence_number = ++tlm->request_seqno;
  request.type_ids.n = 1;
  request.type_ids.type_ids = &tlm->type_id;

  struct ddsi_serdata *serdata = ddsi_serdata_from_sample (gv->tl_svc_request_type, SDK_DATA, &request);
  serdata->timestamp = ddsrt_time_wallclock ();

  tlm->state = TL_META_REQUESTED;
  ddsrt_mutex_unlock (&gv->tl_admin_lock);

  thread_state_awake (lookup_thread_state (), gv);
  // FIXME: GVTRACE ("wr "PGUIDFMT" typeid "PTYPEIDFMT"\n", PGUID (wr->e.guid), PTYPEID(*type_id));
  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  write_sample_gc (lookup_thread_state (), NULL, wr, serdata, tk);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
  thread_state_asleep (lookup_thread_state ());

  return true;
}

static void write_typelookup_reply (struct writer *wr, seqno_t seqno, struct TypeIdentifierTypeObjectPairSeq *types)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  type_lookup_reply_t reply;

  GVTRACE (" tl-reply ");
  memcpy (&reply.writer_guid, &wr->e.guid, sizeof (wr->e.guid));
  reply.sequence_number = seqno;
  reply.types.n = types->n;
  reply.types.types = types->types;
  struct ddsi_serdata *serdata = ddsi_serdata_from_sample (gv->tl_svc_reply_type, SDK_DATA, &reply);
  serdata->timestamp = ddsrt_time_wallclock ();

  GVTRACE ("wr "PGUIDFMT"\n", PGUID (wr->e.guid));
  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  write_sample_gc (lookup_thread_state (), NULL, wr, serdata, tk);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
}

void ddsi_tl_handle_request (struct ddsi_domaingv *gv, struct ddsi_serdata *sample_common)
{
  assert (!(sample_common->statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER)));
  const struct ddsi_serdata_pserop *sample = (const struct ddsi_serdata_pserop *) sample_common;
  const type_lookup_request_t *req = sample->sample;
  GVTRACE (" handle-tl-req wr "PGUIDFMT " seqnr %"PRIi64" ntypeids %"PRIu32, PGUID (req->writer_guid), req->sequence_number, req->type_ids.n);

  ddsrt_mutex_lock (&gv->tl_admin_lock);
  struct TypeIdentifierTypeObjectPairSeq types = { 0, NULL };
  for (uint32_t n = 0; n < req->type_ids.n; n++)
  {
    struct TypeIdentifier *tid = &req->type_ids.type_ids[n];
    GVTRACE (" type "PTYPEIDFMT, PTYPEID (*tid));
    struct tl_meta *tlm = ddsi_tl_meta_lookup_locked (gv, tid, NULL); /* any type name */
    if (tlm != NULL && tlm->state == TL_META_RESOLVED)
    {
      // FIXME
      // types.types = ddsrt_realloc (types.types, (types.n + 1) * sizeof (*types.types));
      // types.types[types.n].type_identifier = tlm->type_id;
      // size_t sz;
      // if (!ddsi_sertype_serialize (tlm->sertype, &sz, &types.types[types.n].type_object.value))
      // {
      //   GVTRACE (" serialize failed");
      // }
      // else
      // {
      //   assert (sz <= UINT32_MAX);
      //   types.n++;
      //   types.types[types.n - 1].type_object.length = (uint32_t) sz;
      //   GVTRACE (" found");
      // }
    }
  }
  ddsrt_mutex_unlock (&gv->tl_admin_lock);

  struct writer *wr = get_typelookup_writer (gv, NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER);
  if (wr != NULL)
    write_typelookup_reply (wr, req->sequence_number, &types);
  else
    GVTRACE (" no tl-reply writer");

  // FIXME
  // for (uint32_t n = 0; n < types.n; n++)
  //   ddsrt_free (types.types[n].type_object.value);
  ddsrt_free (types.types);
}

static void tlm_register_with_proxy_endpoints_locked (struct ddsi_domaingv *gv, struct tl_meta *tlm)
{
  assert (tlm);
  thread_state_awake (lookup_thread_state (), gv);

  struct tlm_proxy_guid_list_iter proxy_guid_it;
  for (ddsi_guid_t guid = tlm_proxy_guid_list_iter_first (&tlm->proxy_guids, &proxy_guid_it); !is_null_guid (&guid); guid = tlm_proxy_guid_list_iter_next (&proxy_guid_it))
  {
#ifdef DDS_HAS_TOPIC_DISCOVERY
    /* For proxy topics the type is not registered (in its topic definition),
       becauses (besides that it causes some locking-order trouble) it would
       only be used when searching for topics and at that point it can easily
       be retrieved using the type identifier via a lookup in the type_lookup
       administration. */
    assert (!is_topic_entityid (guid.entityid));
#endif
    struct entity_common *ec;
    if ((ec = entidx_lookup_guid_untyped (gv->entity_index, &guid)) != NULL)
    {
      assert (ec->kind == EK_PROXY_READER || ec->kind == EK_PROXY_WRITER);
      struct generic_proxy_endpoint *gpe = (struct generic_proxy_endpoint *) ec;
      ddsrt_mutex_lock (&gpe->e.lock);
      if (gpe->c.type == NULL)
        gpe->c.type = ddsi_sertype_ref (tlm->sertype);
      ddsrt_mutex_unlock (&gpe->e.lock);
    }
  }
  thread_state_asleep (lookup_thread_state ());
}

void ddsi_tl_meta_register_with_proxy_endpoints (struct ddsi_domaingv *gv, const struct ddsi_sertype *type)
{
  if (type->tlm != NULL && !ddsi_typeid_is_none (&type->tlm->type_id))
  {
    ddsrt_mutex_lock (&gv->tl_admin_lock);
    struct tl_meta *tlm = ddsi_tl_meta_lookup_locked (gv, &type->tlm->type_id, type->type_name);
    tlm_register_with_proxy_endpoints_locked (gv, tlm);
    ddsrt_mutex_unlock (&gv->tl_admin_lock);
  }
}

void ddsi_tl_handle_reply (struct ddsi_domaingv *gv, struct ddsi_serdata *sample_common)
{
  struct generic_proxy_endpoint **gpe_match_upd = NULL;
  uint32_t n = 0, n_match_upd = 0;
  assert (!(sample_common->statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER)));
  const struct ddsi_serdata_pserop *sample = (const struct ddsi_serdata_pserop *) sample_common;
  const type_lookup_reply_t *reply = sample->sample;
  // struct ddsi_sertype_default *st = NULL;
  bool resolved = false;

  ddsrt_mutex_lock (&gv->tl_admin_lock);
  GVTRACE ("handle-tl-reply wr "PGUIDFMT " seqnr %"PRIi64" ntypeids %"PRIu32" ", PGUID (reply->writer_guid), reply->sequence_number, reply->types.n);
  while (n < reply->types.n)
  {
    TypeIdentifierTypeObjectPair_t r = reply->types.types[n];
    // FIXME: in case of minimal type id resolved, update all records with this id if more exist?
    // FIXME: in case tlm record exists with minimal type info, update if complete type info received
    struct tl_meta *tlm = ddsi_tl_meta_lookup_locked (gv, r.type_identifier, NULL);
    if (tlm != NULL && tlm->state == TL_META_REQUESTED && tlm_proxy_guid_list_count (&tlm->proxy_guids) > 0)
    {
      bool sertype_new = false;
      GVTRACE (" type "PTYPEIDFMT, PTYPEID (*r.type_identifier));
      // FIXME
      // st = ddsrt_malloc (sizeof (*st));
      // assume ddsi_sertype_ops_default at this point, as it should be serialized with the sertype_default serializer
      // if (!ddsi_sertype_deserialize (gv, &st->c, &ddsi_sertype_ops_default, r.type_object.length, r.type_object.value))
      // {
      //   GVTRACE (" deser failed\n");
      //   ddsrt_free (st);
      //   n++;
      //   continue;
      // }
      // ddsrt_mutex_lock (&gv->sertypes_lock);
      // struct ddsi_sertype *existing_sertype = ddsi_sertype_lookup_locked (gv, &st->c);
      // if (existing_sertype == NULL)
      // {
      //   ddsi_sertype_register_locked (gv, &st->c);
      //   sertype_new = true;
      // }
      // ddsi_sertype_unref_locked (gv, &st->c); // unref because both init_from_ser and sertype_lookup/register refcounts the type
      // ddsrt_mutex_unlock (&gv->sertypes_lock);
      // tlm->sertype = &st->c; // refcounted by sertype_register/lookup

      tlm->state = TL_META_RESOLVED;
      if (sertype_new)
      {
        gpe_match_upd = ddsrt_realloc (gpe_match_upd, (n_match_upd + tlm_proxy_guid_list_count (&tlm->proxy_guids)) * sizeof (*gpe_match_upd));
        struct tlm_proxy_guid_list_iter it;
        for (ddsi_guid_t guid = tlm_proxy_guid_list_iter_first (&tlm->proxy_guids, &it); !is_null_guid (&guid); guid = tlm_proxy_guid_list_iter_next (&it))
        {
          if (!is_topic_entityid (guid.entityid))
          {
            struct entity_common *ec = entidx_lookup_guid_untyped (gv->entity_index, &guid);
            if (ec != NULL)
            {
              assert (ec->kind == EK_PROXY_READER || ec->kind == EK_PROXY_WRITER);
              gpe_match_upd[n_match_upd++] = (struct generic_proxy_endpoint *) ec;
            }
          }
        }
        tlm_register_with_proxy_endpoints_locked (gv, tlm);
      }
      resolved = true;
    }
    n++;
  }
  GVTRACE ("\n");
  if (resolved)
    ddsrt_cond_broadcast (&gv->tl_resolved_cond);
  ddsrt_mutex_unlock (&gv->tl_admin_lock);

  if (gpe_match_upd != NULL)
  {
    for (uint32_t e = 0; e < n_match_upd; e++)
      update_proxy_endpoint_matching (gv, gpe_match_upd[e]);
    ddsrt_free (gpe_match_upd);
  }
}

struct TypeInformation *ddsi_tl_meta_to_typeinfo (const struct tl_meta *tlm)
{
  struct TypeInformation *ti = ddsrt_calloc (1, sizeof (*ti));
  // FIXME: implement
  (void) tlm;
  return ti;
}
