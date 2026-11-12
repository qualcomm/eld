llvm_add_library(ELDAArch64TargetInfo STATIC AArch64TargetInfo.cpp
                 LINK_COMPONENTS AArch64AsmParser AArch64CodeGen)
