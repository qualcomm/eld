llvm_add_library(ELDScriptParser STATIC ScriptLexer.cpp ScriptParser.cpp)

target_link_libraries(ELDScriptParser PRIVATE ELDCore ELDSupport)
