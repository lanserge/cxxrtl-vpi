#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Build the harness against cocotb's runtime and run an unmodified cocotb
# testbench on a CXXRTL model. This works end-to-end: cocotb embeds Python,
# drives the design through cxxrtl-vpi, and the test passes.
#
# Requirements: a Python with cocotb>=2.0, plus yosys and a C++ compiler on
# PATH. Point PYTHON at that interpreter:
#   PYTHON=/path/to/venv/bin/python bash run_cocotb.sh
#
# Trace the VPI scheduler with CXXRTL_VPI_DEBUG=1.
set -euo pipefail
cd "$(dirname "$0")"
REPO="$(cd ../.. && pwd)"

PYTHON="${PYTHON:-python3}"
CFG="$PYTHON -m cocotb_tools.config"
COCOTB_LIBDIR="$($CFG --lib-dir)"
LIBPYTHON="$($CFG --libpython)"
# Directory holding libpython*.{dylib,so}, and its link name (e.g. python3.13).
PYLIBDIR="$("$PYTHON" -c 'import sysconfig; print(sysconfig.get_config_var("LIBDIR"))')"
PYLIBNAME="python$("$PYTHON" -c 'import sysconfig; print(sysconfig.get_config_var("LDVERSION"))')"
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
    -L"$PYLIBDIR" "-l$PYLIBNAME" \
    -Wl,-rpath,"$COCOTB_LIBDIR" -Wl,-rpath,"$PYLIBDIR" \
    -o "sim_$TOP"

echo "== run cocotb =="
export COCOTB_TOPLEVEL=$TOP COCOTB_TEST_MODULES=test_counter TOPLEVEL_LANG=verilog
export COCOTB_RESULTS_FILE=results.xml LIBPYTHON_LOC="$LIBPYTHON"
export PYGPI_PYTHON_BIN="$($CFG --python-bin 2>/dev/null || echo "$PYTHON")"
export PYTHONPATH="$WORK"
export PATH="$COCOTB_LIBDIR:$PATH" DYLD_LIBRARY_PATH="$COCOTB_LIBDIR:$PYLIBDIR"
"./sim_$TOP"
echo "== results =="; cat results.xml
