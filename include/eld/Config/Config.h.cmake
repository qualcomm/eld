//===- eld/Config/Config.h.cmake - ELD Target Config -------------*- C++ -*-===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_CONFIG_CONFIG_H
#define ELD_CONFIG_CONFIG_H

#cmakedefine ELD_DEFAULT_TARGET "${ELD_DEFAULT_TARGET}"

#cmakedefine ELD_TARGETS_TO_BUILD "${ELD_TARGETS_TO_BUILD}"

/* Target triple LLVM will generate code for by default */
#cmakedefine ELD_DEFAULT_TARGET_TRIPLE "${ELD_DEFAULT_TARGET_TRIPLE}"

#cmakedefine ELD_ON_UNIX "${ELD_ON_UNIX}"

#cmakedefine ELD_ON_WIN32 "${ELD_ON_WIN32}"

#cmakedefine ELD_ON_MSVC "${ELD_ON_MSVC}"

#define RPATH "${CMAKE_INSTALL_RPATH}"

#define LINKER_ALT_NAME "${USE_LINKER_ALT_NAME}"

@ELD_DEFINE_TARGETS@

#endif

