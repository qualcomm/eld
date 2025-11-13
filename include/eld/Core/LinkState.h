//===- LinkState.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_CORE_LINKSTATE_H
#define ELD_CORE_LINKSTATE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"
namespace eld {
/// Stages of the link process.
/// What actions a plugin can perform depends on the
/// link state. For example:
/// - Plugins can change the output section of an input
///   section only in BeforeLayout link state.
/// - Plugins can move chunks from one output section to
///   another only in CreatingSections link state.
/// - Plugins can only compute the output image layout checkum using
///   `LinkerWrapper::getImageLayoutChecksum` only in AfterLayout linker
///   state.
/// - Plugins can reassign virtual addresses using
///   `LinkerWrapper::reassignVirtualAddresses()` only in CreatingSegments
///   link state.
///
/// Plugin authors should ensure that the action being performed by the
/// plugin is meaningful in the link state in which it is executed.
/// Executing invalid actions for a link state can result in undefined
/// behavior.
enum LinkState : uint8_t {
  Unknown,
  Initializing,
  BeforeLayout,
  CreatingSections,
  CreatingSegments,
  AfterLayout,
};

static inline llvm::StringRef getLinkStateStrRef(LinkState State) {
#define ADD_CASE(S)                                                            \
  case LinkState::S:                                                           \
    return #S;
  switch (State) {
    ADD_CASE(Unknown)
    ADD_CASE(Initializing)
    ADD_CASE(BeforeLayout)
    ADD_CASE(CreatingSections)
    ADD_CASE(CreatingSegments)
    ADD_CASE(AfterLayout)
  }
#undef ADD_CASE
  llvm_unreachable("Invalid LinkState");
}
} // namespace eld
#endif