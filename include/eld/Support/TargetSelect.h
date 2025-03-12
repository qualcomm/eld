//===- TargetSelect.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef ELD_SUPPORT_TARGETSELECT_H
#define ELD_SUPPORT_TARGETSELECT_H

extern "C" {
// Declare all of the target-initialization functions that are available.
#define ELD_TARGET(TargetName) void ELDInitialize##TargetName##LDTargetInfo();
#include "eld/Config/Targets.def"

// Declare all of the target-dependent functions that are available.
#define ELD_TARGET(TargetName) void ELDInitialize##TargetName##LDTarget();
#include "eld/Config/Targets.def"

// Declare all of the target-depedent linker information
#define ELD_LINKER(TargetName) void ELDInitialize##TargetName##LDInfo();
#include "eld/Config/Linkers.def"

// Declare all of the available emulators.
#define ELD_TARGET(TargetName) void ELDInitialize##TargetName##Emulation();
#include "eld/Config/Targets.def"

// Declare all of the available target-specific linker
#define ELD_LINKER(TargetName) void ELDInitialize##TargetName##LDBackend();
#include "eld/Config/Linkers.def"

} // extern "C"

namespace eld {
/// InitializeAllTargetInfos - The main program should call this function if
/// it wants access to all available targets that ELD is configured to
/// support, to make them available via the TargetRegistry.
///
/// It is legal for a client to make multiple calls to this function.
inline void InitializeAllTargetInfos() {
#define ELD_TARGET(TargetName) ELDInitialize##TargetName##LDTargetInfo();
#include "eld/Config/Targets.def"
}

/// InitializeAllTargets - The main program should call this function if it
/// wants access to all available target machines that ELD is configured to
/// support, to make them available via the TargetRegistry.
///
/// It is legal for a client to make multiple calls to this function.
inline void InitializeAllTargets() {
  eld::InitializeAllTargetInfos();

#define ELD_TARGET(TargetName) ELDInitialize##TargetName##LDBackend();
#include "eld/Config/Targets.def"
}

/// InitializeAllEmulations - The main program should call this function if
/// it wants all emulations to be configured to support. This function makes
/// all emulations available via the TargetRegistry.
inline void InitializeAllEmulations() {
#define ELD_TARGET(TargetName) ELDInitialize##TargetName##Emulation();
#include "eld/Config/Targets.def"
}

/// InitializeAllLinkers - The main program should call this function if it
/// wants all linkers that is configured to support, to make them
/// available via the TargetRegistry.
///
/// It is legal for a client to make multiple calls to this function.
inline void InitializeAllLinkers() {
#define ELD_TARGET(TargetName) ELDInitialize##TargetName##LDTarget();
#include "eld/Config/Targets.def"
}

} // namespace eld

#endif
