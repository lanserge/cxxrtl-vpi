#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Full-SystemVerilog example: build a design with an *unpacked struct* (which
# Yosys's native frontend rejects) using the yosys-slang frontend, and run a
# cocotb test against it.
#
# Requires the yosys-slang plugin. Build it once:
#   git clone --recurse-submodules https://github.com/povik/yosys-slang
#   cd yosys-slang && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
# then point SLANG_PLUGIN at the built plugin and YOSYS at the matching binary
# (the one yosys-config targets, so the plugin ABI matches):
#   SLANG_PLUGIN=/path/to/yosys-slang/build/slang.so \
#   YOSYS=/usr/local/bin/yosys \
#   PYTHON=/path/to/venv/bin/python bash run_cocotb.sh
set -euo pipefail
cd "$(dirname "$0")"
PYTHON="${PYTHON:-python3}"
: "${SLANG_PLUGIN:?set SLANG_PLUGIN to the built yosys-slang plugin (slang.so)}"

# Use the yosys whose yosys-config built the plugin (ABI must match).
if [ -n "${YOSYS:-}" ]; then
    export PATH="$(dirname "$YOSYS"):$PATH"
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp us.sv test_us.py "$WORK"/
cd "$WORK"

SLANG_PLUGIN="$SLANG_PLUGIN" "$PYTHON" - <<'PY'
import os, subprocess, sys
from cxxrtl_vpi.cocotb_build import build_cocotb_sim, cocotb_runtime_env

exe = build_cocotb_sim(["us.sv"], "us", "./sim_us",
                       frontend="slang", slang_plugin=os.environ["SLANG_PLUGIN"])
env = dict(os.environ)
env.update(cocotb_runtime_env())
env.update({
    "COCOTB_TOPLEVEL": "us",
    "COCOTB_TEST_MODULES": "test_us",
    "TOPLEVEL_LANG": "verilog",
    "COCOTB_RESULTS_FILE": "results.xml",
    "PYTHONPATH": os.getcwd(),
})
sys.exit(subprocess.run([exe], env=env).returncode)
PY
