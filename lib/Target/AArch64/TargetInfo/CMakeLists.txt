llvm_add_library(ELDAArch64TargetInfo STATIC AArch64TargetInfo.cpp)
target_link_libraries(ELDAArch64TargetInfo LLVMAArch64AsmParser
                      LLVMAArch64CodeGen)
