#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Smoke test for the scaffold: verifies the toolchain is present and that the
# provider library builds. Does NOT yet run a cocotb test (provider is a stub).
set -euo pipefail
cd "$(dirname "$0")/.."

echo "== checking toolchain =="
command -v yosys        >/dev/null || { echo "FAIL: yosys not found"; exit 1; }
command -v yosys-config >/dev/null || { echo "FAIL: yosys-config not found"; exit 1; }
command -v cmake        >/dev/null || { echo "FAIL: cmake not found"; exit 1; }

INC="$(yosys-config --datdir)/include/backends/cxxrtl/runtime"
test -f "$INC/cxxrtl/capi/cxxrtl_capi.h" || { echo "FAIL: cxxrtl_capi.h missing"; exit 1; }
echo "  cxxrtl runtime: $INC"

echo "== building provider library =="
cmake -S . -B build >/dev/null
cmake --build build >/dev/null
test -f build/libcxxrtl_vpi.a || { echo "FAIL: libcxxrtl_vpi.a not built"; exit 1; }

echo "== generating a model (write_cxxrtl smoke) =="
tmp="$(mktemp -d)"
printf 'module t(input a, output b); assign b=~a; endmodule\n' > "$tmp/t.v"
yosys -q -p "read_verilog $tmp/t.v; write_cxxrtl $tmp/t.cc"
grep -q "cxxrtl_design_create" "$tmp/t.cc" || { echo "FAIL: create() not emitted"; exit 1; }
rm -rf "$tmp"

echo "PASS (smoke). Object access + callback/time/control work"
echo "(see run_getput.sh, run_callbacks.sh). Real cocotb bring-up next —"
echo "see docs/vpi-coverage.md"
