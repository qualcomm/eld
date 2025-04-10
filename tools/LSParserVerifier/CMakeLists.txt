set(LLVM_LINK_COMPONENTS
    BinaryFormat
    CodeGen
    Core
    Extensions
    Linker
    MC
    Option
    Object
    Passes
    Remarks
    Support
    TargetParser)
get_property(
  system_libs
  TARGET LLVMSupport
  PROPERTY LLVM_SYSTEM_LIBS)

add_llvm_executable(LSParserVerifier LSParserVerifier.cpp)

target_link_libraries(LSParserVerifier PRIVATE ELDConfig ELDCore ELDDiagnostics
                                               ELDTarget LW ${system_libs})
