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

// static void msg (const char *msg, ...)
// {
//   va_list args;
//   dds_time_t t;
//   t = dds_time ();
//   printf ("%d.%06d ", (int32_t)(t / DDS_NSECS_IN_SEC), (int32_t)(t % DDS_NSECS_IN_SEC) / 1000);
//   va_start (args, msg);
//   vprintf (msg, args);
//   va_end (args);
//   printf ("\n");
// }

CU_Test(ddsc_topic_find_local, valid, .init = topic_find_init, .fini = topic_find_fini)
{
  dds_entity_t topic = dds_find_topic_locally (g_participant1, g_topic_name1, 0);
  CU_ASSERT_NOT_EQUAL_FATAL (topic, g_topic1);
  dds_return_t ret = dds_delete (topic);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
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
