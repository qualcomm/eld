llvm_add_library(ELDx86_64TargetInfo STATIC x86_64TargetInfo.cpp
                 LINK_COMPONENTS X86AsmParser X86CodeGen)
