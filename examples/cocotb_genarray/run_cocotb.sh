#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Generate-scope array example: cocotb indexes dut.lane[i].r, where `lane` is a
# generate-for scope array. Exercises vpiGenScopeArray support.
#
#   PYTHON=/path/to/venv/bin/python bash run_cocotb.sh
set -euo pipefail
cd "$(dirname "$0")"
PYTHON="${PYTHON:-python3}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp genmod.v test_gen.py "$WORK"/
cd "$WORK"

"$PYTHON" - <<'PY'
import os, subprocess, sys
from cxxrtl_vpi.cocotb_build import build_cocotb_sim, cocotb_runtime_env

exe = build_cocotb_sim(["genmod.v"], "genmod", "./sim_gen")
env = dict(os.environ)
env.update(cocotb_runtime_env())
env.update({
    "COCOTB_TOPLEVEL": "genmod",
    "COCOTB_TEST_MODULES": "test_gen",
    "TOPLEVEL_LANG": "verilog",
    "COCOTB_RESULTS_FILE": "results.xml",
    "PYTHONPATH": os.getcwd(),
})
sys.exit(subprocess.run([exe], env=env).returncode)
PY
