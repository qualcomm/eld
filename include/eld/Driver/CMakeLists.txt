set(LLVM_TARGET_DEFINITIONS GnuLinkerOptions.td)
llvm_tablegen(GnuLinkerOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(GnuDriverOptions)

set(LLVM_TARGET_DEFINITIONS HexagonLinkerOptions.td)
llvm_tablegen(HexagonLinkerOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(HexagonDriverOptions)

set(LLVM_TARGET_DEFINITIONS ARMLinkerOptions.td)
llvm_tablegen(ARMLinkerOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(ARMDriverOptions)

set(LLVM_TARGET_DEFINITIONS RISCVLinkerOptions.td)
llvm_tablegen(RISCVLinkerOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(RISCVDriverOptions)

set(LLVM_TARGET_DEFINITIONS TemplateLinkerOptions.td)
llvm_tablegen(TemplateLinkerOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(TemplateDriverOptions)

set(LLVM_TARGET_DEFINITIONS x86_64LinkerOptions.td)
llvm_tablegen(x86_64LinkerOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(x86_64DriverOptions)

# Collect the driver TableGen targets in a global property so other
# directories (e.g., the top-level CMakeLists) can make use of them.
set(ELD_DRIVER_TABLEGEN_TARGETS
    GnuDriverOptions
    HexagonDriverOptions
    ARMDriverOptions
    RISCVDriverOptions
    TemplateDriverOptions
    x86_64DriverOptions)
set_property(GLOBAL PROPERTY ELD_DRIVER_TABLEGEN_TARGETS
             "${ELD_DRIVER_TABLEGEN_TARGETS}")
