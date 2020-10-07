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
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId><EnableTopicDiscoveryEndpoints>true</EnableTopicDiscoveryEndpoints></Discovery>"

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;
static dds_entity_t g_subscriber1  = 0;
static dds_entity_t g_publisher1   = 0;
static dds_entity_t g_domain_remote      = 0;

static void topic_discovery_init (void)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
         * in configuration is 0, so that both domains will map to the same port number.
         * This allows to create two domains in a single test process. */
  char *conf1 = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID1);
  char *conf2 = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID2);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf1);
  g_domain_remote = dds_create_domain (DDS_DOMAINID2, conf2);
  dds_free (conf1);
  dds_free (conf2);

  g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_FATAL (g_participant1 > 0);
  g_subscriber1 = dds_create_subscriber (g_participant1, NULL, NULL);
  CU_ASSERT_FATAL (g_subscriber1 > 0);
  g_publisher1 = dds_create_publisher (g_participant1, NULL, NULL);
  CU_ASSERT_FATAL (g_publisher1 > 0);
}

static void topic_discovery_fini (void)
{
  dds_delete (g_domain1);
  dds_delete (g_domain_remote);
}

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

CU_TheoryDataPoints(ddsc_topic_discovery, remote_topics) = {
    CU_DataPoints(uint32_t,     1,     1,     5,     5,    30,     1,     1,     5,     5,    30,    1,    5), /* number of participants */
    CU_DataPoints(uint32_t,     1,     5,     1,    64,     3,     1,     5,     1,    64,     3,    1,   30), /* number of topics per participant */
    CU_DataPoints(bool,      true,  true,  true,  true,  true, false, false, false, false, false, true, true), /* test historical data for topic discovery */
    CU_DataPoints(bool,     false, false, false, false, false,  true,  true,  true,  true,  true, true, true), /* test live topic discovery */
};

CU_Theory ((uint32_t num_pp, uint32_t num_tp, bool hist_data, bool live_data), ddsc_topic_discovery, remote_topics, .init = topic_discovery_init, .fini = topic_discovery_fini, .timeout = 30)
{
  msg("ddsc_topic_discovery.remote_topics: %u participants, %u topics,%s%s", num_pp, num_tp, hist_data ? " historical-data" : "", live_data ? " live-data" : "");

  CU_ASSERT_FATAL (num_pp > 0);
  CU_ASSERT_FATAL (num_tp > 0 && num_tp <= 64);
  CU_ASSERT_FATAL (hist_data || live_data);

  char **topic_names = ddsrt_malloc (2 * num_pp * num_tp * sizeof (char *));
  uint64_t *seen = ddsrt_malloc (2 * num_pp * sizeof (*seen));
  bool all_seen = false;
  dds_entity_t *participant_remote = ddsrt_malloc (num_pp * sizeof (*participant_remote));

  for (uint32_t p = 0; p < num_pp; p++)
  {
    participant_remote[p] = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
    CU_ASSERT_FATAL (participant_remote[p] > 0);
    seen[p] = seen[num_pp + p] = 0;
  }

  /* create topics before reader has been created (will be delivered as historical data) */
  if (hist_data)
  {
    for (uint32_t p = 0; p < num_pp; p++)
      for (uint32_t t = 0; t < num_tp; t++)
      {
        topic_names[p * num_tp + t] = ddsrt_malloc (101);
        create_unique_topic_name ("ddsc_topic_discovery_test", topic_names[p * num_tp + t], 100);
        dds_entity_t topic = dds_create_topic (participant_remote[p], &Space_Type1_desc, topic_names[p * num_tp + t], NULL, NULL);
        CU_ASSERT_FATAL (topic > 0);
      }

    /* sleep for some time so that deliver_historical_data will be used for (at least some of)
       the sedp samples for the created topics */
    dds_sleepfor (DDS_MSECS (500));
  }

  /* create reader for DCPSTopic */
  dds_entity_t topic_rd = dds_create_reader (g_participant1, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd > 0);

  /* create more topics after reader has been created */
  if (live_data)
  {
    uint32_t offs = num_pp * num_tp;
    for (uint32_t p = 0; p < num_pp; p++)
      for (uint32_t t = 0; t < num_tp; t++)
      {
        topic_names[offs + p * num_tp + t] = ddsrt_malloc (101);
        create_unique_topic_name ("ddsc_topic_discovery_test_remote", topic_names[offs + p * num_tp + t], 100);
        dds_entity_t topic = dds_create_topic (participant_remote[p], &Space_Type1_desc, topic_names[offs + p * num_tp + t], NULL, NULL);
        CU_ASSERT_FATAL (topic > 0);
      }
  }

  /* read DCPSTopic and check if all topics seen */
  dds_time_t t_exp = dds_time () + DDS_SECS (10);
  do
  {
    void *raw[1] = { 0 };
    dds_sample_info_t sample_info[1];
    dds_return_t n;
    while ((n = dds_take (topic_rd, raw, sample_info, 1, 1)) > 0)
    {
      CU_ASSERT_EQUAL_FATAL (n, 1);
      if (sample_info[0].valid_data)
      {
        dds_builtintopic_topic_t *sample = raw[0];
        // msg ("read topic: %s", sample->topic_name);
        for (uint32_t p = 0; p < 2 * num_pp; p++)
          for (uint32_t t = 0; t < num_tp; t++)
            if (((hist_data && p < num_pp) || (live_data && p >= num_pp)) && !strcmp (sample->topic_name, topic_names[p * num_tp + t]))
              seen[p] |= UINT64_C (1) << t;
      }
      dds_return_loan (topic_rd, raw, n);
    }
    all_seen = true;
    for (uint32_t p = 0; p < 2 * num_pp && all_seen; p++)
      if (((hist_data && p < num_pp) || (live_data && p >= num_pp)) && seen[p] != (UINT64_C (2) << (num_tp - 1)) - 1)
        all_seen = false;
    dds_sleepfor (DDS_MSECS (10));
  }
  while (!all_seen && dds_time () < t_exp);
  CU_ASSERT_FATAL (all_seen);

  /* clean up */
  for (uint32_t p = 0; p < 2 * num_pp; p++)
    for (uint32_t t = 0; t < num_tp; t++)
      if ((hist_data && p < num_pp) || (live_data && p >= num_pp))
        ddsrt_free (topic_names[p * num_tp + t]);
  ddsrt_free (seen);
  ddsrt_free (topic_names);
  ddsrt_free (participant_remote);
}

CU_Test (ddsc_topic_discovery, single_topic_def, .init = topic_discovery_init, .fini = topic_discovery_fini)
{
  msg("ddsc_topic_discovery.single_topic_def");

  char topic_name[100];
  create_unique_topic_name ("ddsc_topic_discovery_test_single_def", topic_name, 100);
  dds_entity_t participant_remote = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (participant_remote > 0);

  /* create topic */
  dds_entity_t topic = dds_create_topic (g_participant1, &Space_Type1_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  /* create reader for DCPSTopic and for the application topic */
  dds_entity_t topic_rd = dds_create_reader (g_participant1, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd > 0);
  dds_entity_t app_rd = dds_create_reader (g_participant1, topic, NULL, NULL);
  CU_ASSERT_FATAL (app_rd > 0);

  /* create 'remote' topic and a reader and writer using this topic */
  dds_entity_t topic_remote = dds_create_topic (participant_remote, &Space_Type1_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic_remote > 0);
  dds_entity_t reader_remote = dds_create_reader (participant_remote, topic_remote, NULL, NULL);
  CU_ASSERT_FATAL (reader_remote > 0);
  dds_entity_t writer_remote = dds_create_writer (participant_remote, topic_remote, NULL, NULL);
  CU_ASSERT_FATAL (writer_remote > 0);

  /* check that a single DCPSTopic sample is received */
  dds_time_t t_exp = dds_time () + DDS_SECS (1);
  bool topic_seen = false;
  do
  {
    void *raw[1] = { 0 };
    dds_sample_info_t sample_info[1];
    dds_return_t n;
    while ((n = dds_take (topic_rd, raw, sample_info, 1, 1)) > 0)
    {
      CU_ASSERT_EQUAL_FATAL (n, 1);
      dds_builtintopic_topic_t *sample = raw[0];
      msg ("read topic: %s", sample->topic_name);
      if (!strcmp (sample->topic_name, topic_name))
      {
        CU_ASSERT_FATAL (!topic_seen);
        topic_seen = true;
      }
      dds_return_loan (topic_rd, raw, n);
    }
    dds_sleepfor (DDS_MSECS (10));
  } while (dds_time () < t_exp);
  CU_ASSERT_FATAL (topic_seen);
}

CU_Test (ddsc_topic_discovery, different_type, .init = topic_discovery_init, .fini = topic_discovery_fini)
{
  msg("ddsc_topic_discovery.different_type");

  char topic_name[100];
  create_unique_topic_name ("ddsc_topic_discovery_test_type", topic_name, 100);
  dds_entity_t participant_remote = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (participant_remote > 0);

  /* create topic */
  dds_entity_t topic = dds_create_topic (g_participant1, &Space_Type1_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  /* create reader for DCPSTopic */
  dds_entity_t topic_rd = dds_create_reader (g_participant1, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd > 0);

  /* create 'remote' topic with different type */
  dds_entity_t topic_remote = dds_create_topic (participant_remote, &Space_Type3_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic_remote > 0);

  /* check that a DCPSTopic sample is received for local topic and remote topic (with different key) */
  dds_time_t t_exp = dds_time () + DDS_SECS (1);
  uint32_t topic_seen = 0;
  unsigned char key[16];
  do
  {
    void *raw[1] = { 0 };
    dds_sample_info_t sample_info[1];
    dds_return_t n;
    while ((n = dds_take (topic_rd, raw, sample_info, 1, 1)) > 0)
    {
      CU_ASSERT_EQUAL_FATAL (n, 1);
      dds_builtintopic_topic_t *sample = raw[0];
      msg ("read topic: %s", sample->topic_name);
      if (!strcmp (sample->topic_name, topic_name))
      {
        topic_seen++;
        if (topic_seen == 0)
          memcpy (&key, &sample->key, sizeof (key));
        else
          CU_ASSERT_FATAL (memcmp (&key, &sample->key, sizeof (key)) != 0);
      }
      dds_return_loan (topic_rd, raw, n);
    }
    dds_sleepfor (DDS_MSECS (10));
  } while (dds_time () < t_exp);
  CU_ASSERT_EQUAL_FATAL (topic_seen, 2);
}
