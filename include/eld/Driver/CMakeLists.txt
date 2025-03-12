set(LLVM_TARGET_DEFINITIONS GnuLinkerOptions.td)
tablegen(LLVM GnuLinkerOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(GnuDriverOptions)

set(LLVM_TARGET_DEFINITIONS HexagonLinkerOptions.td)
tablegen(LLVM HexagonLinkerOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(HexagonDriverOptions)

set(LLVM_TARGET_DEFINITIONS ARMLinkerOptions.td)
tablegen(LLVM ARMLinkerOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(ARMDriverOptions)

set(LLVM_TARGET_DEFINITIONS RISCVLinkerOptions.td)
tablegen(LLVM RISCVLinkerOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(RISCVDriverOptions)

set(LLVM_TARGET_DEFINITIONS x86_64LinkerOptions.td)
tablegen(LLVM x86_64LinkerOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(x86_64DriverOptions)
