add_subdirectory(TargetInfo)

add_llvm_library(
  ELDHexagonLDBackend
  HexagonAbsoluteStub.cpp
  HexagonAttributeFragment.cpp
  HexagonELFDynamic.cpp
  HexagonEmulation.cpp
  HexagonFindMask.cpp
  HexagonGOT.cpp
  HexagonLDBackend.cpp
  HexagonPLT.cpp
  HexagonRelocator.cpp
  HexagonTargetMachine.cpp
  HexagonTLSStub.cpp
  HexagonInfo.cpp)

if(HEXAGON_EXPERIMENTAL)
  add_definitions("-DHEXAGON_EXPERIMENTAL")
endif()

target_link_libraries(
  ELDHexagonLDBackend PRIVATE ELDHexagonTargetInfo ELDCore ELDFragment
                              ELDObject ELDTarget LLVMHexagonCodeGen)
