/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdlib.h>
#include <assert.h>

#include "dds/dds.h"
#include "CUnit/Test.h"
#include "CUnit/Theory.h"

#include "dds/version.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/ddsi_xqos.h"

#include "dds/security/dds_security_api.h"

#include "common/config_env.h"
#include "common/authentication_wrapper.h"
#include "common/handshake_test_utils.h"
#include "common/security_config_test_utils.h"
#include "common/test_identity.h"
#include "common/cert_utils.h"

#define ID1 TEST_IDENTITY1_CERTIFICATE
#define ID1K TEST_IDENTITY1_PRIVATE_KEY
#define ID2 TEST_IDENTITY2_CERTIFICATE
#define ID2K TEST_IDENTITY2_PRIVATE_KEY
#define ID3 TEST_IDENTITY3_CERTIFICATE
#define ID3K TEST_IDENTITY3_PRIVATE_KEY
#define CA1 TEST_IDENTITY_CA1_CERTIFICATE
#define CA1K TEST_IDENTITY_CA1_PRIVATE_KEY
#define CA2 TEST_IDENTITY_CA2_CERTIFICATE
#define CA2K TEST_IDENTITY_CA2_PRIVATE_KEY

static const char *config =
    "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}"
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <ExternalDomainId>0</ExternalDomainId>"
    "    <Tag>\\${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <DDSSecurity>"
    "    <Authentication>"
    "      <Library finalizeFunction=\"finalize_test_authentication_wrapped\" initFunction=\"init_test_authentication_wrapped\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
    "      <IdentityCertificate>data:,${TEST_IDENTITY_CERTIFICATE}</IdentityCertificate>"
    "      <PrivateKey>data:,${TEST_IDENTITY_PRIVATE_KEY}</PrivateKey>"
    "      <IdentityCA>data:,${TEST_IDENTITY_CA_CERTIFICATE}</IdentityCA>"
    "      ${TRUSTED_CA_DIR:+<TrustedCADir>}${TRUSTED_CA_DIR}${TRUSTED_CA_DIR:+</TrustedCADir>}"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Library finalizeFunction=\"finalize_access_control\" initFunction=\"init_access_control\"/>"
    "      <Governance>file:" COMMON_ETC_PATH("default_governance.p7s") "</Governance>"
    "      <PermissionsCA>file:" COMMON_ETC_PATH("default_permissions_ca.pem") "</PermissionsCA>"
    "      <Permissions>file:" COMMON_ETC_PATH("default_permissions.p7s") "</Permissions>"
    "    </AccessControl>"
    "    <Cryptographic>"
    "      <Library finalizeFunction=\"finalize_crypto\" initFunction=\"init_crypto\"/>"
    "    </Cryptographic>"
    "  </DDSSecurity>"
    "</Domain>";

#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;

static dds_entity_t g_domain2 = 0;
static dds_entity_t g_participant2 = 0;

static void authentication_init(
    const char * id1_cert, const char * id1_key, const char * id1_ca,
    const char * id2_cert, const char * id2_key, const char * id2_ca,
    const char * trusted_ca_dir, bool exp_pp1_fail, bool exp_pp2_fail)
{
  struct kvp config_vars1[] = {
    { "TEST_IDENTITY_CERTIFICATE", id1_cert, 1 },
    { "TEST_IDENTITY_PRIVATE_KEY", id1_key, 1 },
    { "TEST_IDENTITY_CA_CERTIFICATE", id1_ca, 1 },
    { "TRUSTED_CA_DIR", trusted_ca_dir, 3 },
    { NULL, NULL, 0 }
  };

  struct kvp config_vars2[] = {
    { "TEST_IDENTITY_CERTIFICATE", id2_cert, 1 },
    { "TEST_IDENTITY_PRIVATE_KEY", id2_key, 1 },
    { "TEST_IDENTITY_CA_CERTIFICATE", id2_ca, 1 },
    { "TRUSTED_CA_DIR", trusted_ca_dir, 3 },
    { NULL, NULL, 0 }
  };

  char *conf1 = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars1);
  char *conf2 = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars2);
  CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars1), 0);
  CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars2), 0);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf1);
  g_domain2 = dds_create_domain (DDS_DOMAINID2, conf2);
  dds_free (conf1);
  dds_free (conf2);

  g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  g_participant2 = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL ((exp_pp1_fail && g_participant1 <= 0) || g_participant1 > 0);
  CU_ASSERT_FATAL ((exp_pp2_fail && g_participant2 <= 0) || g_participant2 > 0);
}

static void authentication_fini(bool delete_pp1, bool delete_pp2)
{
  if (delete_pp1)
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant1), DDS_RETCODE_OK);
  if (delete_pp2)
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant2), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain1), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain2), DDS_RETCODE_OK);
}

#define FM_CA "error: unable to get local issuer certificate"
#define FM_INVK "Failed to finalize verify context"
CU_TheoryDataPoints(ddssec_authentication, id_ca_certs) = {
    CU_DataPoints(const char *, ID1,   ID2,   ID3,   ID1,   ID3),      /* Identity for domain 2 */
    CU_DataPoints(const char *, ID1K,  ID2K,  ID3K,  ID1K,  ID1K),     /* Private key for domain 2 identity */
    CU_DataPoints(const char *, CA1,   CA2,   CA1,   CA2,   CA1),      /* CA for domain 2 identity */
    CU_DataPoints(bool,         false, false, false, true,  false),    /* expect validate local failed for domain 2 */
    CU_DataPoints(const char *, NULL,  NULL,  NULL,  FM_CA, NULL),     /* expected error message for validate local failed */
    CU_DataPoints(bool,         false, false, false, false, true),     /* expect handshake request failed */
    CU_DataPoints(const char *, NULL,  NULL,  NULL,  NULL,  FM_INVK),  /* expected error message for handshake request failed */
    CU_DataPoints(bool,         false, true,  false, true,  false),    /* expect handshake reply failed */
    CU_DataPoints(const char *, NULL,  FM_CA, NULL,  FM_CA, NULL)      /* expected error message for handshake reply failed */
};

static void validate_hs(struct Handshake *hs, bool exp_fail_hs_req, const char * fail_hs_req_msg, bool exp_fail_hs_reply, const char * fail_hs_reply_msg)
{
  DDS_Security_ValidationResult_t exp_result = hs->node_type == HSN_REQUESTER ? DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE : DDS_SECURITY_VALIDATION_OK;
  if (hs->node_type == HSN_REQUESTER && exp_fail_hs_req)
  {
    CU_ASSERT_EQUAL_FATAL (hs->finalResult, exp_fail_hs_req ? DDS_SECURITY_VALIDATION_FAILED : exp_result);
    CU_ASSERT_FATAL (hs->err_msg != NULL);
    CU_ASSERT_FATAL (strstr(hs->err_msg, fail_hs_req_msg) != NULL);
  }
  else if (hs->node_type == HSN_REPLIER && exp_fail_hs_reply)
  {
    CU_ASSERT_EQUAL_FATAL (hs->finalResult, exp_fail_hs_reply ? DDS_SECURITY_VALIDATION_FAILED : exp_result);
    CU_ASSERT_FATAL (hs->err_msg != NULL);
    CU_ASSERT_FATAL (strstr(hs->err_msg, fail_hs_reply_msg) != NULL);
  }
}

CU_Theory((const char * id2, const char *key2, const char *ca2,
    bool exp_fail_local, const char * fail_local_msg,
    bool exp_fail_hs_req, const char * fail_hs_req_msg,
    bool exp_fail_hs_reply, const char * fail_hs_reply_msg), ddssec_authentication, id_ca_certs)
{
  struct Handshake *hs_list;
  int nhs;
  bool exp_fail_hs = exp_fail_hs_req || exp_fail_hs_reply;
  authentication_init (ID1, ID1K, CA1, id2, key2, ca2, NULL, exp_fail_hs, exp_fail_hs);

  // Domain 1
  validate_handshake (DDS_DOMAINID1, false, NULL, &hs_list, &nhs);
  for (int n = 0; n < nhs; n++)
    validate_hs (&hs_list[n], exp_fail_hs_req, fail_hs_req_msg, exp_fail_hs_reply, fail_hs_reply_msg);
  handshake_list_fini (hs_list, nhs);

  // Domain 2
  validate_handshake (DDS_DOMAINID2, exp_fail_local, fail_local_msg, &hs_list, &nhs);
  for (int n = 0; n < nhs; n++)
    validate_hs (&hs_list[n], exp_fail_hs_req, fail_hs_req_msg, exp_fail_hs_reply, fail_hs_reply_msg);
  handshake_list_fini (hs_list, nhs);

  authentication_fini (!exp_fail_hs, !exp_fail_hs);
}

CU_TheoryDataPoints(ddssec_authentication, trusted_ca_dir) = {
    CU_DataPoints(const char *, "",    ".",   "/nonexisting", NULL),
    CU_DataPoints(bool,         false, false, true,           false)
};
CU_Theory((const char * ca_dir, bool exp_fail), ddssec_authentication, trusted_ca_dir)
{
  printf("Testing custom CA dir: %s\n", ca_dir);
  authentication_init (ID1, ID1K, CA1, ID1, ID1K, CA1, ca_dir, exp_fail, exp_fail);
  if (!exp_fail)
  {
    validate_handshake_nofail (DDS_DOMAINID1);
    validate_handshake_nofail (DDS_DOMAINID2);
  }
  authentication_fini (!exp_fail, !exp_fail);
}

#define M(n) ((n)*60)
#define H(n) (M(n)*60)
#define D(n) (H(n)*24)
CU_TheoryDataPoints(ddssec_authentication, expired_cert) = {
    CU_DataPoints(const char *,
                       "all valid 1d",
                              "ca expired",
                                      "id1 expired",
                                             "id2 expired",
                                                    "ca and id1 1min valid",
                                                           "id1 and id2 1s valid, delay 1100ms",
                                                                 "id1 valid after 1s, delay 1100ms"),
    CU_DataPoints(int,  0,     -M(1), 0,     0,     0,     0,    0),       /* CA1 not before */
    CU_DataPoints(int,  D(1),  0,     D(1),  D(1),  M(1),  D(1), D(1)),    /* CA1 not after */
    CU_DataPoints(int,  0,     0,     -D(1), 0,     0,     0,    1),       /* ID1 not before */
    CU_DataPoints(int,  D(1),  D(1),  0,     D(1),  M(1),  1,    D(1)),    /* ID1 not after */
    CU_DataPoints(int,  0,     0,     0,     -D(1), 0,     0,    0),       /* ID2 not before */
    CU_DataPoints(int,  D(1),  D(1),  D(1),  0,     D(1),  1,    D(1)),    /* ID2 not after */
    CU_DataPoints(bool, false, true,  true,  false, false, true, false),   /* expect validate local ID1 fail */
    CU_DataPoints(bool, false, true,  false, true,  false, true, false),   /* expect validate local ID2 fail */
    CU_DataPoints(int,  0,     0,     0,     0,     0,     1100, 1100),    /* delay (ms) after generating certificate */
};
CU_Theory(
  (const char * test_descr, int ca_not_before, int ca_not_after, int id1_not_before, int id1_not_after, int id2_not_before, int id2_not_after, bool id1_local_fail, bool id2_local_fail, int delay),
  ddssec_authentication, expired_cert)
{
  char *ca, *id1, *id2;
  printf("running test expired_cert: %s\n", test_descr);
  ca = generate_ca ("ca1", CA1K, ca_not_before, ca_not_after);
  id1 = generate_identity (ca, CA1K, "id1", ID1K, id1_not_before, id1_not_after);
  id2 = generate_identity (ca, CA1K, "id2", ID1K, id2_not_before, id2_not_after);
  dds_sleepfor (DDS_MSECS (delay));
  authentication_init (id1, ID1K, ca, id2, ID1K, ca, NULL, id1_local_fail, id2_local_fail);
  validate_handshake (DDS_DOMAINID1, id1_local_fail, NULL, NULL, NULL);
  validate_handshake (DDS_DOMAINID2, id2_local_fail, NULL, NULL, NULL);
  authentication_fini (!id1_local_fail, !id2_local_fail);
  ddsrt_free (ca);
  ddsrt_free (id1);
  ddsrt_free (id2);
}
#undef D
#undef H
#undef M

