llvm_add_library(ELDHexagonTargetInfo STATIC HexagonTargetInfo.cpp)
target_link_libraries(ELDHexagonTargetInfo LLVMHexagonAsmParser
                      LLVMHexagonCodeGen)
