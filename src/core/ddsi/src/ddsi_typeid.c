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
#include "dds/ddsi/ddsi_typeid.h"
#include "dds/ddsi/ddsi_sertype.h"

type_identifier_t * ddsi_typeid_from_sertype (const struct ddsi_sertype *type)
{
  assert (type != NULL);
  type_identifier_t *type_id = ddsrt_malloc (sizeof (*type_id));
  type->ops->typeid_hash (type, type_id->hash);
  return type_id;
}

type_identifier_t * ddsi_typeid_dup (const type_identifier_t *type_id)
{
  assert (type_id);
  type_identifier_t *t = ddsrt_malloc (sizeof (*t));
  memcpy (&t->hash, type_id->hash, sizeof (t->hash));
  return t;
}

bool ddsi_typeid_equal (const type_identifier_t *a, const type_identifier_t *b)
{
  return memcmp (a->hash, b->hash, sizeof (*a)) == 0;
}

bool ddsi_typeid_none (const type_identifier_t *typeid)
{
  type_identifier_t empty;
  memset (&empty, 0, sizeof (*typeid));
  return memcmp (typeid, &empty, sizeof (*typeid)) == 0;
}
