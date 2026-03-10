option(
  ELD_CREATE_SYMLINKS
  "Enable this option to create <target>-link symlinks in the bin folder of both the build and install directories."
  On)

macro(add_eld_symlink name dest)
  llvm_add_tool_symlink(ELD ${name} ${dest} ALWAYS_GENERATE)
  llvm_install_symlink(ELD ${name} ${dest} ALWAYS_GENERATE)
endmacro()

set(LLVM_LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD} irreader Support)

add_llvm_executable(ld.eld eld.cpp)
add_dependencies(ld.eld update-eld-verinfo)

set(LINKER_WRAPPER_LIB "LW")
target_link_libraries(ld.eld PRIVATE ${LINKER_WRAPPER_LIB})

install(TARGETS ld.eld
        RUNTIME DESTINATION "${ELD_TOOLS_INSTALL_DIR}"
        COMPONENT ld.eld)
if (NOT LLVM_ENABLE_IDE)
  add_llvm_install_targets(install-ld.eld
                           DEPENDS ld.eld
                           COMPONENT ld.eld)
endif()

if(ELD_CREATE_SYMLINKS)
  foreach(target_name ${ELD_TARGETS_TO_BUILD})
    string(TOLOWER ${target_name} ld_eld_name)
    add_eld_symlink("${ld_eld_name}" ld.eld)
  endforeach()
endif()
if(NOT "${USE_LINKER_ALT_NAME}" STREQUAL "")
  add_eld_symlink("${USE_LINKER_ALT_NAME}" ld.eld)
endif()
