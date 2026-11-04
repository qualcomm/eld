llvm_add_library(ELDX86TargetInfo STATIC X86TargetInfo.cpp
                 LINK_COMPONENTS X86AsmParser X86CodeGen)
