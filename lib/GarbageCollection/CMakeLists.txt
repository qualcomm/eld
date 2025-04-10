llvm_add_library(ELDGC STATIC GarbageCollection.cpp)

target_link_libraries(ELDGC PRIVATE ELDConfig ELDFragment ELDSupport
                                    ELDDiagnostics)
