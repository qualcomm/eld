llvm_add_library(ELDARMTargetInfo STATIC ARMTargetInfo.cpp
                 LINK_COMPONENTS ARMAsmParser ARMCodeGen)
