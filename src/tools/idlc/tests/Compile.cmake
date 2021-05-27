#
# Copyright(c) 2021 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
set(_c_compiler "$ENV{CC}")
set(_idl_compiler "$ENV{IDLC}")

if(NOT _c_compiler)
  message(FATAL_ERROR "C compiler not set")
endif()
if(NOT _idl_compiler)
  message(FATAL_ERROR "IDL compiler not set")
endif()

if(WIN32)
  set(_separator ";")
else()
  set(_separator ":")
endif()

foreach(_path $ENV{TEST_INCLUDE_PATHS})
  if(_include_paths)
    set(_include_paths "${_include_paths}${_separator}${_path}")
  else()
    set(_include_paths "${_path}")
  endif()
endforeach()

# set compiler specific values
if(_c_compiler MATCHES "cl\\.exe$")
  set(ENV{INCLUDE} "${_include_paths}")
  set(ENV{PATH} "$ENV{PATH};$ENV{CDDS_BIN_PATH}")
  set(_output_flag "/Fe")
  set(_lib_path "/link /LIBPATH:\"$ENV{CDDS_LIB_PATH}\"")
  foreach(_path $ENV{TEST_LIB_PATHS})
    set(_lib_path "${_lib_path} /LIBPATH:\"${_path}\"")
  endforeach()
  set(_lib "ddsc.lib")
else()
  set(ENV{C_INCLUDE_PATH} "${_include_paths}")
  set(ENV{LD_LIBRARY_PATH} "$ENV{LD_LIBRARY_PATH}:$ENV{CDDS_LIB_PATH}")
  set(ENV{LIBRARY_PATH} "$ENV{LIBRARY_PATH}:$ENV{CDDS_LIB_PATH}")
  set(_output_flag "-o")
  set(_lib_path "")
  set(_lib "-lddsc")
endif()

# get list of sources from argument list
set(_skip_next FALSE)
foreach(_argno RANGE 1 ${CMAKE_ARGC}) # skip executable
  if(_skip_next)
    set(_skip_next FALSE)
    continue()
  endif()

  set(_arg "${CMAKE_ARGV${_argno}}")
  if(_arg MATCHES "^-P")
    set(_skip_next TRUE)
  else()
    list(APPEND _sources "${_arg}")
  endif()
endforeach()
if(NOT _sources)
  message(FATAL_ERROR "no source files specified")
endif()

foreach(_source ${_sources})
  get_filename_component(_base "${_source}" NAME_WE)
  set(_main "${CMAKE_CURRENT_BINARY_DIR}/${_base}.main.c")
  set(_c "${CMAKE_CURRENT_BINARY_DIR}/${_base}.c")
  configure_file("${_source}" ${_main})

  # idl compile the idl file
  execute_process(
    COMMAND "${_idl_compiler}" "${_source}"
    COMMAND_ECHO STDOUT
    RESULT_VARIABLE _result)
  if(NOT _result EQUAL "0")
    message(FATAL_ERROR "Cannot transpile ${_source} to source code")
  endif()

  # compile and link c files
  execute_process(
    COMMAND "${_c_compiler}" ${_output_flag}${_base} ${_main} ${_c} ${_lib} ${_lib_path}
    COMMAND_ECHO STDOUT
    RESULT_VARIABLE _result)
  if(NOT _result EQUAL "0")
    message(FATAL_ERROR "Cannot compile ${_source}")
  endif()

  # execute test process
  execute_process(
    COMMAND "${CMAKE_CURRENT_BINARY_DIR}/${_base}"
    COMMAND_ECHO STDOUT
    RESULT_VARIABLE _result
    ERROR_VARIABLE _error)
  if(NOT _result EQUAL "0")
    message(FATAL_ERROR "Test failed ${_source}: ${_result} ${_error}")
  endif()

endforeach()
