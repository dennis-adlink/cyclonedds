#
# Copyright(c) 2019 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
cmake_minimum_required(VERSION 3.5)
project(CycloneCdrTest)
find_package(CycloneDDS COMPONENTS idlc)

add_compile_options("-I${PROJECT_SOURCE_DIR}/src/ddsrt/include")
add_compile_options("-I${PROJECT_SOURCE_DIR}/src/core/ddsc/include")
add_compile_options("-I${PROJECT_SOURCE_DIR}/src/core/ddsc/src")
add_compile_options("-I${PROJECT_SOURCE_DIR}/src/core/ddsi/include")

add_compile_options("-I$ENV{OSPL_HOME}/src/abstraction/os/include")
add_compile_options("-I$ENV{OSPL_HOME}/src/database/database/include")
add_compile_options("-I$ENV{OSPL_HOME}/src/database/serialization/include")
add_compile_options("-I$ENV{OSPL_HOME}/src/utilities/include")
add_compile_options("-I$ENV{OSPL_HOME}/src/kernel/include")
add_compile_options("-I$ENV{OSPL_HOME}/src/kernel/bld/$ENV{SPLICE_TARGET}")
add_compile_options("-I$ENV{OSPL_HOME}/src/user/include")
add_compile_options("-I$ENV{OSPL_HOME}/src/api/dcps/sac/include")
add_compile_options("-I$ENV{OSPL_HOME}/src/api/dcps/sac/code")

# This is a convenience function, provided by the CycloneDDS package,
# that will supply a library target related the the given idl file.
# In short, it takes the idl file, generates the source files with
# the proper data types and compiles them into a library.
idlc_generate(TARGET xxx_lib FILES "xxx.idl")

add_executable(xxx xxx-cyc.c)
target_include_directories(xxx PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# Replace the vm instruction for bounded sequence
add_custom_target(xxx_c_replaced
                  COMMAND sed -i 's/DDS_OP_TYPE_BST/DDS_OP_TYPE_BSP/g' ${CMAKE_CURRENT_BINARY_DIR}/xxx.c
                  DEPENDS xxx_lib)

# Replace the type for bounded sequence fields
add_custom_target(xxx_h_replaced
                   COMMAND sed -i -E "'s/char[[:space:]]+([[:alnum:]]+)\[[[:digit:]]+\];/char \*\\1;/g'" ${CMAKE_CURRENT_BINARY_DIR}/xxx.h
                   DEPENDS xxx_lib)

add_dependencies(xxx xxx_c_replaced xxx_h_replaced)

# Both executables need to be linked to the idl data type library and
# the ddsc API library.
target_link_libraries(xxx xxx_lib CycloneDDS::ddsc -L$ENV{OSPL_HOME}/lib/$ENV{SPLICE_TARGET} -Wl,-rpath,$ENV{OSPL_HOME}/lib/$ENV{SPLICE_TARGET} ddskernel dcpssac)
