/*
 * Copyright(c) 2006 to 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "dds/ddsrt/heap.h"
#include "dds/ddsc/dds_public_impl.h"
#include "dds/ddsi/ddsi_type_information.h"
#include "dds/ddsi/ddsi_type_lookup.h"
#include "dds/ddsi/ddsi_type_identifier.h"

static bool type_id_with_size_equal (const struct TypeIdentifierWithSize *a, const struct TypeIdentifierWithSize *b)
{
  return ddsi_typeid_equal (&a->type_id, &b->type_id) && a->typeobject_serialized_size == b->typeobject_serialized_size;
}

static bool type_id_with_sizeseq_equal (const struct TypeIdentifierWithSizeSeq *a, const struct TypeIdentifierWithSizeSeq *b)
{
    if (a->length != b->length)
      return false;
    for (uint32_t n = 0; n < a->length; n++)
      if (!type_id_with_size_equal (&a->seq[n], &b->seq[n]))
        return false;
    return true;
}

static bool type_id_with_deps_equal (const struct TypeIdentifierWithDependencies *a, const struct TypeIdentifierWithDependencies *b)
{
  return type_id_with_size_equal (&a->typeid_with_size, &b->typeid_with_size)
    && a->dependent_typeid_count == b->dependent_typeid_count
    && type_id_with_sizeseq_equal (&a->dependent_typeids, &b->dependent_typeids);
}

bool ddsi_type_information_equal (const struct TypeInformation *a, const struct TypeInformation *b)
{
  if (a == NULL || b == NULL)
    return a == b;
  return type_id_with_deps_equal (&a->minimal, &b->minimal) && type_id_with_deps_equal (&a->complete, &b->complete);
}

struct TypeInformation *ddsi_type_information_lookup (struct ddsi_domaingv *gv, const struct TypeIdentifier *typeid)
{
  struct tl_meta *tlm = ddsi_tl_meta_lookup (gv, typeid);
  if (tlm == NULL)
    return NULL;
  struct TypeInformation *type_info = ddsrt_calloc (1, sizeof (*type_info));
  ddsi_typeid_copy (&type_info->minimal.typeid_with_size.type_id, &tlm->type_id);
  type_info->minimal.typeid_with_size.typeobject_serialized_size = 0; /* FIXME */
  if (ddsi_xt_has_complete_typeid (tlm->xt))
  {
    ddsi_typeid_copy (&type_info->complete.typeid_with_size.type_id, &tlm->type_id_complete);
    type_info->complete.typeid_with_size.typeobject_serialized_size = 0; /* FIXME */
  }
  return type_info;
}

void ddsi_type_information_copy (struct TypeInformation *dst, const struct TypeInformation *src)
{
  (void) dst;
  (void) src;
  // FIXME
}

void ddsi_type_information_free (struct TypeInformation *typeinfo)
{
  ddsrt_free (typeinfo);
}
