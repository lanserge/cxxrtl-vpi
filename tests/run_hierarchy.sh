#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Hierarchy-discovery test: the startup calls cocotb makes to find the toplevel
# and walk its signals (vpi_iterate/vpi_scan/vpi_handle_by_name).
set -euo pipefail
cd "$(dirname "$0")/.."

INC="$(yosys-config --datdir)/include/backends/cxxrtl/runtime"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

yosys -q -p "read_verilog examples/cocotb_counter/counter.v; \
             hierarchy -top counter; write_cxxrtl $WORK/counter_cxxrtl.cc"

c++ -std=c++14 -O2 -Iinclude -I"$INC" \
    "$WORK/counter_cxxrtl.cc" \
    "$INC/cxxrtl/capi/cxxrtl_capi.cc" \
    src/model.cc src/vpi_provider.cc tests/test_hierarchy.cc \
    -o "$WORK/test_hierarchy"

"$WORK/test_hierarchy"
