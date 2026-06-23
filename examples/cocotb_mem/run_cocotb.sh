#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Memory example: cocotb indexes a Verilog memory (dut.store[i]) for read and
# direct write, exercising vpi_handle_by_index + memory element get/put.
#
#   PYTHON=/path/to/venv/bin/python bash run_cocotb.sh
set -euo pipefail
cd "$(dirname "$0")"
PYTHON="${PYTHON:-python3}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp mem.v test_mem.py "$WORK"/
cd "$WORK"

"$PYTHON" - <<'PY'
import os, subprocess, sys
from cxxrtl_vpi.cocotb_build import build_cocotb_sim, cocotb_runtime_env

exe = build_cocotb_sim(["mem.v"], "mem", "./sim_mem")
env = dict(os.environ)
env.update(cocotb_runtime_env())
env.update({
    "COCOTB_TOPLEVEL": "mem",
    "COCOTB_TEST_MODULES": "test_mem",
    "TOPLEVEL_LANG": "verilog",
    "COCOTB_RESULTS_FILE": "results.xml",
    "PYTHONPATH": os.getcwd(),
})
sys.exit(subprocess.run([exe], env=env).returncode)
PY
