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
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds__topic.h"
#include "dds__listener.h"
#include "dds__participant.h"
#include "dds__init.h"
#include "dds__domain.h"
#include "dds__get_status.h"
#include "dds__qos.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds__serdata_builtintopic.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_topic)

#define DDS_TOPIC_STATUS_MASK                                    \
                        (DDS_INCONSISTENT_TOPIC_STATUS)

struct topic_sertype_node {
  ddsrt_avl_node_t avlnode;
  uint32_t refc;
  const struct ddsi_sertype *st;
};

static bool is_valid_name (const char *name) ddsrt_nonnull_all;

static bool is_valid_name (const char *name)
{
  /* DDS Spec:
   *  |  TOPICNAME - A topic name is an identifier for a topic, and is defined as any series of characters
   *  |     'a', ..., 'z',
   *  |     'A', ..., 'Z',
   *  |     '0', ..., '9',
   *  |     '-' but may not start with a digit.
   * It is considered that '-' is an error in the spec and should say '_'. So, that's what we'll check for.
   *  |     '/' got added for ROS2
   */
  if (name[0] == '\0' || isdigit ((unsigned char) name[0]))
    return false;
  for (size_t i = 0; name[i]; i++)
    if (!(isalnum ((unsigned char) name[i]) || name[i] == '_' || name[i] == '/'))
      return false;
  return true;
}

static dds_return_t dds_topic_status_validate (uint32_t mask)
{
  return (mask & ~DDS_TOPIC_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

/*
  Topic status change callback handler. Supports INCONSISTENT_TOPIC
  status (only defined status on a topic).  Irrelevant until inconsistent topic
  definitions can be detected, so until topic discovery is added.
*/
#if 0
static void dds_topic_status_cb (struct dds_topic *tp)
{
  struct dds_listener const * const lst = &tp->m_entity.m_listener;

  ddsrt_mutex_lock (&tp->m_entity.m_observers_lock);
  while (tp->m_entity.m_cb_count > 0)
    ddsrt_cond_wait (&tp->m_entity.m_observers_cond, &tp->m_entity.m_observers_lock);
  tp->m_entity.m_cb_count++;

  tp->m_inconsistent_topic_status.total_count++;
  tp->m_inconsistent_topic_status.total_count_change++;
  if (lst->on_inconsistent_topic)
  {
    ddsrt_mutex_unlock (&tp->m_entity.m_observers_lock);
    dds_entity_invoke_listener (&tp->m_entity, DDS_INCONSISTENT_TOPIC_STATUS_ID, &tp->m_inconsistent_topic_status);
    ddsrt_mutex_lock (&tp->m_entity.m_observers_lock);
    tp->m_inconsistent_topic_status.total_count_change = 0;
  }

  dds_entity_status_set (&tp->m_entity, DDS_INCONSISTENT_TOPIC_STATUS);
  tp->m_entity.m_cb_count--;
  ddsrt_cond_broadcast (&tp->m_entity.m_observers_cond);
  ddsrt_mutex_unlock (&tp->m_entity.m_observers_lock);
}
#endif

dds_return_t dds_topic_pin (dds_entity_t handle, struct dds_topic **tp)
{
  struct dds_entity *e;
  dds_return_t ret;
  if ((ret = dds_entity_pin (handle, &e)) < 0)
    return ret;
  if (dds_entity_kind (e) != DDS_KIND_TOPIC)
  {
    dds_entity_unpin (e);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  }
  *tp = (struct dds_topic *) e;
  return DDS_RETCODE_OK;
}

void dds_topic_unpin (struct dds_topic *tp)
{
  dds_entity_unpin (&tp->m_entity);
}

void dds_topic_defer_set_qos (struct dds_topic *tp)
{
  struct dds_ktopic * const ktp = tp->m_ktopic;
  struct dds_participant * const pp = dds_entity_participant (&tp->m_entity);
  ddsrt_mutex_lock (&pp->m_entity.m_mutex);
  ++ktp->defer_set_qos;
  ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
}

void dds_topic_allow_set_qos (struct dds_topic *tp)
{
  struct dds_ktopic * const ktp = tp->m_ktopic;
  struct dds_participant * const pp = dds_entity_participant (&tp->m_entity);
  ddsrt_mutex_lock (&pp->m_entity.m_mutex);
  assert (ktp->defer_set_qos > 0);
  if (--ktp->defer_set_qos == 0)
    ddsrt_cond_broadcast (&pp->m_entity.m_cond);
  ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
}

static dds_return_t dds_topic_delete (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_topic_delete (dds_entity *e)
{
  struct dds_topic * const tp = (dds_topic *) e;
  struct dds_ktopic * const ktp = tp->m_ktopic;
  assert (dds_entity_kind (e->m_parent) == DDS_KIND_PARTICIPANT);
  dds_participant * const pp = (dds_participant *) e->m_parent;
#ifdef DDSI_INCLUDE_TYPE_DISCOVERY
  ddsi_tl_meta_local_unref (&e->m_domain->gv, NULL, tp->m_stype);
#endif
  ddsrt_free (tp->m_name);

  ddsrt_mutex_lock (&pp->m_entity.m_mutex);

  // unref
  struct ktopic_type_guid templ;
  type_identifier_t *tid = ddsi_typeid_from_sertype (tp->m_stype);
  memset (&templ, 0, sizeof (templ));
  templ.type_id = tid;
  struct ktopic_type_guid *m = ddsrt_hh_lookup (ktp->topic_guid_map, &templ);
  assert (m != NULL);
  ddsrt_free (tid);
  if (--m->refc == 0)
  {
    ddsrt_hh_remove (ktp->topic_guid_map, m);
    thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
    (void) delete_topic (&e->m_domain->gv, &m->guid);
    thread_state_asleep (lookup_thread_state ());
    ddsrt_free ((type_identifier_t *) m->type_id);
    ddsrt_free (m);
  }

  // unref ktopic and delete if last ref
  if (--ktp->refc == 0)
  {
    ddsrt_avl_delete (&participant_ktopics_treedef, &pp->m_ktopics, ktp);
    dds_delete_qos (ktp->qos);
    ddsrt_free (ktp->name);
    ddsrt_free (ktp->type_name);
    ddsrt_hh_free (ktp->topic_guid_map);
    dds_free (ktp);
  }

  ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
  ddsi_sertype_unref (tp->m_stype);
  return DDS_RETCODE_OK;
}

static dds_return_t dds_topic_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* We never actually set the qos of a struct dds_topic and really shouldn't be here,
     but the code to check whether set_qos is supported uses the entity's qos_set
     function as a proxy.  */
  (void) e; (void) qos; (void) enabled;
  return DDS_RETCODE_OK;
}

static bool dupdef_qos_ok (const dds_qos_t *qos, const dds_ktopic *ktp)
{
  if ((qos == NULL) != (ktp->qos == NULL))
    return false;
  else if (qos == NULL)
    return true;
  else
    return dds_qos_equal (ktp->qos, qos);
}

const struct dds_entity_deriver dds_entity_deriver_topic = {
  .interrupt = dds_entity_deriver_dummy_interrupt,
  .close = dds_entity_deriver_dummy_close,
  .delete = dds_topic_delete,
  .set_qos = dds_topic_qos_set,
  .validate_status = dds_topic_status_validate
};

/**
* @brief Checks whether a ktopic with the same name exists in the participant,
* and if so, whether it's QoS matches or not.
*
* The set of ktopics is stored in the participant, protected by the participant's
* mutex and the internal state of these ktopics (including the QoS) is also
* protected by that mutex.
*
* @param[out] ktp_out    matching ktopic if call was successful, or NULL if no
*                        ktopic with this name exists
* @param[in]  pp         pinned & locked participant
* @param[in]  name       topic name to look for
* @param[in]  type_name  type name the topic must have
* @param[in]  new_qos    QoS for the new topic (can be NULL)
*
* @returns success + ktopic, success + NULL or error.
*
* @retval DDS_RETCODE_OK
*             ktp_out is either NULL (first attempt at creating this topic), or
*             the matching ktopic entity
* @retval DDS_RETCODE_INCONSISTENT_POLICY
*             a ktopic exists with differing QoS
* @retval DDS_RETCODE_PRECONDITION_NOT_MET
*             a ktopic exists with a different type name
*/
static dds_return_t lookup_and_check_ktopic (struct dds_ktopic **ktp_out, dds_participant *pp, const char *name, const char *type_name, const dds_qos_t *new_qos)
{
  struct ddsi_domaingv * const gv = &pp->m_entity.m_domain->gv;
  struct dds_ktopic *ktp;
  if ((ktp = *ktp_out = ddsrt_avl_lookup (&participant_ktopics_treedef, &pp->m_ktopics, name)) == NULL)
  {
    GVTRACE ("lookup_and_check_ktopic_may_unlock_pp: no such ktopic\n");
    return DDS_RETCODE_OK;
  }
  else if (strcmp (ktp->type_name, type_name) != 0)
  {
    GVTRACE ("lookup_and_check_ktopic_may_unlock_pp: ktp %p typename %s mismatch\n", (void *) ktp, ktp->type_name);
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }
  else if (!dupdef_qos_ok (new_qos, ktp))
  {
    GVTRACE ("lookup_and_check_ktopic_may_unlock_pp: ktp %p qos mismatch\n", (void *) ktp);
    return DDS_RETCODE_INCONSISTENT_POLICY;
  }
  else
  {
    GVTRACE ("lookup_and_check_ktopic_may_unlock_pp: ktp %p reuse\n", (void *) ktp);
    return DDS_RETCODE_OK;
  }
}

static dds_entity_t create_topic_pp_locked (struct dds_participant *pp, struct dds_ktopic *ktp, bool implicit, const char *topic_name, struct ddsi_sertype *sertype_registered, const dds_listener_t *listener, const ddsi_plist_t *sedp_plist)
{
  (void) sedp_plist;
  dds_entity_t hdl;
  dds_topic *tp = dds_alloc (sizeof (*tp));
  hdl = dds_entity_init (&tp->m_entity, &pp->m_entity, DDS_KIND_TOPIC, implicit, NULL, listener, DDS_TOPIC_STATUS_MASK);
  tp->m_entity.m_iid = ddsi_iid_gen ();
  dds_entity_register_child (&pp->m_entity, &tp->m_entity);
  tp->m_ktopic = ktp;
  tp->m_name = ddsrt_strdup (topic_name);
  tp->m_stype = sertype_registered;
  dds_entity_init_complete (&tp->m_entity);
  return hdl;
}

static int ktopic_type_guid_equal (const void *ktp_guid_a, const void *ktp_guid_b)
{
  struct ktopic_type_guid *a = (struct ktopic_type_guid *) ktp_guid_a;
  struct ktopic_type_guid *b = (struct ktopic_type_guid *) ktp_guid_b;
  return ddsi_typeid_equal (a->type_id, b->type_id);
}

static uint32_t ktopic_type_guid_hash (const void *ktp_guid)
{
  struct ktopic_type_guid *x = (struct ktopic_type_guid *)ktp_guid;
  return (uint32_t) x->type_id;
}

dds_entity_t dds_create_topic_impl (dds_entity_t participant, const char * name, struct ddsi_sertype **sertype, const dds_qos_t *qos, const dds_listener_t *listener, const ddsi_plist_t *sedp_plist)
{
  dds_return_t rc;
  dds_participant *pp;
  dds_qos_t *new_qos = NULL;
  dds_entity_t hdl;
  struct ddsi_sertype *sertype_registered;

  if (sertype == NULL || *sertype == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  {
    dds_entity *par_ent;
    if ((rc = dds_entity_pin (participant, &par_ent)) < 0)
      return rc;
    if (dds_entity_kind (par_ent) != DDS_KIND_PARTICIPANT)
    {
      dds_entity_unpin (par_ent);
      return DDS_RETCODE_ILLEGAL_OPERATION;
    }
    pp = (struct dds_participant *) par_ent;
  }

  new_qos = dds_create_qos ();
  if (qos)
    ddsi_xqos_mergein_missing (new_qos, qos, DDS_TOPIC_QOS_MASK);
  /* One would expect this:
   *
   *   ddsi_xqos_mergein_missing (new_qos, &gv.default_xqos_tp, ~(uint64_t)0);
   *
   * but the crazy defaults of the DDS specification has a default setting
   * for reliability that is dependent on the entity type: readers and
   * topics default to best-effort, but writers to reliable.
   *
   * Leaving the topic QoS sparse means a default-default topic QoS of
   * best-effort will do "the right thing" and let a writer still default to
   * reliable ... (and keep behaviour unchanged) */
  struct ddsi_domaingv * const gv = &pp->m_entity.m_domain->gv;
  if ((rc = ddsi_xqos_valid (&gv->logconfig, new_qos)) != DDS_RETCODE_OK)
    goto error;

  if (!q_omg_security_check_create_topic (&pp->m_entity.m_domain->gv, &pp->m_entity.m_guid, name, new_qos))
  {
    rc = DDS_RETCODE_NOT_ALLOWED_BY_SECURITY;
    goto error;
  }

  /* See if we're allowed to create the topic; ktp is returned pinned & locked
     so we can be sure it doesn't disappear and its QoS can't change */
  GVTRACE ("dds_create_topic_generic (pp %p "PGUIDFMT" sertype %p reg?%s refc %"PRIu32" %s/%s)\n",
           (void *) pp, PGUID (pp->m_entity.m_guid), (void *) (*sertype), (*sertype)->registered ? "yes" : "no",
           (*sertype)->refc, name, (*sertype)->type_name);
  ddsrt_mutex_lock (&pp->m_entity.m_mutex);
  struct dds_ktopic *ktp;
  if ((rc = lookup_and_check_ktopic (&ktp, pp, name, (*sertype)->type_name, new_qos)) != DDS_RETCODE_OK)
  {
    GVTRACE ("dds_create_topic_generic: failed after compatibility check: %s\n", dds_strretcode (rc));
    ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
    goto error;
  }

  /* Create a ktopic if it doesn't exist yet, else reference existing one and delete the
     unneeded "new_qos". */
  if (ktp == NULL)
  {
    ktp = dds_alloc (sizeof (*ktp));
    ktp->refc = 1;
    ktp->defer_set_qos = 0;
    ktp->qos = new_qos;
    ktp->name = ddsrt_strdup (name);
    /* have to copy these because the ktopic can outlast any specific sertype */
    ktp->type_name = ddsrt_strdup ((*sertype)->type_name);
    ktp->topic_guid_map = ddsrt_hh_new (1, ktopic_type_guid_hash, ktopic_type_guid_equal);

    ddsrt_avl_insert (&participant_ktopics_treedef, &pp->m_ktopics, ktp);
    GVTRACE ("create_and_lock_ktopic: ktp %p\n", (void *) ktp);
  }
  else
  {
    ktp->refc++;
    dds_delete_qos (new_qos);
  }

  /* sertype: re-use a previously registered one if possible, else register this one */
  {
    ddsrt_mutex_lock (&gv->sertypes_lock);
    if ((sertype_registered = ddsi_sertype_lookup_locked (gv, *sertype)) != NULL)
      GVTRACE ("dds_create_topic_generic: reuse sertype %p\n", (void *) sertype_registered);
    else
    {
      GVTRACE ("dds_create_topic_generic: register new sertype %p\n", (void *) (*sertype));
      ddsi_sertype_register_locked (*sertype);
      sertype_registered = *sertype;
    }
    ddsrt_mutex_unlock (&gv->sertypes_lock);
  }

  /* Create topic referencing ktopic & sertype_registered */
  /* FIXME: setting "implicit" based on sertype->ops is a hack */
  hdl = create_topic_pp_locked (pp, ktp, (sertype_registered->ops == &ddsi_sertype_ops_builtintopic), name, sertype_registered, listener, sedp_plist);
  ddsi_sertype_unref (*sertype);
  *sertype = sertype_registered;

  /* register ktopic-sertype meta-data entry */
  struct ktopic_type_guid templ, *m;
  type_identifier_t *tid = ddsi_typeid_from_sertype (sertype_registered);
  memset (&templ, 0, sizeof (templ));
  templ.type_id = tid;
  if ((m = ddsrt_hh_lookup (ktp->topic_guid_map, &templ)) == NULL)
  {
    /* register new */
    thread_state_awake (lookup_thread_state (), gv);
    const struct ddsi_guid * ppguid = dds_entity_participant_guid (&pp->m_entity);
    struct participant * pp_ddsi = entidx_lookup_participant_guid (gv->entity_index, ppguid);

    m = ddsrt_malloc (sizeof (*m));
    m->type_id = tid;
    m->refc = 1;
    rc = new_topic (&m->tp, &m->guid, pp_ddsi, ktp->name, sertype_registered, ktp->qos);
    assert (rc == DDS_RETCODE_OK); /* FIXME: can be out-of-resources at the very least */
    ddsrt_hh_add (ktp->topic_guid_map, m);
    thread_state_asleep (lookup_thread_state ());
  }
  else
  {
    /* refc existing */
    m->refc++;
    ddsrt_free (tid);
  }

  ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
#ifdef DDSI_INCLUDE_TYPE_DISCOVERY
  ddsi_tl_meta_local_ref (gv, NULL, sertype_registered);
  ddsi_tl_meta_register_with_proxy_endpoints (gv, sertype_registered);
#endif
  dds_entity_unpin (&pp->m_entity);
  GVTRACE ("dds_create_topic_generic: new topic %"PRId32"\n", hdl);
  return hdl;

 error:
  dds_entity_unpin (&pp->m_entity);
  dds_delete_qos (new_qos);
  return rc;
}

dds_entity_t dds_create_topic_generic (dds_entity_t participant, const char *name, struct ddsi_sertype **sertype, const dds_qos_t *qos, const dds_listener_t *listener, const ddsi_plist_t *sedp_plist)
{
  if (sertype == NULL || *sertype == NULL || name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  if (!strncmp(name, "DCPS", 4))
    return DDS_RETCODE_BAD_PARAMETER;
  return dds_create_topic_impl (participant, name, sertype, qos, listener, sedp_plist);
}

dds_entity_t dds_create_topic_arbitrary (dds_entity_t participant, const char *name, struct ddsi_sertype *sertype, const dds_qos_t *qos, const dds_listener_t *listener, const ddsi_plist_t *sedp_plist)
{
  if (sertype == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_entity_t ret;
  struct ddsi_sertype *st = sertype;
  ddsi_sertype_ref (st);
  if ((ret = dds_create_topic_generic (participant, name, &st, qos, listener, sedp_plist)) < 0)
    ddsi_sertype_unref (st);
  return ret;
}

dds_entity_t dds_create_topic (dds_entity_t participant, const dds_topic_descriptor_t *desc, const char *name, const dds_qos_t *qos, const dds_listener_t *listener)
{
  struct ddsi_sertype_default *st;
  struct ddsi_sertype *st_tmp;
  ddsi_plist_t plist;
  dds_entity_t hdl;
  struct dds_entity *ppent;
  dds_return_t ret;

  if (desc == NULL || name == NULL || !is_valid_name (name))
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_pin (participant, &ppent)) < 0)
    return ret;

  st = dds_alloc (sizeof (*st));

  ddsi_sertype_init (&ppent->m_domain->gv, &st->c, desc->m_typename, &ddsi_sertype_ops_default, desc->m_nkeys ? &ddsi_serdata_ops_cdr : &ddsi_serdata_ops_cdr_nokey, (desc->m_nkeys == 0));
  st->native_encoding_identifier = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? CDR_LE : CDR_BE);
  st->serpool = ppent->m_domain->gv.serpool;
  st->type.size = desc->m_size;
  st->type.align = desc->m_align;
  st->type.flagset = desc->m_flagset;
  st->type.keys.nkeys = desc->m_nkeys;
  st->type.keys.keys = ddsrt_malloc (st->type.keys.nkeys  * sizeof (*st->type.keys.keys));
  for (uint32_t i = 0; i < st->type.keys.nkeys; i++)
    st->type.keys.keys[i] = desc->m_keys[i].m_index;
  st->type.ops.nops = dds_stream_countops (desc->m_ops);
  st->type.ops.ops = ddsrt_memdup (desc->m_ops, st->type.ops.nops * sizeof (*st->type.ops.ops));

  /* Check if topic cannot be optimised (memcpy marshal) */
  if (!(st->type.flagset & DDS_TOPIC_NO_OPTIMIZE)) {
    st->opt_size = dds_stream_check_optimize (&st->type);
    DDS_CTRACE (&ppent->m_domain->gv.logconfig, "Marshalling for type: %s is %soptimised\n", desc->m_typename, st->opt_size ? "" : "not ");
  }

  ddsi_plist_init_empty (&plist);
  /* Set Topic meta data (for SEDP publication) */
  plist.qos.topic_name = ddsrt_strdup (name);
  plist.qos.type_name = ddsrt_strdup (st->c.type_name);
  plist.qos.present |= (QP_TOPIC_NAME | QP_TYPE_NAME);
  if (desc->m_meta)
  {
    plist.type_description = dds_string_dup (desc->m_meta);
    plist.present |= PP_ADLINK_TYPE_DESCRIPTION;
  }
  if (desc->m_nkeys)
  {
    plist.qos.present |= QP_ADLINK_SUBSCRIPTION_KEYS;
    plist.qos.subscription_keys.use_key_list = 1;
    plist.qos.subscription_keys.key_list.n = desc->m_nkeys;
    plist.qos.subscription_keys.key_list.strs = dds_alloc (desc->m_nkeys * sizeof (char*));
    for (uint32_t index = 0; index < desc->m_nkeys; index++)
      plist.qos.subscription_keys.key_list.strs[index] = dds_string_dup (desc->m_keys[index].m_name);
  }

  st_tmp = &st->c;
  hdl = dds_create_topic_generic (participant, name, &st_tmp, qos, listener, &plist);
  if (hdl < 0)
    ddsi_sertype_unref (st_tmp);
  dds_entity_unpin (ppent);
  ddsi_plist_fini (&plist);
  return hdl;
}

dds_entity_t dds_find_topic (dds_entity_t participant, const char *name)
{
  dds_participant *pp;
  dds_return_t rc;

  if (name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((rc = dds_participant_lock (participant, &pp)) < 0)
    return rc;

  ddsrt_avl_iter_t it;
  for (dds_entity *e = ddsrt_avl_iter_first (&dds_entity_children_td, &pp->m_entity.m_children, &it); e != NULL; e = ddsrt_avl_iter_next (&it))
  {
    if (dds_entity_kind (e) != DDS_KIND_TOPIC)
      continue;

    struct dds_entity *x;
    if (dds_entity_pin (e->m_hdllink.hdl, &x) != DDS_RETCODE_OK)
      continue;

    struct dds_topic * const tp = (struct dds_topic *) e;
    if (x != e || strcmp (tp->m_ktopic->name, name) != 0)
    {
      dds_entity_unpin (x);
      continue;
    }

    struct ddsi_sertype * const sertype = ddsi_sertype_ref (tp->m_stype);
    struct dds_ktopic * const ktp = tp->m_ktopic;
    ktp->refc++;

    struct ktopic_type_guid templ;
    type_identifier_t *tid = ddsi_typeid_from_sertype (sertype);
    memset (&templ, 0, sizeof (templ));
    templ.type_id = tid;
    struct ktopic_type_guid *m = ddsrt_hh_lookup (ktp->topic_guid_map, &templ);
    assert (m != NULL);
    m->refc++;
    ddsrt_free (tid);
    dds_entity_unpin (x);

    dds_entity_t hdl = create_topic_pp_locked (pp, ktp, false, name, sertype, NULL, NULL);
    dds_participant_unlock (pp);
#ifdef DDSI_INCLUDE_TYPE_DISCOVERY
    ddsi_tl_meta_local_ref (sertype->gv, NULL, sertype);
#endif
    return hdl;
  }
  dds_participant_unlock (pp);
  return DDS_RETCODE_PRECONDITION_NOT_MET;
}

static bool dds_topic_chaining_filter (const void *sample, void *ctx)
{
  dds_topic_filter_fn realf = (dds_topic_filter_fn) ctx;
  return realf (sample);
}

static void dds_topic_mod_filter (dds_entity_t topic, dds_topic_intern_filter_fn *filter, void **ctx, bool set)
{
  dds_topic *t;
  if (dds_topic_lock (topic, &t) == DDS_RETCODE_OK)
  {
    if (set) {
      t->filter_fn = *filter;
      t->filter_ctx = *ctx;
    } else {
      *filter = t->filter_fn;
      *ctx = t->filter_ctx;
    }
    dds_topic_unlock (t);
  }
  else
  {
    *filter = 0;
    *ctx = NULL;
  }
}

void dds_set_topic_filter (dds_entity_t topic, dds_topic_filter_fn filter)
{
  dds_topic_intern_filter_fn chaining = dds_topic_chaining_filter;
  void *realf = (void *) filter;
  dds_topic_mod_filter (topic, &chaining, &realf, true);
}

void dds_topic_set_filter (dds_entity_t topic, dds_topic_filter_fn filter)
{
  dds_set_topic_filter (topic, filter);
}

dds_topic_filter_fn dds_get_topic_filter (dds_entity_t topic)
{
  dds_topic_intern_filter_fn filter;
  void *ctx;
  dds_topic_mod_filter (topic, &filter, &ctx, false);
  return (filter == dds_topic_chaining_filter) ? (dds_topic_filter_fn) ctx : 0;
}

dds_topic_filter_fn dds_topic_get_filter (dds_entity_t topic)
{
  return dds_get_topic_filter (topic);
}

void dds_topic_set_filter_with_ctx (dds_entity_t topic, dds_topic_intern_filter_fn filter, void *ctx)
{
  dds_topic_mod_filter (topic, &filter, &ctx, true);
}

dds_topic_intern_filter_fn dds_topic_get_filter_with_ctx (dds_entity_t topic)
{
  dds_topic_intern_filter_fn filter;
  void *ctx;
  dds_topic_mod_filter (topic, &filter, &ctx, false);
  return (filter == dds_topic_chaining_filter) ? 0 : filter;
}

dds_return_t dds_get_name (dds_entity_t topic, char *name, size_t size)
{
  dds_topic *t;
  dds_return_t ret;
  if (size <= 0 || name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  name[0] = '\0';
  if ((ret = dds_topic_pin (topic, &t)) != DDS_RETCODE_OK)
    return ret;
  (void) snprintf (name, size, "%s", t->m_name);
  dds_topic_unpin (t);
  return DDS_RETCODE_OK;
}

dds_return_t dds_get_type_name (dds_entity_t topic, char *name, size_t size)
{
  dds_topic *t;
  dds_return_t ret;
  if (size <= 0 || name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  name[0] = '\0';
  if ((ret = dds_topic_pin (topic, &t)) != DDS_RETCODE_OK)
    return ret;
  (void) snprintf (name, size, "%s", t->m_stype->type_name);
  dds_topic_unpin (t);
  return DDS_RETCODE_OK;
}

DDS_GET_STATUS(topic, inconsistent_topic, INCONSISTENT_TOPIC, total_count_change)
