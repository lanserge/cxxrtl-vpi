# SPDX-License-Identifier: ISC
"""
cxxrtl-vpi Python helpers.

Small conveniences for locating the CXXRTL runtime headers and generating a
model. The heavy lifting is the C++ VPI provider; this package is the thin glue
a build/runner uses (and the seam where a future cocotb Runner subclass lives).
"""

import os
import shutil
import subprocess

__version__ = "0.0.0"


def cxxrtl_runtime_include():
    """Return the CXXRTL runtime include dir (contains cxxrtl/capi/cxxrtl_capi.h).

    Resolution: $CXXRTL_INCLUDE_DIR, else `yosys-config --datdir`.
    Raises FileNotFoundError if the header cannot be located.
    """
    override = os.environ.get("CXXRTL_INCLUDE_DIR")
    candidates = []
    if override:
        candidates.append(override)

    yosys_config = shutil.which("yosys-config")
    if yosys_config:
        datdir = subprocess.run(
            [yosys_config, "--datdir"],
            stdout=subprocess.PIPE, text=True, check=True).stdout.strip()
        candidates.append(os.path.join(datdir, "include", "backends", "cxxrtl", "runtime"))

    for cand in candidates:
        if os.path.isfile(os.path.join(cand, "cxxrtl", "capi", "cxxrtl_capi.h")):
            return os.path.abspath(cand)

    raise FileNotFoundError(
        "CXXRTL runtime headers not found. Set CXXRTL_INCLUDE_DIR or put "
        "yosys-config on PATH.")


def write_cxxrtl(sources, top, output, yosys="yosys",
                 randomize_init=False, init_seed=1):
    """Generate a CXXRTL model from Verilog sources via Yosys.

    Args:
        sources: list of HDL source paths.
        top: top module name.
        output: path to write the generated .cc.
        randomize_init: if True, randomize uninitialized flip-flop state via
            ``setundef -init -random`` (the CXXRTL analogue of Verilator's
            --x-initial unique). CXXRTL otherwise inits 2-state engines to 0, so
            a design that secretly relies on uninitialized state passes silently;
            randomizing per-seed exposes it. Vary `init_seed` across runs.
        init_seed: integer seed for the randomization.
    Returns the output path.
    """
    if isinstance(sources, str):
        sources = [sources]
    cmds = [
        "read_verilog " + " ".join(sources),
        f"hierarchy -top {top}",
    ]
    if randomize_init:
        # proc lowers always-blocks to flops so setundef can seed their init.
        cmds += ["proc", f"setundef -init -random {int(init_seed)}"]
    cmds.append(f"write_cxxrtl {output}")
    subprocess.run([yosys, "-q", "-p", "; ".join(cmds)], check=True)
    return output
