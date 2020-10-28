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
#include <dlfcn.h>
#include <assert.h>
#include <string.h>
#include <dds/ddsrt/dynlib.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"

dds_return_t ddsrt_dlopen (const char *name, bool translate, ddsrt_dynlib_t *handle)
{
  assert (handle);
  *handle = NULL;

  if (translate && strrchr (name, '/') == NULL)
  {
#if __APPLE__
    static const char suffix[] = ".dylib";
#else
    static const char suffix[] = ".so";
#endif
    char* lib_name;
    if (ddsrt_asprintf (&lib_name, "lib%s%s", name, suffix) == -1)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    *handle = dlopen (lib_name, RTLD_GLOBAL | RTLD_NOW);
    ddsrt_free (lib_name);
  }

  if (*handle == NULL)
  {
    /* name contains a path, (auto)translate is disabled or dlopen on translated name failed. */
    *handle = dlopen (name, RTLD_GLOBAL | RTLD_NOW);
  }

  return *handle != NULL ? DDS_RETCODE_OK : DDS_RETCODE_ERROR;
}

dds_return_t ddsrt_dlclose (ddsrt_dynlib_t handle)
{
  assert (handle);
  return (dlclose (handle) == 0) ? DDS_RETCODE_OK : DDS_RETCODE_ERROR;
}

dds_return_t ddsrt_dlsym (ddsrt_dynlib_t handle, const char *symbol, void **address)
{
  assert (handle);
  assert (address);
  assert (symbol);
  *address = dlsym (handle, symbol);
  return (*address == NULL) ? DDS_RETCODE_ERROR : DDS_RETCODE_OK;
}

