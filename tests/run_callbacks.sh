#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Callback/time/control test: builds the provider against a generated CXXRTL
# model and drives it with the same VPI callback pattern cocotb uses (clock via
# cbAfterDelay, edges via cbValueChange, finish via vpi_control).
set -euo pipefail
cd "$(dirname "$0")/.."

INC="$(yosys-config --datdir)/include/backends/cxxrtl/runtime"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "== generate CXXRTL model =="
yosys -q -p "read_verilog examples/cocotb_counter/counter.v; \
             hierarchy -top counter; write_cxxrtl $WORK/counter_cxxrtl.cc"

echo "== build callback test =="
c++ -std=c++14 -O2 -Iinclude -I"$INC" \
    "$WORK/counter_cxxrtl.cc" \
    "$INC/cxxrtl/capi/cxxrtl_capi.cc" \
    "$INC/cxxrtl/capi/cxxrtl_capi_vcd.cc" \
    src/model.cc src/vpi_provider.cc tests/test_callbacks.cc \
    -o "$WORK/test_callbacks"

echo "== run =="
"$WORK/test_callbacks"
