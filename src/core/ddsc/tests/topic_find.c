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
#include <limits.h>

#include "dds/dds.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_whc.h"
#include "dds__entity.h"

#include "test_common.h"

#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1

#ifdef DDSI_INCLUDE_TOPIC_DISCOVERY
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId><EnableTopicDiscoveryEndpoints>true</EnableTopicDiscoveryEndpoints></Discovery>"
#else
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
#endif

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;
static dds_entity_t g_topic1 = 0;
static dds_entity_t g_domain_remote = 0;

ddsrt_atomic_uint32_t g_stop;

#define MAX_NAME_SIZE (100)
char g_topic_name1[MAX_NAME_SIZE];

static void topic_find_init (void)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
         * in configuration is 0, so that both domains will map to the same port number.
         * This allows to create two domains in a single test process. */
  char *conf1 = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID1);
  char *conf2 = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID2);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf1);
  CU_ASSERT_FATAL (g_domain1 > 0);
  g_domain_remote = dds_create_domain (DDS_DOMAINID2, conf2);
  CU_ASSERT_FATAL (g_domain_remote > 0);
  dds_free (conf1);
  dds_free (conf2);

  g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_FATAL (g_participant1 > 0);

  create_unique_topic_name("ddsc_topic_find_test1", g_topic_name1, MAX_NAME_SIZE);
  g_topic1 = dds_create_topic (g_participant1, &Space_Type1_desc, g_topic_name1, NULL, NULL);
  CU_ASSERT_FATAL (g_topic1 > 0);
}

static void topic_find_fini (void)
{
  dds_delete (g_domain1);
  dds_delete (g_domain_remote);
}

CU_Test(ddsc_topic_find_local, domain, .init = topic_find_init, .fini = topic_find_fini)
{
  dds_entity_t topic = dds_find_topic_locally (g_domain1, g_topic_name1, 0);
  CU_ASSERT_FATAL (topic > 0);
  CU_ASSERT_NOT_EQUAL_FATAL (topic, g_topic1);
  dds_return_t ret = dds_delete (topic);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
}

CU_Test(ddsc_topic_find_local, participant, .init = topic_find_init, .fini = topic_find_fini)
{
  dds_entity_t topic = dds_find_topic_locally (g_participant1, g_topic_name1, 0);
  CU_ASSERT_FATAL (topic > 0);
  CU_ASSERT_NOT_EQUAL_FATAL (topic, g_topic1);
}

CU_Test(ddsc_topic_find_local, non_participants, .init = topic_find_init, .fini = topic_find_fini)
{
  dds_entity_t topic = dds_find_topic_locally (g_topic1, "non_participant", 0);
  CU_ASSERT_EQUAL_FATAL (topic, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_topic_find_local, null, .init = topic_find_init, .fini = topic_find_fini)
{
  DDSRT_WARNING_MSVC_OFF (6387); /* Disable SAL warning on intentional misuse of the API */
  dds_entity_t topic = dds_find_topic_locally (g_participant1, NULL, 0);
  DDSRT_WARNING_MSVC_ON (6387);
  CU_ASSERT_EQUAL_FATAL (topic, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_topic_find_local, unknown, .init = topic_find_init, .fini = topic_find_fini)
{
  dds_entity_t topic = dds_find_topic_locally (g_participant1, "unknown", 0);
  CU_ASSERT_EQUAL_FATAL (topic, DDS_RETCODE_PRECONDITION_NOT_MET);
}

CU_Test(ddsc_topic_find_local, deleted, .init = topic_find_init, .fini = topic_find_fini)
{
  dds_delete (g_topic1);
  dds_entity_t topic = dds_find_topic_locally (g_participant1, g_topic_name1, 0);
  CU_ASSERT_EQUAL_FATAL (topic, DDS_RETCODE_PRECONDITION_NOT_MET);
}

#ifdef DDSI_INCLUDE_TOPIC_DISCOVERY
static void create_remote_topic (char * topic_name_remote)
{
  dds_entity_t participant_remote = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (participant_remote > 0);

  create_unique_topic_name ("ddsc_topic_find_remote", topic_name_remote, MAX_NAME_SIZE);
  dds_entity_t topic_remote = dds_create_topic (participant_remote, &Space_Type1_desc, topic_name_remote, NULL, NULL);
  CU_ASSERT_FATAL (topic_remote > 0);
}
#endif

CU_Test(ddsc_topic_find_global, domain, .init = topic_find_init, .fini = topic_find_fini)
{
#ifdef DDSI_INCLUDE_TOPIC_DISCOVERY
  char topic_name_remote[MAX_NAME_SIZE];
  create_remote_topic (topic_name_remote);

  dds_entity_t topic = dds_find_topic_globally (g_domain1, topic_name_remote, DDS_SECS (3));
  CU_ASSERT_FATAL (topic > 0);

  dds_return_t ret = dds_delete (topic);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
#endif
}

CU_Test(ddsc_topic_find_global, participant, .init = topic_find_init, .fini = topic_find_fini)
{
#ifdef DDSI_INCLUDE_TOPIC_DISCOVERY
  char topic_name_remote[MAX_NAME_SIZE];
  create_remote_topic (topic_name_remote);

  dds_entity_t topic = dds_find_topic_globally (g_participant1, topic_name_remote, DDS_SECS (3));
  CU_ASSERT_FATAL (topic > 0);
#endif
}

#ifdef DDSI_INCLUDE_TOPIC_DISCOVERY

static void msg (const char *msg, ...)
{
  va_list args;
  dds_time_t t;
  t = dds_time ();
  printf ("%d.%06d ", (int32_t)(t / DDS_NSECS_IN_SEC), (int32_t)(t % DDS_NSECS_IN_SEC) / 1000);
  va_start (args, msg);
  vprintf (msg, args);
  va_end (args);
  printf ("\n");
}

enum topic_thread_state {
  INIT,
  DONE,
  STOPPED
};

struct create_topic_thread_arg
{
  bool remote;
  ddsrt_atomic_uint32_t state;
  uint32_t num_tp;
  dds_entity_t pp;
  char topic_name_prefix[MAX_NAME_SIZE];
  const dds_topic_descriptor_t *topic_desc;
};

static void set_topic_name (char *name, const char *prefix, uint32_t index)
{
  snprintf (name, MAX_NAME_SIZE + 10, "%s_%u", prefix, index);
}

static uint32_t topics_thread (void *a)
{
  char topic_name[MAX_NAME_SIZE + 10];
  struct create_topic_thread_arg *arg = (struct create_topic_thread_arg *) a;
  dds_entity_t *topics = ddsrt_malloc (arg->num_tp * sizeof (*topics));

  /* create topics */
  msg ("%s topics thread: creating %u topics with prefix %s", arg->remote ? "remote" : "local", arg->num_tp, arg->topic_name_prefix);
  for (uint32_t t = 0; t < arg->num_tp; t++)
  {
    set_topic_name (topic_name, arg->topic_name_prefix, t);
    topics[t] = dds_create_topic (arg->pp, arg->topic_desc, topic_name, NULL, NULL);
    CU_ASSERT_FATAL (topics[t] > 0);
  }
  ddsrt_atomic_st32 (&arg->state, DONE);

  /* wait for stop signal */
  while (!ddsrt_atomic_ld32 (&g_stop))
    dds_sleepfor (DDS_MSECS (10));

  /* delete topics */
  for (uint32_t t = 0; t < arg->num_tp; t++)
  {
    dds_return_t ret = dds_delete (topics[t]);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    dds_sleepfor (DDS_MSECS (1));
  }
  ddsrt_atomic_st32 (&arg->state, STOPPED);
  ddsrt_free (topics);
  return 0;
}

static dds_return_t topics_thread_state (struct create_topic_thread_arg *arg, uint32_t desired_state, dds_duration_t timeout)
{
  const dds_time_t abstimeout = dds_time () + timeout;
  while (dds_time () < abstimeout && ddsrt_atomic_ld32 (&arg->state) != desired_state)
    dds_sleepfor (DDS_MSECS (10));
  return ddsrt_atomic_ld32 (&arg->state) == desired_state ? DDS_RETCODE_OK : DDS_RETCODE_TIMEOUT;
}
#endif /* DDSI_INCLUDE_TOPIC_DISCOVERY */

CU_TheoryDataPoints (ddsc_topic_find_global, find_delete_topics) = {
    CU_DataPoints (uint32_t,     1,  5,  0,  5), /* number of local participants */
    CU_DataPoints (uint32_t,     1,  0,  5,  5), /* number of remote participants */
    CU_DataPoints (uint32_t,     1, 50, 50, 50), /* number of topics per participant */
};

CU_Theory ((uint32_t num_local_pp, uint32_t num_remote_pp, uint32_t num_tp), ddsc_topic_find_global, find_delete_topics, .init = topic_find_init, .fini = topic_find_fini, .timeout = 30)
{
#ifdef DDSI_INCLUDE_TOPIC_DISCOVERY
  msg("ddsc_topic_find_global.remote_topics: %u/%u local/remote participants, %u topics", num_local_pp, num_remote_pp, num_tp);
  dds_return_t ret;
  dds_entity_t participant_remote = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (participant_remote > 0);
  ddsrt_atomic_st32 (&g_stop, 0);
  char topic_name[MAX_NAME_SIZE + 10];

  /* Start threads that create topics on local and remote participant] */
  struct create_topic_thread_arg *create_args = ddsrt_malloc ((num_local_pp + num_remote_pp) * sizeof (*create_args));
  for (uint32_t n = 0; n < num_local_pp + num_remote_pp; n++)
  {
    bool remote = n >= num_local_pp;
    create_args[n].remote = remote;
    ddsrt_atomic_st32 (&create_args[n].state, INIT);
    create_args[n].num_tp = num_tp;
    create_args[n].pp = remote ? participant_remote : g_participant1;
    create_unique_topic_name ("ddsc_topic_find_remote", create_args[n].topic_name_prefix, MAX_NAME_SIZE);
    create_args[n].topic_desc = n % 3 ? (n % 3 == 1 ? &Space_Type2_desc : &Space_Type3_desc) : &Space_Type1_desc;

    ddsrt_thread_t thread_id;
    ddsrt_threadattr_t thread_attr;
    ddsrt_threadattr_init (&thread_attr);
    ret = ddsrt_thread_create (&thread_id, "create_topic", &thread_attr, topics_thread, &create_args[n]);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  }

  /* wait for all created topics to be found */
  msg ("find topics");
  for (uint32_t n = 0; n < num_local_pp + num_remote_pp; n++)
  {
    ret = topics_thread_state (&create_args[n], DONE, DDS_MSECS (5000));
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

    for (uint32_t t = 0; t < create_args[n].num_tp; t++)
    {
      set_topic_name (topic_name, create_args->topic_name_prefix, t);
      dds_entity_t topic = dds_find_topic_globally (g_participant1, topic_name, DDS_SECS (5));
      CU_ASSERT_FATAL (topic > 0);
    }
  }

  /* Stop threads (which will delete their topics) and keep looking
     for these topics (we're not interested in the result) */
  ddsrt_atomic_st32 (&g_stop, 1);
  const dds_time_t abstimeout = dds_time () + DDS_MSECS (500);
  uint32_t t = 0;
  do
  {
    set_topic_name (topic_name, create_args->topic_name_prefix, t);
    (void) dds_find_topic_locally (g_participant1, topic_name, 0);
    (void) dds_find_topic_locally (g_participant1, topic_name, DDS_MSECS (1));
    (void) dds_find_topic_globally (g_participant1, topic_name, 0);
    (void) dds_find_topic_globally (g_participant1, topic_name, DDS_MSECS (1));
    dds_sleepfor (DDS_MSECS (1));
    if (++t == num_local_pp + num_remote_pp)
      t = 0;
  } while (dds_time () < abstimeout);

  /* Cleanup */
  for (uint32_t n = 0; n < num_local_pp + num_remote_pp; n++)
  {
    ret = topics_thread_state (&create_args[n], STOPPED, DDS_MSECS (5000));
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  }
  ddsrt_free (create_args);
#else
  (void) num_local_pp;
  (void) num_remote_pp;
  (void) num_tp;
#endif /* DDSI_INCLUDE_TOPIC_DISCOVERY */
}
