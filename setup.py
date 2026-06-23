# SPDX-License-Identifier: ISC
#
# The package ships its C++ sources (the VPI provider + harness + headers) so a
# wheel install can build a cocotb sim. The canonical copies live at the repo
# root (used by CMake, tests, the standalone scripts); this hook copies them
# into cxxrtl_vpi/_cxx/ at build time so they end up inside the wheel.

import os
import shutil

from setuptools import setup
from setuptools.command.build_py import build_py

_CXX_DIRS = ["src", "include"]


class build_py_with_cxx(build_py):
    def run(self):
        super().run()
        dst_root = os.path.join(self.build_lib, "cxxrtl_vpi", "_cxx")
        for d in _CXX_DIRS:
            if not os.path.isdir(d):
                continue
            dst = os.path.join(dst_root, d)
            if os.path.exists(dst):
                shutil.rmtree(dst)
            shutil.copytree(d, dst)


setup(cmdclass={"build_py": build_py_with_cxx})
