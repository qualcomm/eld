#"===------------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
#"===------------------------------------------------------------------------===

ARCH = {
    "mach": "EM_X86_64",
    "endianness": "little",
    "config": {
        "scratch_reg": "%rax",
        "compiler": ["clang", "-target", "x86_64-unknown-linux-gnu"],
        "linker": ["ld.eld", "-m", "elf_x86_64"],
        "readelf": ["llvm-readelf"],
        "objdump": ["llvm-objdump"],
        "run_prefix": [],
        "run_timeout_seconds": 5,
    },
    "relocs": [
        {
            "name": "R_X86_64_64",
            "category": "address",
            "usage": "movabs ${{var}}, {{reg}}",
        },
        {
            "name": "R_X86_64_PLT32",
            "category": "branch",
            "usage": "call {{func}}",
        },
    ],
}
