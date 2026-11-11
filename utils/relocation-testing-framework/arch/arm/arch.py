#"===------------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
#"===------------------------------------------------------------------------===

ARCH = {
  "mach": "EM_ARM",
  "endianness": "little",
  "bits": 32,
  "instr_bytes": 4,
  "config": {
    "scratch_reg": "r4",
    "compiler": ["clang", "-target", "armv7a-none-linux-gnueabi"],
    "linker": ["ld.eld", "-m", "armelf_linux_eabi"],
    "readelf": ["llvm-readelf"],
    "objdump": ["llvm-objdump"],
    "run_prefix": ["qemu-arm"],
    "run_timeout_seconds": 5,
  },
  "relocs": [
    {
      "name": "R_ARM_ABS32",
      "category": "address",
      "usage": "ldr {{reg}}, ={{var}}",
    },
  ],
}
