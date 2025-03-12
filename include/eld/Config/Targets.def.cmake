//===- llvm/Config/Targets.def - ELD Target Architectures ------*- C++ -*-===*
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
// The set of targets supported by ELD is generated at configuration
// time, at which point this header is generated. Do not modify this
// header directly.
//
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET
#  error Please define the macro ELD_TARGET(TargetName)
#endif

${ELD_ENUM_TARGETS}

#undef ELD_TARGET
