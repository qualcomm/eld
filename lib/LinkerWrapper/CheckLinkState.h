//===- CheckLinkState.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef LINKER_WRAPPER_CHECK_LINK_STATE_H
#define LINKER_WRAPPER_CHECK_LINK_STATE_H

#include "LinkerWrapper.h"
#include "eld/Diagnostics/MsgHandler.h"
#include <initializer_list>
#include <string>

#define RETURN_INVALID_LINK_STATE_ERR(LW, validStates)                         \
  return std::make_unique<DiagnosticEntry>(                                    \
      Diag::error_invalid_link_state,                                          \
      std::vector<std::string>{std::string(LW.getCurrentLinkStateAsStr()),     \
                               std::string(LLVM_PRETTY_FUNCTION),              \
                               std::string(validStates)});

#define CHECK_LINK_STATE(LW, ...)                                              \
  if (!isValidLinkState(LW, {__VA_ARGS__}))                                    \
    RETURN_INVALID_LINK_STATE_ERR((LW), joinStrings({__VA_ARGS__}));

static inline bool
isValidLinkState(const eld::plugin::LinkerWrapper &LW,
                 std::initializer_list<std::string_view> ValidLinkStates) {
  for (const auto &S : ValidLinkStates) {
    bool b = S == "Initializing" || S == "ActBeforeRuleMatching" ||
             S == "BeforeLayout" || S == "ActBeforeSectionMerging" ||
             S == "CreatingSections" || S == "ActBeforePerformingLayout" ||
             S == "CreatingSegments" || S == "AfterLayout" ||
             S == "ActBeforeWritingOutput";
    ASSERT(b, "Invalid link state: " + std::string(S));
    if (S == "Initializing" && LW.isLinkStateInitializing())
      return true;
    if (S == "ActBeforeRuleMatching" && LW.isLinkStateActBeforeRuleMatching())
      return true;
    if (S == "BeforeLayout" && LW.isLinkStateBeforeLayout())
      return true;
    if (S == "ActBeforeSectionMerging" &&
        LW.isLinkStateActBeforeSectionMerging())
      return true;
    if (S == "CreatingSections" && LW.isLinkStateCreatingSections())
      return true;
    if (S == "PerformingLayout" && LW.isLinkStateBeforeLayout())
      return true;
    if (S == "CreatingSegments" && LW.isLinkStateCreatingSegments())
      return true;
    if (S == "ActBeforePerformingLayout" && LW.isLinkStateActBeforePerformingLayout())
      return true;
    if (S == "AfterLayout" && LW.isLinkStateAfterLayout())
      return true;
    if (S == "ActBeforeWritingOutput" && LW.isLinkStateActBeforeWritingOutput())
      return true;
  }
  return false;
}

static inline std::string
joinStrings(std::initializer_list<std::string_view> Names) {
  std::string S;
  for (auto it = Names.begin(), e = Names.end(); it != e; ++it) {
    S += *it;
    if (it != e - 1)
      S += ", ";
  }
  return S;
}
#endif