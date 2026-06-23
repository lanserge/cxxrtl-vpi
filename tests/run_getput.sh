#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Functional MVP test: builds the VPI provider against a generated CXXRTL model
# and exercises the object-access surface (handle/get/put) on a counter.
set -euo pipefail
cd "$(dirname "$0")/.."

INC="$(yosys-config --datdir)/include/backends/cxxrtl/runtime"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "== conformance (compile-time header check) =="
c++ -std=c++14 -Iinclude tests/test_conformance.cc -o "$WORK/conf"
"$WORK/conf"
echo "  OK"

echo "== generate CXXRTL model =="
yosys -q -p "read_verilog examples/cocotb_counter/counter.v; \
             hierarchy -top counter; write_cxxrtl $WORK/counter_cxxrtl.cc"

echo "== build functional test =="
c++ -std=c++14 -O2 -Iinclude -I"$INC" \
    "$WORK/counter_cxxrtl.cc" \
    "$INC/cxxrtl/capi/cxxrtl_capi.cc" \
    src/model.cc src/vpi_provider.cc tests/test_getput.cc \
    -o "$WORK/test_getput"

echo "== run =="
"$WORK/test_getput"
