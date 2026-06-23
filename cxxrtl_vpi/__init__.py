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


def write_cxxrtl(sources, top, output, yosys="yosys"):
    """Generate a CXXRTL model from Verilog sources via Yosys.

    Args:
        sources: list of HDL source paths.
        top: top module name.
        output: path to write the generated .cc.
    Returns the output path.
    """
    if isinstance(sources, str):
        sources = [sources]
    script = "; ".join([
        "read_verilog " + " ".join(sources),
        f"hierarchy -top {top}",
        f"write_cxxrtl {output}",
    ])
    subprocess.run([yosys, "-q", "-p", script], check=True)
    return output
