//===- Diagnostic.cpp------------------------------------------------------===//
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
#include "eld/Diagnostics/Diagnostic.h"
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <ctype.h>

using namespace eld;

//===----------------------------------------------------------------------===//
//  Diagnostic
Diagnostic::Diagnostic(DiagnosticEngine &PEngine) : DiagEngine(PEngine) {}

// FIXME: We should use trivial destructor whenever possible.
// Empty user-defined destructor is non-trivial.
Diagnostic::~Diagnostic() {}

// format - format this diagnostic into string, subsituting the formal
// arguments. The result is appended at on the pOutStr.
eld::Expected<void> Diagnostic::format(std::string &POutStr) const {
  // we've not implemented DWARF LOC messages yet. So, keep pIsLoC false
  eld::Expected<llvm::StringRef> ExpDesc =
      DiagEngine.infoMap().getDescription(getID(), false);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(ExpDesc);
  llvm::StringRef Desc = ExpDesc.value();
  eld::Expected<void> ExpFormat = format(Desc.begin(), Desc.end(), POutStr);
  return ExpFormat;
}

const char *Diagnostic::findMatch(char PVal, const char *PBegin,
                                  const char *PEnd) const {
  unsigned int Depth = 0;
  for (; PBegin != PEnd; ++PBegin) {
    if (0 == Depth && *PBegin == PVal)
      return PBegin;
    if (0 != Depth && *PBegin == '}')
      --Depth;

    if ('%' == *PBegin) {
      ++PBegin;
      if (PBegin == PEnd)
        break;

      if (!isdigit(*PBegin) && !ispunct(*PBegin)) {
        ++PBegin;
        while (PBegin != PEnd && !isdigit(*PBegin) && *PBegin != '{')
          ++PBegin;

        if (PBegin == PEnd)
          break;
        if ('{' == *PBegin)
          ++Depth;
      }
    }
  } // end of for
  return PEnd;
}

// format - format the given formal string, subsituting the formal
// arguments. The result is appended at on the pOutStr.
eld::Expected<void> Diagnostic::format(const char *PBegin, const char *PEnd,
                                       std::string &POutStr) const {
  const char *CurChar = PBegin;
  while (CurChar != PEnd) {
    if ('%' != *CurChar) {
      const char *NewEnd = std::find(CurChar, PEnd, '%');
      POutStr.append(CurChar, NewEnd);
      CurChar = NewEnd;
      continue;
    }
    if (ispunct(CurChar[1])) {
      POutStr.push_back(CurChar[1]); // %% -> %.
      CurChar += 2;
      continue;
    }

    // skip the %.
    ++CurChar;

    const char *Modifier = nullptr;
    size_t ModifierLen = 0;

    // we get a modifier
    if (!isdigit(*CurChar)) {
      Modifier = CurChar;
      while (*CurChar == '-' || (*CurChar >= 'a' && *CurChar <= 'z'))
        ++CurChar;
      ModifierLen = CurChar - Modifier;

      // we get an argument
      if ('{' == *CurChar) {
        ++CurChar; // skip '{'
        CurChar = findMatch('}', CurChar, PEnd);

        if (CurChar == PEnd) {
          // DIAG's format error
          llvm::report_fatal_error(
              llvm::Twine("Mismatched {} in the diagnostic: ") +
              llvm::Twine(getID()));
        }

        ++CurChar; // skip '}'
      }
    }
    if (!isdigit(*CurChar)) {
      // FIXME: This should be returned using eld::Expected as well!
      llvm::report_fatal_error(llvm::Twine("In diagnostic: ") +
                               llvm::Twine(getID()) + llvm::Twine(": ") +
                               llvm::Twine(PBegin) +
                               llvm::Twine("\nNo given arugment number:\n"));
    }

    unsigned int ArgNo = *CurChar - '0';
    if (ArgNo >= getNumArgs()) {
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          Diag::fatal_missing_diag_args,
          {std::to_string(ArgNo), std::string(PBegin, PEnd)}));
    }
    ++CurChar; // skip argument number

    DiagnosticEngine::ArgumentKind Kind = getArgKind(ArgNo);
    switch (Kind) {
    case DiagnosticEngine::ak_std_string: {
      if (0 != ModifierLen) {
        llvm::report_fatal_error(
            llvm::Twine("In diagnostic: ") + llvm::Twine(getID()) +
            llvm::Twine(": ") + llvm::Twine(PBegin) +
            llvm::Twine("\nNo modifiers for strings yet\n"));
      }
      const std::string &Str = getArgStdStr(ArgNo);
      POutStr.append(Str.begin(), Str.end());
      break;
    }
    case DiagnosticEngine::ak_c_string: {
      if (0 != ModifierLen) {
        llvm::report_fatal_error(
            llvm::Twine("In diagnostic: ") + llvm::Twine(getID()) +
            llvm::Twine(": ") + llvm::Twine(PBegin) +
            llvm::Twine("\nNo modifiers for strings yet\n"));
      }
      const char *Str = getArgCStr(ArgNo);
      if (nullptr == Str)
        Str = "(null)";
      POutStr.append(Str);
      break;
    }
    case DiagnosticEngine::ak_sint: {
      int Val = getArgSInt(ArgNo);
      llvm::raw_string_ostream(POutStr) << Val;
      break;
    }
    case DiagnosticEngine::ak_uint: {
      unsigned int Val = getArgUInt(ArgNo);
      llvm::raw_string_ostream(POutStr) << Val;
      break;
    }
    case DiagnosticEngine::ak_ulonglong: {
      unsigned long long Val = getArgULongLong(ArgNo);
      llvm::raw_string_ostream(POutStr) << Val;
      break;
    }
    case DiagnosticEngine::ak_bool: {
      bool Val = getArgBool(ArgNo);
      if (Val)
        POutStr.append("true");
      else
        POutStr.append("false");
      break;
    }
    } // end of switch
  } // end of while
  return {};
}
