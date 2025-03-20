//===- Input.h-------------------------------------------------------------===//
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

#ifndef ELD_INPUT_INPUT_H
#define ELD_INPUT_INPUT_H

#include "eld/Input/InputTree.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/Path.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include <unordered_map>
#include <vector>

namespace eld {

class DiagnosticEngine;
class InputFile;
class LinkerConfig;
class MemoryArea;
class SearchDirs;
class WildcardPattern;
class DiagnosticPrinter;

/** \class Input
 *  \brief Input provides the information of a input file.
 */
class Input {
public:
  enum InputType {
    Archive,       // This is used mainly by -Bstatic
    DynObj,        // This is used mainly by -Bdynamic
    Script,        // Linker script
    Namespec,      // Used for -l: and -l
    ArchiveMember, // If the input is an archive member
    Internal,      // Internal inputs.
    Default
  };

  explicit Input(std::string Name, DiagnosticEngine *DiagEngine,
                 InputType T = Input::Default);

  explicit Input(std::string Name, const Attribute &PAttr,
                 DiagnosticEngine *DiagEngine, InputType T = Input::Default);

  virtual ~Input();

  static bool classof(const Input *I) { return true; }

  /// Return a user-facing text representation of input type.
  static llvm::StringRef toString(InputType);

  /// getFileName returns the FileName passed to the driver otherwise.
  const std::string getFileName() const { return FileName; }

  /// getName for the Input class returns the FileName
  const std::string getName() const { return Name; }

  const sys::fs::Path &getResolvedPath() const { return *ResolvedPath; }

  void setResolvedPath(std::string Path) { ResolvedPath = Path; }

  uint32_t getInputOrdinal() { return InputOrdinal; }

  Attribute &getAttribute() { return Attr; }

  uint64_t getSize() const { return (MemArea ? MemArea->size() : 0); }

  // -----  memory area  ----- //
  void setMemArea(MemoryArea *PMemArea) { MemArea = PMemArea; }

  MemoryArea *getMemArea() const { return MemArea; }

  llvm::StringRef getFileContents() const;

  llvm::MemoryBufferRef getMemoryBufferRef() const {
    return MemArea->getMemoryBufferRef();
  }

  virtual std::string decoratedPath(bool ShowAbsolute = false) const {
    // FIXME: We do not need to do llvm::Twine(...).str() because
    // getFullPath() and native() already returns std::string.
    if (ShowAbsolute)
      return llvm::Twine(getResolvedPath().getFullPath()).str();
    return llvm::Twine(getResolvedPath().native()).str();
  }

  virtual std::string
  getDecoratedRelativePath(const std::string &Basepath) const;

  // Returns Object File Name.
  virtual std::pair<std::string, std::string>
  decoratedPathPair(bool ShowAbsolute = false) const {
    if (ShowAbsolute)
      return std::make_pair(getResolvedPath().getFullPath(), "");
    return std::make_pair(getResolvedPath().native(), "");
  }

  uint64_t getResolvedPathHash() const { return ResolvedPathHash; }

  uint64_t getArchiveMemberNameHash() const { return MemberNameHash; }

  // -----------------------Namespec support -------------------------------
  bool resolvePath(const LinkerConfig &PConfig);

  bool resolvePathMappingFile(const LinkerConfig &PConfig);

  /// -------------------------Input Type ---------------------------
  void setInputType(Input::InputType T) { Type = T; }

  InputType getInputType() const { return Type; }

  bool isInternal() const { return Type == Internal; }

  /// -------------------------InputFile support---------------------
  InputFile *getInputFile() const {
    if (!IF)
      return nullptr;
    return IF;
  }

  void setInputFile(InputFile *Inp);

  void overrideInputFile(InputFile *Inp);

  /// -------------------------Helper functions ---------------------
  bool isArchiveMember() const { return Type == ArchiveMember; }

  bool isNamespec() const { return Type == Namespec; }

  void setName(std::string N) { Name = N; }

  /// -----------------------Release Memory-------------------------
  void releaseMemory(bool IsVerbose = false);

  bool isAlreadyReleased() const { return IsReleased; }

  /// --------------------- WildcardPattern ------------------------
  void addFileMatchedPattern(const WildcardPattern *W, bool R) {
    FilePatternMap[W] = R;
  }

  bool findFileMatchedPattern(const WildcardPattern *W, bool &Result) {
    auto F = FilePatternMap.find(W);
    if (F == FilePatternMap.end())
      return false;
    Result = F->second;
    return true;
  }

  void addMemberMatchedPattern(const WildcardPattern *W, bool R);

  bool findMemberMatchedPattern(const WildcardPattern *W, bool &Result);

  uint64_t getWildcardPatternSize();

  void resize(uint32_t N);

  void clear();

  bool isPatternMapInitialized() const { return PatternMapInitialized; }

  static llvm::hash_code computeFilePathHash(llvm::StringRef FilePath);

  /// If MemoryArea is previously allocated for filepath, then return it;
  /// Otherwise returns nullptr.
  static MemoryArea *getMemoryAreaForPath(const std::string &Filepath,
                                          DiagnosticEngine *DiagEngine);

  static MemoryArea *createMemoryArea(const std::string &Filepath,
                                      DiagnosticEngine *DiagEngine);

private:
  // Check if a path is valid and emit any errors
  bool isPathValid(const std::string &Path) const;

protected:
  InputFile *IF = nullptr;
  MemoryArea *MemArea = nullptr;
  std::string FileName;                      // Filename as what is passed to
                                             //   the linker
  std::string Name;                          // Resolved Name or
                                             // Member Name or the SONAME.
  std::optional<sys::fs::Path> ResolvedPath; // Resolved path.
  Attribute Attr;                            // Attribute
  uint32_t InputOrdinal = 0;
  uint64_t ResolvedPathHash = 0;
  uint64_t MemberNameHash = 0;
  InputType Type = Default; // The type of input file.
  bool IsReleased = false;
  bool TraceMe = false;
  llvm::DenseMap<const WildcardPattern *, bool> FilePatternMap;
  llvm::DenseMap<const WildcardPattern *, bool> MemberPatternMap;
  bool PatternMapInitialized = false;
  DiagnosticEngine *DiagEngine = nullptr;

  /// Keeps track of already created MemoryAreas.
  ///
  /// It is used to reuse Inputs' MemoryArea when an input file is repeated in
  /// the link command-line.
  static std::unordered_map<std::string, MemoryArea *>
      ResolvedPathToMemoryAreaMap;
};

} // namespace eld

#endif
