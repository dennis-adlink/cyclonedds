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
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_type_xtypes.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_cdrstream.h"

void ddsi_typeobj_ser (const struct TypeObject *typeobj, unsigned char **buf, uint32_t *sz)
{
  dds_ostream_t os = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = CDR_ENC_VERSION_2 };
  dds_stream_writeLE ((dds_ostreamLE_t *) &os, (const void *) typeobj, TypeObject_ops);
  *buf = os.m_buffer;
  *sz = os.m_index;
}

void ddsi_typeobj_deser (unsigned char *buf, uint32_t sz, struct TypeObject **typeobj)
{
  unsigned char *data;
  uint32_t srcoff = 0;
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  if (bswap)
  {
    data = ddsrt_memdup (buf, sz);
    dds_stream_normalize1 ((char *) data, &srcoff, sz, bswap, CDR_ENC_VERSION_2, TypeObject_ops);
  }
  else
    data = buf;

  dds_istream_t is = { .m_buffer = data, .m_index = 0, .m_size = sz, .m_xcdr_version = CDR_ENC_VERSION_2 };
  dds_stream_read (&is, (void *) *typeobj, TypeObject_ops);
}

bool ddsi_typeobj_is_minimal (const struct TypeObject *typeobj)
{
  return typeobj != NULL && typeobj->_d == EK_MINIMAL;
}

bool ddsi_typeobj_is_complete (const struct TypeObject *typeobj)
{
  return typeobj != NULL && typeobj->_d == EK_COMPLETE;
}
