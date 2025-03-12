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
  enum Type {
    Archive,       // This is used mainly by -Bstatic
    DynObj,        // This is used mainly by -Bdynamic
    Script,        // Linker script
    Namespec,      // Used for -l: and -l
    ArchiveMember, // If the input is an archive member
    Internal,      // Internal inputs.
    Default
  };

  explicit Input(std::string Name, DiagnosticEngine *diagEngine,
                 Type T = Input::Default);

  explicit Input(std::string Name, const Attribute &pAttr,
                 DiagnosticEngine *diagEngine, Type T = Input::Default);

  virtual ~Input();

  static bool classof(const Input *I) { return true; }

  /// Return a user-facing text representation of input type.
  static llvm::StringRef toString(Type);

  /// getFileName returns the FileName passed to the driver otherwise.
  const std::string getFileName() const { return m_FileName; }

  /// getName for the Input class returns the FileName
  const std::string getName() const { return m_Name; }

  const sys::fs::Path &getResolvedPath() const { return *m_ResolvedPath; }

  void setResolvedPath(std::string Path) { m_ResolvedPath = Path; }

  uint32_t getInputOrdinal() { return m_InputOrdinal; }

  Attribute &getAttribute() { return m_pAttr; }

  uint64_t getSize() const { return (m_pMemArea ? m_pMemArea->size() : 0); }

  // -----  memory area  ----- //
  void setMemArea(MemoryArea *pMemArea) { m_pMemArea = pMemArea; }

  MemoryArea *getMemArea() const { return m_pMemArea; }

  llvm::StringRef getFileContents() const;

  llvm::MemoryBufferRef getMemoryBufferRef() const {
    return m_pMemArea->getMemoryBufferRef();
  }

  virtual std::string decoratedPath(bool showAbsolute = false) const {
    // FIXME: We do not need to do llvm::Twine(...).str() because
    // getFullPath() and native() already returns std::string.
    if (showAbsolute)
      return llvm::Twine(getResolvedPath().getFullPath()).str();
    return llvm::Twine(getResolvedPath().native()).str();
  }

  virtual std::string
  getDecoratedRelativePath(const std::string &basepath) const;

  // Returns Object File Name.
  virtual std::pair<std::string, std::string>
  decoratedPathPair(bool showAbsolute = false) const {
    if (showAbsolute)
      return std::make_pair(getResolvedPath().getFullPath(), "");
    return std::make_pair(getResolvedPath().native(), "");
  }

  uint64_t getResolvedPathHash() const { return m_ResolvedPathHash; }

  uint64_t getArchiveMemberNameHash() const { return m_MemberNameHash; }

  // -----------------------Namespec support -------------------------------
  bool resolvePath(const LinkerConfig &pConfig);

  bool resolvePathMappingFile(const LinkerConfig &pConfig);

  /// -------------------------Input Type ---------------------------
  void setInputType(Input::Type T) { m_Type = T; }

  Type getInputType() const { return m_Type; }

  bool isInternal() const { return m_Type == Internal; }

  /// -------------------------InputFile support---------------------
  InputFile *getInputFile() const {
    if (!m_InputFile)
      return nullptr;
    return m_InputFile;
  }

  void setInputFile(InputFile *Inp);

  void overrideInputFile(InputFile *Inp);

  /// -------------------------Helper functions ---------------------
  bool isArchiveMember() const { return m_Type == ArchiveMember; }

  bool isNamespec() const { return m_Type == Namespec; }

  void setName(std::string Name) { m_Name = Name; }

  /// -----------------------Release Memory-------------------------
  void releaseMemory(bool isVerbose = false);

  bool isAlreadyReleased() const { return isReleased; }

  /// --------------------- WildcardPattern ------------------------
  void addFileMatchedPattern(const WildcardPattern *W, bool R) {
    FilePatternMap[W] = R;
  }

  bool findFileMatchedPattern(const WildcardPattern *W, bool &result) {
    auto F = FilePatternMap.find(W);
    if (F == FilePatternMap.end())
      return false;
    result = F->second;
    return true;
  }

  void addMemberMatchedPattern(const WildcardPattern *W, bool R);

  bool findMemberMatchedPattern(const WildcardPattern *W, bool &result);

  uint64_t getWildcardPatternSize();

  void resize(uint32_t N);

  void clear();

  bool isPatternMapInitialized() const { return PatternMapInitialized; }

  static llvm::hash_code computeFilePathHash(llvm::StringRef filePath);

  /// If MemoryArea is previously allocated for filepath, then return it;
  /// Otherwise returns nullptr.
  static MemoryArea *getMemoryAreaForPath(const std::string &filepath,
                                          DiagnosticEngine *diagEngine);

  static MemoryArea *createMemoryArea(const std::string &filepath,
                                      DiagnosticEngine *diagEngine);

private:
  // Check if a path is valid and emit any errors
  bool isPathValid(const std::string &Path) const;

protected:
  InputFile *m_InputFile = nullptr;
  MemoryArea *m_pMemArea = nullptr;
  std::string m_FileName;                      // Filename as what is passed to
                                               //   the linker
  std::string m_Name;                          // Resolved Name or
                                               // Member Name or the SONAME.
  std::optional<sys::fs::Path> m_ResolvedPath; // Resolved path.
  Attribute m_pAttr;                           // Attribute
  uint32_t m_InputOrdinal = 0;
  uint64_t m_ResolvedPathHash = 0;
  uint64_t m_MemberNameHash = 0;
  Type m_Type = Default; // The type of input file.
  bool isReleased = false;
  bool TraceMe = false;
  llvm::DenseMap<const WildcardPattern *, bool> FilePatternMap;
  llvm::DenseMap<const WildcardPattern *, bool> MemberPatternMap;
  bool PatternMapInitialized = false;
  DiagnosticEngine *m_DiagEngine = nullptr;

  /// Keeps track of already created MemoryAreas.
  ///
  /// It is used to reuse Inputs' MemoryArea when an input file is repeated in
  /// the link command-line.
  static std::unordered_map<std::string, MemoryArea *>
      m_ResolvedPathToMemoryAreaMap;
};

} // namespace eld

#endif
