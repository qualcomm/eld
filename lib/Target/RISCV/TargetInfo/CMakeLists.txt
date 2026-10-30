llvm_add_library(ELDRISCVTargetInfo STATIC RISCVTargetInfo.cpp
                 LINK_COMPONENTS RISCVAsmParser RISCVCodeGen)
