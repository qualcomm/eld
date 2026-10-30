cmake_minimum_required(VERSION 3.20)

# Keep package discovery independent of the invoking user's environment.
set(ENV{ELD_ROOT} "")

# Configuration-only tests; fixtures are linker-script placeholders, not host
# binaries. All file creation is confined to the test's build-tree directory.
file(MAKE_DIRECTORY "${TEST_ROOT}/package one/Tools/lib"
                    "${TEST_ROOT}/package two/lib64"
                    "${TEST_ROOT}/override lib" "${TEST_ROOT}/incomplete/lib")
foreach(_dir "package one/Tools/lib" "package two/lib64" "override lib")
  file(WRITE "${TEST_ROOT}/${_dir}/libc++.so" "INPUT(libc++.so.1 -lc++abi)\n")
  file(WRITE "${TEST_ROOT}/${_dir}/libc++abi.so" "")
endforeach()
file(WRITE "${TEST_ROOT}/incomplete/lib/libc++.so" "")

function(expect_flags prefix)
  foreach(kind EXE SHARED MODULE)
    if(NOT CMAKE_${kind}_LINKER_FLAGS STREQUAL "${prefix}-user-${kind}")
      message(FATAL_ERROR "Unexpected ${kind} flags: ${CMAKE_${kind}_LINKER_FLAGS}")
    endif()
  endforeach()
endfunction()

set(ELD_PACKAGE_ROOT "${TEST_ROOT}/package one")
foreach(kind EXE SHARED MODULE)
  set(CMAKE_${kind}_LINKER_FLAGS "-user-${kind}" CACHE STRING "" FORCE)
endforeach()
include("${RUNTIME_MODULE}")
if(NOT ELD_PACKAGE_CXX_RUNTIME_DIR STREQUAL "${TEST_ROOT}/package one/Tools/lib")
  message(FATAL_ERROR "Package runtime selection must be automatic on Linux")
endif()
expect_flags("-L\"${TEST_ROOT}/package one/Tools/lib\" ")
include("${RUNTIME_MODULE}")
expect_flags("-L\"${TEST_ROOT}/package one/Tools/lib\" ")

set(ELD_PACKAGE_ROOT "${TEST_ROOT}/package two")
include("${RUNTIME_MODULE}")
expect_flags("-L\"${TEST_ROOT}/package two/lib64\" ")

set(ELD_CXX_RUNTIME_DIR "${TEST_ROOT}/override lib" CACHE PATH "" FORCE)
include("${RUNTIME_MODULE}")
expect_flags("-L\"${TEST_ROOT}/override lib\" ")

# Other hosts retain native toolchain behavior, with no Linux runtime flags.
set(_saved_host_system_name "${CMAKE_HOST_SYSTEM_NAME}")
set(CMAKE_HOST_SYSTEM_NAME "Darwin")
include("${RUNTIME_MODULE}")
expect_flags("")
if(ELD_PACKAGE_CXX_RUNTIME_DIR)
  message(FATAL_ERROR "Non-Linux host retained a Linux runtime directory")
endif()
set(CMAKE_HOST_SYSTEM_NAME "${_saved_host_system_name}")

# Prefix-based discovery without ELD_PACKAGE_ROOT, and initialization from the
# standard linker-flags INIT variables before any language has been enabled.
set(ELD_CXX_RUNTIME_DIR "" CACHE PATH "" FORCE)
set(ELD_PACKAGE_ROOT "")
set(CMAKE_PREFIX_PATH "${TEST_ROOT}/package two")
set(ENV{LDFLAGS} "")
foreach(kind EXE SHARED MODULE)
  unset(CMAKE_${kind}_LINKER_FLAGS)
  unset(CMAKE_${kind}_LINKER_FLAGS CACHE)
  set(CMAKE_${kind}_LINKER_FLAGS_INIT "-user-${kind}")
endforeach()
include("${RUNTIME_MODULE}")
expect_flags("-L\"${TEST_ROOT}/package two/lib64\" ")

# ELD_ROOT and environment linker flags also work before language setup.
set(CMAKE_PREFIX_PATH "")
set(ELD_ROOT "${TEST_ROOT}/package one")
set(ENV{LDFLAGS} "-environment")
foreach(kind EXE SHARED MODULE)
  unset(CMAKE_${kind}_LINKER_FLAGS)
  unset(CMAKE_${kind}_LINKER_FLAGS CACHE)
endforeach()
include("${RUNTIME_MODULE}")
expect_flags("-L\"${TEST_ROOT}/package one/Tools/lib\" -environment ")

# An environment-only ELD_ROOT works too.
set(ELD_ROOT "")
set(ENV{ELD_ROOT} "${TEST_ROOT}/package two")
include("${RUNTIME_MODULE}")
expect_flags("-L\"${TEST_ROOT}/package two/lib64\" -environment ")
set(ENV{ELD_ROOT} "")

# Missing/incomplete package runtime must not silently choose the system copy.
foreach(case missing incomplete)
  execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DELD_PACKAGE_ROOT=${TEST_ROOT}/${case}" -P "${RUNTIME_MODULE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(result EQUAL 0 OR NOT "${output}${error}" MATCHES "requires.*libc[+][+]")
    message(FATAL_ERROR "Expected missing runtime diagnostic: ${output}${error}")
  endif()
endforeach()
message(STATUS "Package runtime configuration checks passed")
