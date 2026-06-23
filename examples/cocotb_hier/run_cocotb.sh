#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Wide + hierarchical cocotb example: a 40-bit bus through a submodule. Drives
# it via cxxrtl_vpi.cocotb_build and runs the cocotb test (it passes).
#
#   PYTHON=/path/to/venv/bin/python bash run_cocotb.sh
set -euo pipefail
cd "$(dirname "$0")"
PYTHON="${PYTHON:-python3}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp top.v test_top.py "$WORK"/
cd "$WORK"

"$PYTHON" - <<'PY'
import os, subprocess, sys
from cxxrtl_vpi.cocotb_build import build_cocotb_sim, cocotb_runtime_env

exe = build_cocotb_sim(["top.v"], "top", "./sim_top")
env = dict(os.environ)
env.update(cocotb_runtime_env())
env.update({
    "COCOTB_TOPLEVEL": "top",
    "COCOTB_TEST_MODULES": "test_top",
    "TOPLEVEL_LANG": "verilog",
    "COCOTB_RESULTS_FILE": "results.xml",
    "PYTHONPATH": os.getcwd(),
})
sys.exit(subprocess.run([exe], env=env).returncode)
PY
