llvm_add_library(ELDCore STATIC Linker.cpp LinkerScript.cpp Module.cpp)

target_link_libraries(ELDCore PRIVATE ELDInput ELDPluginOp ELDObject ELDScript)
