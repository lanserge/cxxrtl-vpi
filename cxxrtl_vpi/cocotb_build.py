# SPDX-License-Identifier: ISC
"""
Build a cocotb simulation executable from RTL + the cxxrtl-vpi provider.

This is the reusable build logic behind examples/cocotb_counter/run_cocotb.sh:
generate the CXXRTL model with Yosys, then compile it together with the
cxxrtl-vpi provider/harness and link against cocotb's VPI runtime and libpython.
The resulting executable IS a VPI-speaking simulator that cocotb drives.

The engine adapter owns this so any consumer (e.g. an SC tool driver) can build a
cocotb sim without re-deriving the link recipe.
"""

import os
import subprocess
import sys

from cxxrtl_vpi import cxxrtl_runtime_include, write_cxxrtl


def _repo_cxx_dir():
    """Directory containing the provider C++ sources (with src/ and include/).

    For a wheel install this is cxxrtl_vpi/_cxx/ (bundled by setup.py's build
    hook); for an editable/source install it is the repo root.
    """
    pkg = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(pkg, "_cxx"),       # wheel install
        os.path.dirname(pkg),            # editable / source checkout (repo root)
    ]
    for root in candidates:
        if os.path.isfile(os.path.join(root, "src", "vpi_provider.cc")):
            return root
    raise FileNotFoundError(
        "cxxrtl-vpi C++ sources not found (looked in %s)." % ", ".join(candidates))


def cocotb_config(python=None):
    """Return (lib_dir, libpython, py_lib_dir, py_lib_name) for linking cocotb."""
    python = python or sys.executable

    def cfg(*args):
        return subprocess.run([python, "-m", "cocotb_tools.config", *args],
                              stdout=subprocess.PIPE, text=True,
                              check=True).stdout.strip()
    lib_dir = cfg("--lib-dir")
    libpython = cfg("--libpython")
    py_lib_dir = subprocess.run(
        [python, "-c", "import sysconfig; print(sysconfig.get_config_var('LIBDIR'))"],
        stdout=subprocess.PIPE, text=True, check=True).stdout.strip()
    ldversion = subprocess.run(
        [python, "-c", "import sysconfig; print(sysconfig.get_config_var('LDVERSION'))"],
        stdout=subprocess.PIPE, text=True, check=True).stdout.strip()
    return lib_dir, libpython, py_lib_dir, f"python{ldversion}"


def build_cocotb_sim(rtl_sources, top, output, *,
                     vpi_sim="verilator", cxx=None, cxxstd="c++14",
                     opt="-O2", extra_cflags=(), python=None):
    """Build a cocotb-driven CXXRTL simulation executable.

    Args:
        rtl_sources: list of Verilog/SystemVerilog source paths.
        top: top module name.
        output: path of the executable to produce.
        vpi_sim: which cocotb VPI consumer lib to link (any works for a generic
                 VPI host; default "verilator").
        cxx: C++ compiler (default $CXX or c++).
        python: Python interpreter whose cocotb to link (default sys.executable).

    Returns the output path.
    """
    cxx = cxx or os.environ.get("CXX", "c++")
    repo = _repo_cxx_dir()
    inc = cxxrtl_runtime_include()
    lib_dir, _libpython, py_lib_dir, py_lib_name = cocotb_config(python)

    outdir = os.path.dirname(os.path.abspath(output)) or "."
    os.makedirs(outdir, exist_ok=True)
    # Generated model is an intermediate: keep it in the cwd, not in the output
    # directory (SC treats unexpected files under outputs/ as an error).
    model = f"{top}_cxxrtl.cc"
    write_cxxrtl(rtl_sources, top, model)

    cmd = [
        cxx, f"-std={cxxstd}", opt, "-DCXXRTL_VPI_COCOTB",
        f"-I{os.path.join(repo, 'include')}", f"-I{inc}",
        model,
        os.path.join(inc, "cxxrtl", "capi", "cxxrtl_capi.cc"),
        os.path.join(repo, "src", "model.cc"),
        os.path.join(repo, "src", "vpi_provider.cc"),
        os.path.join(repo, "src", "harness.cc"),
        *extra_cflags,
        f"-L{lib_dir}", f"-lcocotbvpi_{vpi_sim}", "-lgpi", "-lcocotb",
        f"-L{py_lib_dir}", f"-l{py_lib_name}",
        f"-Wl,-rpath,{lib_dir}", f"-Wl,-rpath,{py_lib_dir}",
        "-o", output,
    ]
    subprocess.run(cmd, check=True)
    return output


def cocotb_runtime_env(python=None):
    """Environment additions needed to *run* a built cocotb sim (PATH/lib path)."""
    lib_dir, libpython, py_lib_dir, _ = cocotb_config(python)
    sep = os.pathsep
    return {
        "LIBPYTHON_LOC": libpython,
        "PATH": lib_dir + sep + os.environ.get("PATH", ""),
        "DYLD_LIBRARY_PATH": lib_dir + sep + py_lib_dir,
        "LD_LIBRARY_PATH": lib_dir + sep + py_lib_dir,
        "PYGPI_PYTHON_BIN": python or sys.executable,
    }
