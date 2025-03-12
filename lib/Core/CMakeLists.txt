add_llvm_library(ELDCore Linker.cpp LinkerScript.cpp Module.cpp)

target_link_libraries(ELDCore PRIVATE ELDInput ELDPluginOp ELDObject
                                       ELDScript)