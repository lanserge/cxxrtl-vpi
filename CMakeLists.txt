# SPDX-License-Identifier: ISC
cmake_minimum_required(VERSION 3.16)
project(cxxrtl_vpi CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Locate the CXXRTL runtime include dir (contains cxxrtl/capi/cxxrtl_capi.h)
# Derived from `yosys-config --datdir`, overridable with -DCXXRTL_INCLUDE_DIR=...
if(NOT CXXRTL_INCLUDE_DIR)
  find_program(YOSYS_CONFIG yosys-config)
  if(YOSYS_CONFIG)
    execute_process(COMMAND ${YOSYS_CONFIG} --datdir
                    OUTPUT_VARIABLE YOSYS_DATDIR
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(CXXRTL_INCLUDE_DIR "${YOSYS_DATDIR}/include/backends/cxxrtl/runtime")
  endif()
endif()

if(NOT EXISTS "${CXXRTL_INCLUDE_DIR}/cxxrtl/capi/cxxrtl_capi.h")
  message(FATAL_ERROR
    "CXXRTL runtime headers not found. Pass -DCXXRTL_INCLUDE_DIR=<dir> "
    "(the directory containing cxxrtl/capi/cxxrtl_capi.h), or ensure "
    "yosys-config is on PATH.")
endif()
message(STATUS "CXXRTL runtime include: ${CXXRTL_INCLUDE_DIR}")

# --- VPI header: we ship a self-contained, minimal IEEE-1364/1800 vpi_user.h
# under include/ (ISC). To build against a full system header instead, pass
# -DVPI_INCLUDE_DIR=<dir>; it is placed *ahead* of our include/ so its include
# guard wins and ours is skipped (no clashes). The constant values and struct
# layouts are normative, so the two agree by construction — see
# tests/test_conformance.cc which asserts this.
add_library(cxxrtl_vpi STATIC src/model.cc src/vpi_provider.cc)
if(VPI_INCLUDE_DIR)
  message(STATUS "vpi_user.h: overridden by ${VPI_INCLUDE_DIR}")
  target_include_directories(cxxrtl_vpi BEFORE PUBLIC ${VPI_INCLUDE_DIR})
else()
  message(STATUS "vpi_user.h: bundled (include/vpi_user.h)")
endif()
target_include_directories(cxxrtl_vpi PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CXXRTL_INCLUDE_DIR}
)

# NOTE: harness.cc carries main() and must be compiled *together with* a
# per-design generated model (write_cxxrtl output) and linked against the
# cocotb VPI consumer. That step lives in examples/cocotb_counter/Makefile,
# which is the prototype of the cocotb-side glue. We expose the sources here:
set(CXXRTL_VPI_HARNESS ${CMAKE_CURRENT_SOURCE_DIR}/src/harness.cc
    CACHE FILEPATH "harness main() to link with a generated model")

install(TARGETS cxxrtl_vpi ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
