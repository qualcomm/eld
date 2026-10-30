llvm_add_library(ELDHexagonTargetInfo STATIC HexagonTargetInfo.cpp
                 LINK_COMPONENTS HexagonAsmParser HexagonCodeGen)
