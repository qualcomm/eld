add_llvm_library(ELDGC GarbageCollection.cpp)

target_link_libraries(ELDGC PRIVATE ELDConfig ELDFragment ELDSupport
                                     ELDDiagnostics)
