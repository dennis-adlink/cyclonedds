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
#include <stdio.h>
#include <assert.h>
#include <dds/ddsrt/dynlib.h>
#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/io.h"

dds_return_t ddsrt_dlopen (const char *name, bool translate, ddsrt_dynlib_t *handle)
{
  assert (handle);
  *handle = NULL;

  if (translate && (strrchr (name, '/') == NULL && strrchr (name, '\\') == NULL))
  {
    static const char suffix[] = ".dll";
    char *lib_name;
    if (ddsrt_asprintf (&lib_name, "%s%s", name, suffix) == -1)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    *handle = (ddsrt_dynlib_t) LoadLibrary (lib_name);
    ddsrt_free (lib_name);
  }

  if (*handle == NULL)
  {
    /* Name contains a path, (auto)translate is disabled or LoadLibrary on translated name failed. */
    *handle = (ddsrt_dynlib_t) LoadLibrary (name);
  }

  return (*handle != NULL) ? DDS_RETCODE_OK : DDS_RETCODE_ERROR;
}

dds_return_t ddsrt_dlclose (ddsrt_dynlib_t handle)
{
  assert (handle);
  return (FreeLibrary ((HMODULE) handle) == 0) ? DDS_RETCODE_ERROR : DDS_RETCODE_OK;
}

dds_return_t ddsrt_dlsym (ddsrt_dynlib_t handle, const char *symbol, void **address)
{
  assert (handle);
  assert (address);
  assert (symbol);

  *address = GetProcAddress ((HMODULE) handle, symbol);
  return (*address == NULL) ? DDS_RETCODE_ERROR : DDS_RETCODE_OK;
}
