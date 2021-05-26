#
# Copyright(c) 2006 to 2021 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

set(sources
  test_type1.idl)

# https://stackoverflow.com/questions/15074402/where-is-microsoft-windows-sdk-folder-located-in-windows-xp-and-windows-98/50662068

if(MSVC)
  # add the proper include directory or msvc can't find stdint.h
  # C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.28.29333/
  set(compiler "${CMAKE_C_COMPILER}")
  string(REGEX REPLACE "[\\/]bin[\\/].*$" "/include" includes "${compiler}")
  set(includes "${includes}\\;C:/Program Files (x86)/Windows Kits/10/Include/10.0.18362.0/ucrt")
endif()

set(includes "${includes}\\;${CMAKE_SOURCE_DIR}/src/core/ddsc/include")
set(includes "${includes}\\;${CMAKE_SOURCE_DIR}/src/core/ddsi/include")
set(includes "${includes}\\;${CMAKE_SOURCE_DIR}/src/ddsrt/include")
set(includes "${includes}\\;${CMAKE_BINARY_DIR}/src/core/include")

set(_dir "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(source ${sources})
  set(source "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
  get_filename_component(base "${source}" NAME_WE)
  add_test(
    NAME "idlc_${base}"
    COMMAND ${CMAKE_COMMAND} -P "${_dir}/Compile.cmake" ${source})
  set_property(TEST "idlc_${base}" APPEND PROPERTY ENVIRONMENT
    "CC=${CMAKE_C_COMPILER}"
    "IDLC=$<TARGET_FILE:CycloneDDS::idlc>"
    "C_INCLUDE_PATH=${includes}"
    "LIBRARY_PATH=$<TARGET_FILE_DIR:CycloneDDS::ddsc>")
endforeach()
