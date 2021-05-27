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

set(_sources
  test_type1.idl)

set(_includes "${_includes}\\;${CMAKE_SOURCE_DIR}/src/core/ddsc/include")
set(_includes "${_includes}\\;${CMAKE_SOURCE_DIR}/src/core/ddsi/include")
set(_includes "${_includes}\\;${CMAKE_SOURCE_DIR}/src/ddsrt/include")
set(_includes "${_includes}\\;${CMAKE_BINARY_DIR}/src/core/include")

set(_dir "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(_source ${_sources})
  set(_source "${CMAKE_CURRENT_SOURCE_DIR}/${_source}")
  get_filename_component(_base "${_source}" NAME_WE)
  add_test(
    NAME "idlc_${_base}"
    COMMAND ${CMAKE_COMMAND} -P "${_dir}/Compile.cmake" ${_source})
  set_property(
      TEST "idlc_${_base}"
      APPEND PROPERTY ENVIRONMENT
        "CC=${CMAKE_C_COMPILER}"
        "IDLC=$<TARGET_FILE:CycloneDDS::idlc>"
        "C_INCLUDE_PATH=${_includes}"
        "LIBRARY_PATH=$<TARGET_LINKER_FILE_DIR:CycloneDDS::ddsc>"
        "CDDS_BIN=$<TARGET_FILE_DIR:CycloneDDS::ddsc>")
endforeach()
