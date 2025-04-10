include(AddNoVersionedSymbolExports)

set(LLVM_LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD} Option Support)

set(DRIVER_SOURCES GnuLdDriver.cpp)

if(ELD_ENABLE_TARGET_ARM OR ELD_ENABLE_TARGET_AARCH64)
  set(DRIVER_SOURCES ${DRIVER_SOURCES} ARMLinkDriver.cpp)
endif()

if(ELD_ENABLE_TARGET_HEXAGON)
  set(DRIVER_SOURCES ${DRIVER_SOURCES} HexagonLinkDriver.cpp)
endif()

if(ELD_ENABLE_TARGET_RISCV)
  set(DRIVER_SOURCES ${DRIVER_SOURCES} RISCVLinkDriver.cpp)
endif()

if(ELD_ENABLE_TARGET_X86_64)
  set(DRIVER_SOURCES ${DRIVER_SOURCES} x86_64LinkDriver.cpp)
endif()

set(SOURCES
    Diagnostics.cpp
    DiagnosticBuilder.cpp
    DiagnosticEntry.cpp
    Driver.cpp
    DWARF.cpp
    ${DRIVER_SOURCES}
    LayoutADT.cpp
    LayoutWrapper.cpp
    LinkerPluginConfig.cpp
    LinkerPlugin.cpp
    LinkerScript.cpp
    LinkerWrapper.cpp
    PluginADT.cpp
    SmallJSON.cpp
    TarWriter.cpp
    ThreadPool.cpp)

set(PLUGIN_ELD_DEPENDENCIES
    ARMDriverOptions
    GnuDriverOptions
    HexagonDriverOptions
    intrinsics_gen
    RISCVDriverOptions
    x86_64DriverOptions
    LLVMSupport
    ELDConfig
    ELDCore
    ELDDiagnostics
    ELDFragment
    ELDInput
    ELDObject
    ELDPluginOp
    ELDScript
    ELDSupport
    ELDTarget)

set(SHARED_LIB_SOURCES ${SOURCES})

set(bsl ${BUILD_SHARED_LIBS})

set(BUILD_SHARED_LIBS ON)

get_property(
  system_libs
  TARGET LLVMSupport
  PROPERTY LLVM_SYSTEM_LIBS)

add_llvm_library(
  LW
  ${SHARED_LIB_SOURCES}
  PARTIAL_SOURCES_INTENDED
  DEPENDS
  ${PLUGIN_ELD_DEPENDENCIES}
  LINK_LIBS
  ${LLVM_PTHREAD_LIB}
  ${system_libs}
  LINK_COMPONENTS
  BinaryFormat
  CodeGen
  Core
  Extensions
  Linker
  MC
  Option
  Object
  Passes
  Remarks
  Support
  TargetParser)

if(ELD_ON_UNIX)
  set_target_properties(
    LW PROPERTIES SOVERSION ${LINKER_PLUGIN_API_MAJOR_VERSION}
                  VERSION ${LINKER_PLUGIN_API_MAJOR_VERSION})
  add_no_version_symbol_exports(LW ${CMAKE_CURRENT_SOURCE_DIR}/LW.exports)
endif(ELD_ON_UNIX)

# Add each target as part of the link step.
foreach(t ${ELD_TARGETS_TO_BUILD})
  target_link_libraries(LW PRIVATE ELD${t}LDBackend)
  string(TOUPPER ${t} TARGET_UPPER)
  target_compile_definitions(LW PRIVATE ELD_ENABLE_TARGET_${TARGET_UPPER})
endforeach()

# Install LinkerWrapper shared library in the bin folder on windows, since the
# library is one of the dependent libraries of the linker.
if(ELD_ON_MSVC)
  install(TARGETS LW RUNTIME DESTINATION bin)
else()
  install(TARGETS LW LIBRARY DESTINATION lib)
endif()

set(BUILD_SHARED_LIBS ${bsl})
