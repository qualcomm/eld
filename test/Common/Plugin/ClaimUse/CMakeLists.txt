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
  add_llvm_library(CreatedUse CreatedUse.cpp
    PARTIAL_SOURCES_INTENDED LINK_LIBS LW)
  add_llvm_library(LegacyClaimUse LegacyClaimUse.cpp
    PARTIAL_SOURCES_INTENDED LINK_LIBS LW)
  add_llvm_library(LegacySectionObserver LegacyClaimUse.cpp
    PARTIAL_SOURCES_INTENDED LINK_LIBS LW)
  target_compile_definitions(LegacySectionObserver PRIVATE SECTION_ITERATOR)
  add_llvm_library(HookStateObserver HookStateObserver.cpp
    PARTIAL_SOURCES_INTENDED LINK_LIBS LW)
  add_llvm_library(ClaimUseFileConsumer ClaimUseBlockConsumer.cpp
    PARTIAL_SOURCES_INTENDED LINK_LIBS LW)
  add_llvm_library(ClaimUseMemoryConsumer ClaimUseBlockConsumer.cpp
    PARTIAL_SOURCES_INTENDED LINK_LIBS LW)
  target_compile_definitions(ClaimUseMemoryConsumer PRIVATE MEMORY_SIZE)
  set(BUILD_SHARED_LIBS ${bsl})
endif()
add_plugin(ClaimUse)
add_plugin(ForeignUseWriter)
add_plugin(ClaimUseMerging)
add_plugin(EarlyClaimUse)
add_plugin(CreatedUse)
add_plugin(LegacyClaimUse)
add_plugin(LegacySectionObserver)
add_plugin(HookStateObserver)
add_plugin(ClaimUseFileConsumer)
add_plugin(ClaimUseMemoryConsumer)
