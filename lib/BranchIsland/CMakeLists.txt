llvm_add_library(ELDBranchIsland STATIC BranchIsland.cpp
                 BranchIslandFactory.cpp StubFactory.cpp)

target_link_libraries(ELDBranchIsland PRIVATE ELDFragment ELDDiagnostics)
