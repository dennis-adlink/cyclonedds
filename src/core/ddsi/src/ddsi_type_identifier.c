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
#include "dds/ddsi/ddsi_type_xtypes.h"
#include "dds/ddsi/ddsi_sertype.h"

struct TypeIdentifier * ddsi_typeid_from_sertype (const struct ddsi_sertype *type)
{
  assert (type != NULL);
  // FIXME
  return NULL;
}

void ddsi_typeid_copy (struct TypeIdentifier *dst, const struct TypeIdentifier *src)
{
  assert (src);
  assert (dst);

  abort ();
  return t;
}

bool ddsi_typeid_equal (const struct TypeIdentifier *a, const struct TypeIdentifier *b)
{
  /* FIXME: implement compare */
  (void) a;
  (void) b;
  abort ();
  return false;
}

bool ddsi_typeid_none (const struct TypeIdentifier *typeid)
{
  if (typeid == NULL)
    return true;
  return typeid->_d == TK_NONE;
}
