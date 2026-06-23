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

# --- VPI header: vpi_user.h (IEEE 1364). cocotb does NOT ship the base header
# (only vpi_user_ext.h) — it relies on the simulator's. Since *we* are the
# simulator, we need one. TODO: vendor the IEEE-1364 reference header so the
# repo is self-contained (as Verilator/Icarus do). For now, auto-discover a
# system copy; override with -DVPI_INCLUDE_DIR=<dir>.
find_path(VPI_INCLUDE_DIR vpi_user.h
  HINTS
    /usr/local/share/verilator/include/vltstd
    /usr/local/Cellar/icarus-verilog/*/include/iverilog
  PATH_SUFFIXES vltstd iverilog
)

# model.cc only needs cxxrtl_capi and always builds. vpi_provider.cc needs
# vpi_user.h; include it only when a header is available.
set(_provider_srcs src/model.cc)
if(VPI_INCLUDE_DIR)
  message(STATUS "vpi_user.h: ${VPI_INCLUDE_DIR}")
  list(APPEND _provider_srcs src/vpi_provider.cc)
else()
  message(WARNING
    "vpi_user.h not found: building model.cc only (VPI provider skipped). "
    "Pass -DVPI_INCLUDE_DIR=<dir> with a vpi_user.h to build the full library.")
endif()

add_library(cxxrtl_vpi STATIC ${_provider_srcs})
target_include_directories(cxxrtl_vpi PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CXXRTL_INCLUDE_DIR}
)
if(VPI_INCLUDE_DIR)
  target_include_directories(cxxrtl_vpi PUBLIC ${VPI_INCLUDE_DIR})
endif()

# NOTE: harness.cc carries main() and must be compiled *together with* a
# per-design generated model (write_cxxrtl output) and linked against the
# cocotb VPI consumer. That step lives in examples/cocotb_counter/Makefile,
# which is the prototype of the cocotb-side glue. We expose the sources here:
set(CXXRTL_VPI_HARNESS ${CMAKE_CURRENT_SOURCE_DIR}/src/harness.cc
    CACHE FILEPATH "harness main() to link with a generated model")

install(TARGETS cxxrtl_vpi ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
