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

__version__ = "0.0.1"


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
                 randomize_init=False, init_seed=1,
                 frontend="verilog", defines=(), slang_plugin=None):
    """Generate a CXXRTL model from (System)Verilog sources via Yosys.

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
        frontend: "verilog" (default) uses Yosys's native ``read_verilog -sv``,
            which covers synthesizable Verilog and a SystemVerilog subset.
            "slang" uses the yosys-slang plugin's ``read_slang`` for full
            SystemVerilog (structs, interfaces, packages, ...); requires the
            slang plugin to be installed (loaded via ``yosys -m slang``).
        defines: macro defines to pass to the frontend (e.g. ["FOO", "BAR=1"]).
    Returns the output path.
    """
    if isinstance(sources, str):
        sources = [sources]

    argv = [yosys, "-q"]
    cmds = []
    define_args = " ".join(f"-D{d}" for d in defines)

    if frontend == "slang":
        # Load the yosys-slang plugin: by explicit path if given, else by the
        # installed name "slang".
        argv += ["-m", slang_plugin or "slang"]
        cmds.append(f"read_slang --top {top} {define_args} " + " ".join(sources))
        # yosys-slang emits $buf cells; opt_clean lowers them for write_cxxrtl.
        cmds.append("opt_clean")
    elif frontend == "verilog":
        cmds.append(f"read_verilog -sv {define_args} " + " ".join(sources))
        cmds.append(f"hierarchy -top {top}")
    else:
        raise ValueError(f"unknown frontend {frontend!r} (verilog|slang)")

    if randomize_init:
        # proc lowers always-blocks to flops so setundef can seed their init.
        cmds += ["proc", f"setundef -init -random {int(init_seed)}"]
    cmds.append(f"write_cxxrtl {output}")
    argv += ["-p", "; ".join(cmds)]
    subprocess.run(argv, check=True)
    return output
