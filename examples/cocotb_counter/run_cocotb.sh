#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# EXPERIMENTAL / WIP: build the harness against cocotb's real runtime and run an
# unmodified cocotb testbench on a CXXRTL model.
#
# Status: cocotb boots end-to-end against cxxrtl-vpi — it embeds Python,
# registers our VPI, discovers the toplevel, runs the test, and writes
# results.xml. ONE known issue remains: cocotb 2.0's queued writes
# (`handle.value = x`) are not reaching vpi_put_value, so the clock never
# toggles and the test stalls waiting on RisingEdge. See docs/vpi-coverage.md
# ("Known issue: cocotb write flush"). Run with CXXRTL_VPI_DEBUG=1 to trace the
# scheduler (register_cb / timed / put_value / value-change).
#
# Requirements: a Python with cocotb>=2.0 installed. Point PYTHON at it:
#   PYTHON=/path/to/venv/bin/python bash run_cocotb.sh
set -euo pipefail
cd "$(dirname "$0")"
REPO="$(cd ../.. && pwd)"

PYTHON="${PYTHON:-python3}"
CFG="$PYTHON -m cocotb_tools.config"
COCOTB_LIBDIR="$($CFG --lib-dir)"
LIBPYTHON="$($CFG --libpython)"
PYLIBDIR="$(dirname "$LIBPYTHON")"
INC="$(yosys-config --datdir)/include/backends/cxxrtl/runtime"

TOP=counter
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp counter.v test_counter.py "$WORK"/
cd "$WORK"

echo "== generate model =="
yosys -q -p "read_verilog $TOP.v; hierarchy -top $TOP; write_cxxrtl ${TOP}_cxxrtl.cc"

echo "== link harness against cocotb VPI =="
c++ -std=c++14 -O2 -DCXXRTL_VPI_COCOTB \
    -I "$REPO/include" -I"$INC" \
    "${TOP}_cxxrtl.cc" "$INC/cxxrtl/capi/cxxrtl_capi.cc" \
    "$REPO/src/model.cc" "$REPO/src/vpi_provider.cc" "$REPO/src/harness.cc" \
    -L"$COCOTB_LIBDIR" -lcocotbvpi_verilator -lgpi -lcocotb \
    -L"$PYLIBDIR" -lpython3.13 \
    -Wl,-rpath,"$COCOTB_LIBDIR" -Wl,-rpath,"$PYLIBDIR" \
    -o "sim_$TOP"

echo "== run cocotb =="
export COCOTB_TOPLEVEL=$TOP COCOTB_TEST_MODULES=test_counter TOPLEVEL_LANG=verilog
export COCOTB_RESULTS_FILE=results.xml LIBPYTHON_LOC="$LIBPYTHON"
export PYGPI_PYTHON_BIN="$($CFG --python-bin 2>/dev/null || echo "$PYTHON")"
export PYTHONPATH="$WORK"
export PATH="$COCOTB_LIBDIR:$PATH" DYLD_LIBRARY_PATH="$COCOTB_LIBDIR:$PYLIBDIR"
"./sim_$TOP"
echo "== results =="; cat results.xml 2>/dev/null || echo "(no results.xml)"
