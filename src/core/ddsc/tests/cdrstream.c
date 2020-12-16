/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "CUnit/Theory.h"
#include "dds/dds.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsc/dds_public_impl.h"
#include "dds__topic.h"
#include "dds/ddsi/ddsi_serdata.h"

#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1
#define DDS_CONFIG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"

typedef void * sample_init (void);
typedef bool sample_equal (void *s1, void *s2);
typedef void sample_free (void *);

/**********************************************
 * Nested structs
 **********************************************/
typedef struct TestIdl_SubMsg1
{
  uint32_t submsg_field1;
} TestIdl_SubMsg1;

typedef struct TestIdl_SubMsg2
{
  uint32_t submsg_field1;
  uint32_t submsg_field2;
  TestIdl_SubMsg1 submsg_field3;
} TestIdl_SubMsg2;

typedef struct TestIdl_MsgNested
{
  TestIdl_SubMsg1 msg_field1;
  TestIdl_SubMsg2 msg_field2;
  TestIdl_SubMsg1 msg_field3;
} TestIdl_MsgNested;

static const uint32_t TestIdl_MsgNested_ops [] =
{
  // Msg2
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgNested, msg_field1), (3u << 16u) + 18u,  // SubMsg1
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgNested, msg_field2), (3u << 16u) + 7u,   // SubMsg2
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgNested, msg_field3), (3u << 16u) + 12u,  // SubMsg1
  DDS_OP_RTS,

  // SubMsg2
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsg2, submsg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsg2, submsg_field2),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_SubMsg2, submsg_field3), (3u << 16u) + 4u, // SubMsg2
  DDS_OP_RTS,

  // SubMsg1
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsg1, submsg_field1),
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgNested_desc = { sizeof (TestIdl_MsgNested), 4u, 0u, 0u, "TestIdl::MsgNested", NULL, 17, TestIdl_MsgNested_ops, "" };

static void * sample_init_nested ()
{
  TestIdl_MsgNested msg = {
            .msg_field1 = { .submsg_field1 = 1100 },
            .msg_field2 = { .submsg_field1 = 2100, .submsg_field2 = 2200, .submsg_field3 = { .submsg_field1 = 2310 } },
            .msg_field3 = { .submsg_field1 = 3100 } };
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgNested));
}

static bool sample_equal_nested (void *s1, void *s2)
{
  return !memcmp (s1, s2, sizeof (TestIdl_MsgNested));
}

static void sample_free_nested (void *s)
{
  dds_free (s);
}

/**********************************************
 * String types
 **********************************************/
typedef struct TestIdl_StrType
{
  char * str1;
  char * str2; // bounded (5)
  char * strseq3[2];
  char * strseq4[3]; // bounded (5)
} TestIdl_StrType;

typedef struct TestIdl_MsgStr
{
  TestIdl_StrType msg_field1;
} TestIdl_MsgStr;

static const uint32_t TestIdl_Msg_ops [] =
{
  DDS_OP_ADR | DDS_OP_TYPE_STR, offsetof (TestIdl_MsgStr, msg_field1.str1),
  DDS_OP_ADR | DDS_OP_TYPE_BSP, offsetof (TestIdl_MsgStr, msg_field1.str2), 6,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_STR, offsetof (TestIdl_MsgStr, msg_field1.strseq3), 2,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_BSP, offsetof (TestIdl_MsgStr, msg_field1.strseq4), 3, 0, 6,
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgStr_desc = { sizeof (TestIdl_MsgStr), sizeof (char *), DDS_TOPIC_NO_OPTIMIZE, 0u, "TestIdl::MsgStr", NULL, 6, TestIdl_Msg_ops, "" };

static void * sample_init_str ()
{
  TestIdl_MsgStr msg = { .msg_field1 = { .str1 = "vstr", .str2 = "bstr", .strseq3 = { "vstr1", "vstr2" }} };
  msg.msg_field1.strseq4[0] = ddsrt_strdup ("bstr1");
  msg.msg_field1.strseq4[1] = ddsrt_strdup ("bstr2");
  msg.msg_field1.strseq4[2] = ddsrt_strdup ("bstr3");
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgStr));
}

static bool sample_equal_str (void *s1, void *s2)
{
  TestIdl_MsgStr *msg1 = s1, *msg2 = s2;
  bool eq = true;
  for (int i = 0; i < 2 && eq; i++)
    if (strcmp (msg1->msg_field1.strseq3[i], msg2->msg_field1.strseq3[i]))
      eq = false;
  for (int i = 0; i < 3 && eq; i++)
    if (strcmp (msg1->msg_field1.strseq4[i], msg2->msg_field1.strseq4[i]))
      eq = false;
  return (eq
    && !strcmp (msg1->msg_field1.str1, msg2->msg_field1.str1)
    && !strcmp (msg1->msg_field1.str2, msg2->msg_field1.str2));
}

static void sample_free_str (void *s)
{
  TestIdl_MsgStr *msg = s;
  dds_free (msg->msg_field1.strseq4[0]);
  dds_free (msg->msg_field1.strseq4[1]);
  dds_free (msg->msg_field1.strseq4[2]);
  dds_free (msg);
}

/**********************************************
 * Unions
 **********************************************/
typedef enum TestIdl_Kind1
{
  TestIdl_KIND1_0,
  TestIdl_KIND1_1,
  TestIdl_KIND1_2
} TestIdl_Kind1;

typedef enum TestIdl_Kind2
{
  TestIdl_KIND2_0,
  TestIdl_KIND2_5 = 5,
  TestIdl_KIND2_6,
  TestIdl_KIND2_10 = 10
} TestIdl_Kind2;

typedef enum TestIdl_Kind3
{
  TestIdl_KIND3_0,
  TestIdl_KIND3_1,
  TestIdl_KIND3_2
} TestIdl_Kind3;

typedef struct TestIdl_Union0
{
  int32_t _d;
  union
  {
    int32_t field0_1;
    uint32_t field0_2;
  } _u;
} TestIdl_Union0;

typedef struct TestIdl_Union1
{
  TestIdl_Kind3 _d;
  union
  {
    int32_t field1;
    TestIdl_Kind2 field2;
    TestIdl_Union0 field3;
  } _u;
} TestIdl_Union1;

typedef struct TestIdl_MsgUnion
{
  TestIdl_Kind1 msg_field1;
  TestIdl_Kind2 msg_field2;
  TestIdl_Union1 msg_field3;
} TestIdl_MsgUnion;

static const uint32_t TestIdl_MsgUnion_ops [] =
{
  DDS_OP_ADR | DDS_OP_TYPE_ENU, offsetof (TestIdl_MsgUnion, msg_field1), 2u,
  DDS_OP_ADR | DDS_OP_TYPE_ENU, offsetof (TestIdl_MsgUnion, msg_field2), 10u,

  DDS_OP_ADR | DDS_OP_TYPE_UNI | DDS_OP_SUBTYPE_ENU, offsetof (TestIdl_MsgUnion, msg_field3._d), 3u, (26u << 16) + 5u, 2u,
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN | 0, TestIdl_KIND3_0, offsetof (TestIdl_MsgUnion, msg_field3._u.field1),
    DDS_OP_JEQ | DDS_OP_TYPE_ENU | 0, TestIdl_KIND3_1, offsetof (TestIdl_MsgUnion, msg_field3._u.field2), 10u,
    DDS_OP_JEQ | DDS_OP_TYPE_UNI | 3, TestIdl_KIND3_2, offsetof (TestIdl_MsgUnion, msg_field3._u.field3),

  DDS_OP_ADR | DDS_OP_TYPE_UNI | DDS_OP_SUBTYPE_4BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_Union0, _d), 2u, (10u << 16) + 4u,
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN | 0, 0, offsetof (TestIdl_Union0, _u.field0_1),
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | 0, 1, offsetof (TestIdl_Union0, _u.field0_2),
  DDS_OP_RTS,
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgUnion_desc = { sizeof (TestIdl_MsgUnion), 4u, DDS_TOPIC_CONTAINS_UNION, 0u, "TestIdl::MsgUnion", NULL, 3, TestIdl_MsgUnion_ops, "" };

static void * sample_init_union ()
{
  TestIdl_MsgUnion msg = { .msg_field1 = TestIdl_KIND1_1, .msg_field2 = TestIdl_KIND2_10, .msg_field3._d = TestIdl_KIND3_1, .msg_field3._u.field2 = TestIdl_KIND2_6 };
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgUnion));
}

static bool sample_equal_union (void *s1, void *s2)
{
  return !memcmp (s1, s2, sizeof (TestIdl_MsgUnion));
}

static void sample_free_union (void *s)
{
  dds_free (s);
}

/**********************************************
 * Recursive types
 **********************************************/

struct TestIdl_SubMsgRecursive;

typedef struct TestIdl_SubMsgRecursive_submsg_field2_seq
{
  uint32_t _maximum;
  uint32_t _length;
  struct TestIdl_SubMsgRecursive *_buffer;
  bool _release;
} TestIdl_SubMsgRecursive_submsg_field2_seq;

typedef struct TestIdl_SubMsgRecursive
{
  uint32_t submsg_field1;
  TestIdl_SubMsgRecursive_submsg_field2_seq submsg_field2;
  int32_t submsg_field3;
} TestIdl_SubMsgRecursive;

typedef struct TestIdl_MsgRecursive
{
  uint32_t msg_field1;
  TestIdl_SubMsgRecursive msg_field2;
  int32_t msg_field3;
} TestIdl_MsgRecursive;

static const uint32_t TestIdl_MsgRecursive_ops [] =
{
  // MsgRecursive
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_MsgRecursive, msg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgRecursive, msg_field2), (3u << 16u) + 6u,   // SubMsg
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_MsgRecursive, msg_field3),
  DDS_OP_RTS,

  // SubMsgRecursive
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgRecursive, submsg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_SubMsgRecursive, submsg_field2), sizeof (TestIdl_SubMsgRecursive), (6u << 16u) + 4u,   // sequence<SubMsgRecursive>
    DDS_OP_JSR | (65536 - 6), DDS_OP_RTS,   // SubMsgRecursive
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_SubMsgRecursive, submsg_field3),
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgRecursive_desc = { sizeof (TestIdl_MsgRecursive), 4u, DDS_TOPIC_NO_OPTIMIZE, 0u, "TestIdl::MsgRecursive", NULL, 18, TestIdl_MsgRecursive_ops, "" };

static void * sample_init_recursive ()
{
  TestIdl_SubMsgRecursive sseq[] = { { .submsg_field1 = 1, .submsg_field3 = 2 }, { .submsg_field1 = 3, .submsg_field3 = 3 } };
  TestIdl_SubMsgRecursive_submsg_field2_seq seq = { ._length = 2, ._maximum = 2, ._buffer = ddsrt_memdup (sseq, 2 * sizeof (TestIdl_SubMsgRecursive)) };
  TestIdl_SubMsgRecursive s1 = { .submsg_field1 = 5, .submsg_field2 = seq, .submsg_field3 = 6 };
  TestIdl_MsgRecursive msg = { .msg_field1 = 1, .msg_field2 = s1, .msg_field3 = 3};
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgRecursive));
}

static bool sample_equal_recursive (void *s1, void *s2)
{
  TestIdl_MsgRecursive *msg1 = (TestIdl_MsgRecursive *) s1, *msg2 = (TestIdl_MsgRecursive *) s2;
  return (msg1->msg_field1 == msg2->msg_field1
    && msg1->msg_field2.submsg_field1 == msg2->msg_field2.submsg_field1
    && msg1->msg_field2.submsg_field3 == msg2->msg_field2.submsg_field3
    && msg1->msg_field2.submsg_field2._length == msg2->msg_field2.submsg_field2._length
    && !memcmp (msg1->msg_field2.submsg_field2._buffer, msg2->msg_field2.submsg_field2._buffer, msg1->msg_field2.submsg_field2._length * sizeof (TestIdl_SubMsgRecursive))
    && msg1->msg_field3 == msg2->msg_field3);
}

static void sample_free_recursive (void *s)
{
  TestIdl_MsgRecursive *msg = (TestIdl_MsgRecursive *) s;
  dds_free (msg->msg_field2.submsg_field2._buffer);
  dds_free (s);
}

/**********************************************
 * Appendable types
 **********************************************/

/* @appendable */
typedef struct TestIdl_AppendableUnion0
{
  int32_t _d;
  union
  {
    uint32_t field1;
    int32_t field2;
  } _u;
} TestIdl_AppendableUnion0;

/* @appendable */
typedef struct TestIdl_AppendableSubMsg1
{
  uint32_t submsg1_field1;
  char *submsg1_field2;
} TestIdl_AppendableSubMsg1;

/* @appendable */
typedef struct TestIdl_AppendableSubMsg2
{
  uint32_t submsg2_field1;
  uint32_t submsg2_field2;
} TestIdl_AppendableSubMsg2;

/* @appendable */
typedef struct TestIdl_AppendableMsg_msg_field3_seq
{
  uint32_t _maximum;
  uint32_t _length;
  struct TestIdl_AppendableSubMsg2 *_buffer;
  bool _release;
} TestIdl_AppendableMsg_msg_field3_seq;

/* @appendable */
typedef struct TestIdl_AppendableMsg_msg_field5_seq
{
  uint32_t _maximum;
  uint32_t _length;
  struct TestIdl_AppendableUnion0 *_buffer;
  bool _release;
} TestIdl_AppendableMsg_msg_field5_seq;

/* @appendable */
typedef struct TestIdl_AppendableMsg
{
  TestIdl_AppendableSubMsg1 msg_field1;
  TestIdl_AppendableSubMsg2 msg_field2;
  TestIdl_AppendableMsg_msg_field3_seq msg_field3;
  TestIdl_AppendableUnion0 msg_field4;
  TestIdl_AppendableMsg_msg_field5_seq msg_field5;
} TestIdl_AppendableMsg;

static const uint32_t TestIdl_AppendableMsg_ops [] =
{
  /* AppendableMsg */
  DDS_OP_XCDR2_DLH,
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_AppendableMsg, msg_field1), (3u << 16u) + 18u,  // AppendableSubMsg1
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_AppendableMsg, msg_field2), (3u << 16u) + 21u, // AppendableSubMsg2
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU , offsetof (TestIdl_AppendableMsg, msg_field3), sizeof (TestIdl_AppendableSubMsg2), (4u << 16u) + 18u,  // sequence<AppendableSubMsg2>
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_AppendableMsg, msg_field4), (3u << 16u) + 20u,  // AppendableUnion0
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_UNI, offsetof (TestIdl_AppendableMsg, msg_field5), sizeof (TestIdl_AppendableUnion0), (4u << 16u) + 17u,   // sequenec<AppendableUnion0>
  DDS_OP_RTS,

  /* AppendableSubMsg1 */
  DDS_OP_XCDR2_DLH,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_AppendableSubMsg1, submsg1_field1),
  DDS_OP_ADR | DDS_OP_TYPE_STR, offsetof (TestIdl_AppendableSubMsg1, submsg1_field2),
  DDS_OP_RTS,

  /* AppendableSubMsg2 */
  DDS_OP_XCDR2_DLH,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_AppendableSubMsg2, submsg2_field1),
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_AppendableSubMsg2, submsg2_field2),
  DDS_OP_RTS,

  /* AppendableUnion0 */
  DDS_OP_XCDR2_DLH,
  DDS_OP_ADR | DDS_OP_TYPE_UNI | DDS_OP_SUBTYPE_1BY, offsetof (TestIdl_AppendableUnion0, _d), 2u, (10u << 16) + 4u,
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | 0, 0, offsetof (TestIdl_AppendableUnion0, _u.field1),
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN | 0, 1, offsetof (TestIdl_AppendableUnion0, _u.field2),
  DDS_OP_RTS,
};

const dds_topic_descriptor_t TestIdl_MsgAppendable_desc = { sizeof (TestIdl_AppendableMsg), 4u, DDS_TOPIC_NO_OPTIMIZE, 0u, "TestIdl::AppendableMsg", NULL, 4, TestIdl_AppendableMsg_ops, "" };

static void * sample_init_appendable ()
{
  TestIdl_AppendableSubMsg2 sseq[] = { { .submsg2_field1 = 111, .submsg2_field2 = 222 }, { .submsg2_field1 = 333, .submsg2_field2 = 444 } };
  TestIdl_AppendableUnion0 useq[] = { { ._d = 0, ._u.field1 = 555 }, { ._d = 1, ._u.field2 = -555 }, { ._d = 0, ._u.field1 = 666 } };
  TestIdl_AppendableMsg msg = {
          .msg_field1 = { .submsg1_field1 = 1100, .submsg1_field2 = "test0123" },
          .msg_field2 = { .submsg2_field1 = 2100, .submsg2_field2 = 2200 },
          .msg_field3 = { ._length = 2, ._maximum = 2, ._buffer = ddsrt_memdup (sseq, 2 * sizeof (TestIdl_AppendableSubMsg2)) },
          .msg_field4 = { ._d = 1, ._u.field2 = -10 },
          .msg_field5 = { ._length = 3, ._maximum = 3, ._buffer = ddsrt_memdup (useq, 3 * sizeof (TestIdl_AppendableUnion0)) }
  };
  return ddsrt_memdup (&msg, sizeof (TestIdl_AppendableMsg));
}

static bool sample_equal_appendable (void *s1, void *s2)
{
  TestIdl_AppendableMsg *msg1 = (TestIdl_AppendableMsg *) s1, *msg2 = (TestIdl_AppendableMsg *) s2;
  return (
    msg1->msg_field1.submsg1_field1 == msg2->msg_field1.submsg1_field1
    && !strcmp (msg1->msg_field1.submsg1_field2, msg2->msg_field1.submsg1_field2)
    && !memcmp (&msg1->msg_field2, &msg2->msg_field2, sizeof (msg2->msg_field2))
    && msg1->msg_field3._length == msg2->msg_field3._length
    && !memcmp (msg1->msg_field3._buffer, msg2->msg_field3._buffer, msg1->msg_field3._length * sizeof (TestIdl_AppendableSubMsg2))
    && !memcmp (&msg1->msg_field4, &msg2->msg_field4, sizeof (msg2->msg_field4))
    && msg1->msg_field5._length == msg2->msg_field5._length
    && !memcmp (msg1->msg_field5._buffer, msg2->msg_field5._buffer, msg1->msg_field5._length * sizeof (TestIdl_AppendableUnion0))
  );
}

static void sample_free_appendable (void *s)
{
  TestIdl_AppendableMsg *msg = (TestIdl_AppendableMsg *) s;
  dds_free (msg->msg_field3._buffer);
  dds_free (msg->msg_field5._buffer);
  dds_free (s);
}

/**********************************************
 * Keys in nested types
 **********************************************/

typedef struct TestIdl_SubMsgKeysNested2
{
  uint32_t submsg2_field1;
  uint32_t submsg2_field2;
} TestIdl_SubMsgKeysNested2;

typedef struct TestIdl_SubMsgKeysNested
{
  uint32_t submsg_field1;
  uint32_t submsg_field2;
  uint32_t submsg_field3;
  TestIdl_SubMsgKeysNested2 submsg_field4;
} TestIdl_SubMsgKeysNested;

typedef struct TestIdl_MsgKeysNested_msg_field2_seq
{
  uint32_t _maximum;
  uint32_t _length;
  TestIdl_SubMsgKeysNested *_buffer;
  bool _release;
} TestIdl_MsgKeysNested_msg_field2_seq;

typedef struct TestIdl_MsgKeysNested
{
  TestIdl_SubMsgKeysNested msg_field1;
  TestIdl_MsgKeysNested_msg_field2_seq msg_field2;
} TestIdl_MsgKeysNested;

static const uint32_t TestIdl_MsgKeysNested_ops [] =
{
  // Msg
  DDS_OP_ADR | DDS_OP_TYPE_EXT | DDS_OP_FLAG_KEY, offsetof (TestIdl_MsgKeysNested, msg_field1), (3u << 16u) + 8u,  // SubMsgKeysNested
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgKeysNested, msg_field2), sizeof (TestIdl_SubMsgKeysNested), (4u << 16u) + 5u, // sequence<SubMsgKeysNested>
  DDS_OP_RTS,

  // SubMsg
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgKeysNested, submsg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY, offsetof (TestIdl_SubMsgKeysNested, submsg_field2),
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY, offsetof (TestIdl_SubMsgKeysNested, submsg_field3),
  DDS_OP_ADR | DDS_OP_TYPE_EXT | DDS_OP_FLAG_KEY, offsetof (TestIdl_SubMsgKeysNested, submsg_field4), (3u << 16) + 4u, // SubMsgKeysNested2
  DDS_OP_RTS,

  // SubMsg2
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgKeysNested2, submsg2_field1),
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY, offsetof (TestIdl_SubMsgKeysNested2, submsg2_field2),
  DDS_OP_RTS,

  DDS_OP_KOF | 2u, 0u, 2u,      // msg_field1.submsg_field2
  DDS_OP_KOF | 2u, 0u, 4u,      // msg_field1.submsg_field3
  DDS_OP_KOF | 3u, 0u, 6u, 2u   // msg_field1.submsg_field4.submsg2_field2
};

static const dds_key_descriptor_t TestIdl_MsgKeysNested_keys[3] =
{
  { "msg_field1.submsg_field2", 23 },
  { "msg_field1.submsg_field3", 26 },
  { "msg_field1.submsg_field3", 29 }
};

const dds_topic_descriptor_t TestIdl_MsgKeysNested_desc = { sizeof (TestIdl_MsgKeysNested), sizeof (char *), DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE, 3u, "TestIdl::MsgKeysNested", TestIdl_MsgKeysNested_keys, 8, TestIdl_MsgKeysNested_ops, "" };

static void * sample_init_keysnested ()
{
  TestIdl_SubMsgKeysNested sseq[] = { { .submsg_field1 = 2100, .submsg_field2 = 2200, .submsg_field3 = 2300, .submsg_field4.submsg2_field1 = 2310, .submsg_field4.submsg2_field2 = 2320 },
      { .submsg_field1 = 2101, .submsg_field2 = 2201, .submsg_field3 = 2301, .submsg_field4.submsg2_field1 = 2411, .submsg_field4.submsg2_field2 = 2412 } };
  TestIdl_MsgKeysNested msg = {
          .msg_field1 = { .submsg_field1 = 1100, .submsg_field2 = 1200, .submsg_field3 = 1300, .submsg_field4.submsg2_field1 = 1410, .submsg_field4.submsg2_field2 = 1420 },
          .msg_field2 = { ._length = 2, ._maximum = 2, ._buffer = ddsrt_memdup (sseq, 2 * sizeof (TestIdl_SubMsgKeysNested)) }
  };
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgKeysNested));
}

static bool sample_equal_keysnested (void *s1, void *s2)
{
  TestIdl_MsgKeysNested *msg1 = (TestIdl_MsgKeysNested *) s1, *msg2 = (TestIdl_MsgKeysNested *) s2;
  return
      !memcmp (&msg1->msg_field1, &msg2->msg_field1, sizeof (TestIdl_SubMsgKeysNested))
      && msg1->msg_field2._length == msg2->msg_field2._length
      && !memcmp (msg1->msg_field2._buffer, msg2->msg_field2._buffer, msg1->msg_field2._length * sizeof (TestIdl_SubMsgKeysNested));
}

static void sample_free_keysnested (void *s)
{
  TestIdl_MsgKeysNested *msg = (TestIdl_MsgKeysNested *) s;
  dds_free (msg->msg_field2._buffer);
  dds_free (s);
}


/**********************************************
 * Generic implementation and tests
 **********************************************/

static int topic_nr = 0;

static void sync_reader_writer (dds_entity_t participant_rd, dds_entity_t reader, dds_entity_t participant_wr, dds_entity_t writer)
{
  dds_attach_t triggered;
  dds_entity_t waitset_rd = dds_create_waitset (participant_rd);
  dds_entity_t waitset_wr = dds_create_waitset (participant_wr);

  /* Sync reader to writer. */
  dds_set_status_mask (reader, DDS_SUBSCRIPTION_MATCHED_STATUS);
  dds_waitset_attach (waitset_rd, reader, reader);
  dds_waitset_wait (waitset_rd, &triggered, 1, DDS_SECS (1));
  dds_waitset_detach (waitset_rd, reader);
  dds_delete (waitset_rd);

  /* Sync writer to reader. */
  dds_set_status_mask (writer, DDS_PUBLICATION_MATCHED_STATUS);
  dds_waitset_attach (waitset_wr, writer, writer);
  dds_waitset_wait (waitset_wr, &triggered, 1, DDS_SECS (1));
  dds_waitset_detach (waitset_wr, writer);
  dds_delete (waitset_wr);
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

static dds_entity_t tp1, tp2, dp1, dp2, rd, wr, ws;

static void cdrstream_init ()
{
  char * conf = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID1);
  dds_entity_t d1 = dds_create_domain (DDS_DOMAINID1, conf);
  CU_ASSERT_FATAL (d1 > 0);
  dds_free (conf);
  conf = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID2);
  dds_entity_t d2 = dds_create_domain (DDS_DOMAINID2, conf);
  CU_ASSERT_FATAL (d2 > 0);
  dds_free (conf);

  dp1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_FATAL (dp1 > 0);
  dp2 = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (dp2 > 0);
}

static void entity_init (const dds_topic_descriptor_t *desc)
{
  char * name;
  ddsrt_asprintf (&name, "ddsc_cdrstream_%d", topic_nr++);
  tp1 = dds_create_topic (dp1, desc, name, NULL, NULL);
  CU_ASSERT_FATAL (tp1 > 0);
  tp2 = dds_create_topic (dp2, desc, name, NULL, NULL);
  CU_ASSERT_FATAL (tp2 > 0);
  dds_free (name);
  rd = dds_create_reader (dp2, tp2, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);
  wr = dds_create_writer (dp1, tp1, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  sync_reader_writer (dp2, rd, dp1, wr);

  dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS);
  ws = dds_create_waitset (dp2);
  dds_waitset_attach (ws, rd, rd);
}

static void write_key_sample (void * msg)
{
  struct dds_topic *x;
  if (dds_topic_pin (tp1, &x) < 0) abort();
  struct ddsi_sertype *stype = ddsi_sertype_ref (x->m_stype);
  dds_topic_unpin (x);
  struct ddsi_serdata *sd = ddsi_serdata_from_sample (stype, SDK_KEY, msg);
  ddsi_serdata_unref (sd);
  ddsi_sertype_unref (stype);
}

static void cdrstream_fini ()
{
  dds_delete (dp1);
  dds_delete (dp2);
}

#define D(n) TestIdl_Msg ## n ## _desc
#define I(n) sample_init_ ## n
#define C(n) sample_equal_ ## n
#define F(n) sample_free_ ## n
CU_TheoryDataPoints (ddsc_cdrstream, ser_des) = {
  CU_DataPoints (const char *,                   "nested structs",
  /*                                             |          */"string types",
  /*                                             |           |       */"unions",
  /*                                             |           |        |         */"recursive",
  /*                                             |           |        |          |             */"appendable",
  /*                                             |           |        |          |              |              */"keys nested" ),
  CU_DataPoints (const dds_topic_descriptor_t *, &D(Nested), &D(Str), &D(Union), &D(Recursive), &D(Appendable), &D(KeysNested) ),
  CU_DataPoints (sample_init *,                  I(nested),  I(str),  I(union),  I(recursive),  I(appendable),  I(keysnested)  ),
  CU_DataPoints (sample_equal *,                 C(nested),  C(str),  C(union),  C(recursive),  C(appendable),  C(keysnested)  ),
  CU_DataPoints (sample_free *,                  F(nested),  F(str),  F(union),  F(recursive),  F(appendable),  F(keysnested)  ),
};
#undef D
#undef I
#undef C
#undef F
CU_Theory ((const char *descr, const dds_topic_descriptor_t *desc, sample_init *sample_init_fn, sample_equal *sample_equal_fn, sample_free *sample_free_fn),
    ddsc_cdrstream, ser_des, .init = cdrstream_init, .fini = cdrstream_fini)
{
  dds_return_t ret;
  msg ("Running test: %s", descr);

  entity_init (desc);

  void * msg = sample_init_fn ();

  if (desc->m_nkeys > 0)
    write_key_sample (msg);

  ret = dds_write (wr, msg);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  dds_attach_t triggered;
  ret = dds_waitset_wait (ws, &triggered, 1, DDS_SECS(5));
  CU_ASSERT_EQUAL_FATAL (ret, 1);

  void * rds[1] = { NULL };
  dds_sample_info_t si[1];
  ret = dds_read (rd, rds, si, 1, 1);
  CU_ASSERT_EQUAL_FATAL (ret, 1);
  bool eq = sample_equal_fn (msg, rds[0]);
  CU_ASSERT_FATAL (eq);

  // cleanup
  sample_free_fn (msg);
  dds_return_loan (rd, rds, 1);
}
