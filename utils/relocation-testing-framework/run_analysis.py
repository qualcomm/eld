#"===------------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
#"===------------------------------------------------------------------------===

"""Runtime (qemu-based) relocation testing.


"""

import sys

def run(arch_name, arch_info, link_cmd, output_dir):
    print(f"run-pass mode not yet implemented for arch '{arch_name}'", file=sys.stderr)
