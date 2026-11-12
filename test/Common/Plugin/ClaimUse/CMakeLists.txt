set(SOURCES ClaimUse.cpp)
if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(bsl ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS ON)
  add_llvm_library(ClaimUse ${SOURCES} PARTIAL_SOURCES_INTENDED LINK_LIBS LW)
  add_llvm_library(ForeignUseWriter ForeignUseWriter.cpp
    PARTIAL_SOURCES_INTENDED LINK_LIBS LW)
  add_llvm_library(ClaimUseMerging ClaimUseMerging.cpp
    PARTIAL_SOURCES_INTENDED LINK_LIBS LW)
  add_llvm_library(EarlyClaimUse EarlyClaimUse.cpp
    PARTIAL_SOURCES_INTENDED LINK_LIBS LW)
  set(BUILD_SHARED_LIBS ${bsl})
endif()
add_plugin(ClaimUse)
add_plugin(ForeignUseWriter)
add_plugin(ClaimUseMerging)
add_plugin(EarlyClaimUse)
