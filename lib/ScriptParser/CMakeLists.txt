add_llvm_library(
  ELDScriptParser
  ScriptLexer.cpp
  ScriptParser.cpp)

target_link_libraries(ELDScriptParser PRIVATE ELDCore ELDSupport)
