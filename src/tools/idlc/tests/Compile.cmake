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

# Use C_INCLUDE_PATH environment variable to forward directories with GCC
# Use INCLUDE environment variable to forward directories with MSVC
#string(REGEX REPLACE ":+" ";" _paths "$ENV{C_INCLUDE_PATH};$ENV{INCLUDE}")

if(WIN32)
  set(_separator ";")
else()
  set(_separator ":")
endif()

foreach(_path $ENV{C_INCLUDE_PATH} $ENV{INCLUDE} ${CMAKE_CURRENT_BINARY_DIR})
  if(_c_include_path)
    set(_c_include_path "${_c_include_path}${_separator}${_path}")
  else()
    set(_c_include_path "${_path}")
  endif()
endforeach()

# set compiler specific values
if(_c_compiler MATCHES "cl\\.exe$")
  set(ENV{INCLUDE} "${_c_include_path}")
  set(_output_flag "/Fe")
else()
  set(ENV{C_INCLUDE_PATH} "${_c_include_path}")
  set(_output_flag "-o")
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

# C:\Program Files (x86)\Windows Kits\NETFXSDK\4.8\lib\um\x86;
# LIB=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.28.29333\ATLMFC\lib\x86;

#set(ENV{LIB} "C:\Program Files (x86)\Windows Kits\10\lib\10.0.18362.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\lib\10.0.18362.0\um\x64")
#set(ENV{LIB} "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.28.29333\lib\x64;C:\Program Files (x86)\Windows Kits\10\lib\10.0.18362.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\lib\10.0.18362.0\um\x64")

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
    COMMAND "${_c_compiler}" ${_output_flag}${_base} "${_main}" "${_c}" "-lddsc"
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
