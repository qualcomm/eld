add_llvm_library(ELDBranchIsland BranchIsland.cpp BranchIslandFactory.cpp
                 StubFactory.cpp)

target_link_libraries(ELDBranchIsland PRIVATE ELDFragment ELDDiagnostics)
