//===- InputFile.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_INPUT_INPUTFILE_H
#define ELD_INPUT_INPUTFILE_H

#include "eld/Support/MappingFile.h"
#include "llvm/ADT/StringRef.h"
#include <mutex>

namespace eld {

class DiagnosticEngine;
class ELFObjectFile;
class Input;

/** \class InputFile
 *  \brief InputFile represents a real object file, a linker script or anything
 *  that the rest of the linker can work with.
 */
class InputFile {
public:
  enum Kind {
    ELFObjFileKind,
    ELFDynObjFileKind,
    ELFExecutableFileKind,
    BitcodeFileKind,
    GNUArchiveFileKind,
    GNULinkerScriptKind,
    ELFSymDefFileKind,
    ELFProvideSymDefFileKind,
    InternalInputKind,
    BinaryFileKind,
    UnsupportedKind,
    UnknownKind,
  };

  virtual ~InputFile() {}

  InputFile(Input *I, DiagnosticEngine *diagEngine, Kind K = UnknownKind)
      : m_Input(I), m_Kind(K), m_DiagEngine(diagEngine) {}

  /// Create an Input File.
  static InputFile *Create(Input *I, DiagnosticEngine *diagEngine);

  /// Create an Input File.
  static InputFile *CreateEmbedded(Input *I, llvm::StringRef S,
                                   DiagnosticEngine *diagEngine);

  /// Create an Input File with a specific kind
  static InputFile *Create(Input *I, Kind K, DiagnosticEngine *diagEngine);

  /// Get the type of Input File.
  static Kind GetInputFileKind(llvm::StringRef S);

  /// Casting support.
  static bool classof(const InputFile *I) { return true; }

  /// InputFile kind.
  Kind getKind() const { return m_Kind; }

  /// Get the associated Input.
  Input *getInput() const { return m_Input; }

  /// Nice accessor functions.
  bool isDynamicLibrary() const { return m_Kind == ELFDynObjFileKind; }

  bool isBitcode() const { return m_Kind == BitcodeFileKind; }

  bool isObjectFile() const { return m_Kind == ELFObjFileKind; }

  bool isInternal() const;

  bool isLinkerScript() const { return m_Kind == GNULinkerScriptKind; }

  bool isArchive() const { return m_Kind == GNUArchiveFileKind; }

  bool isExecutableELFFile() const { return m_Kind == ELFExecutableFileKind; }

  virtual bool isLTOObject() const { return false; }

  void setNeeded() { m_Needed = true; }

  bool isNeeded() const { return m_Needed; }

  bool isUsed();

  void setUsed(bool Use);

  bool hasMappedPath() const { return MappedPath.size() > 0; }

  MappingFile::Kind getMappingFileKind() const { return m_MappingKind; }

  void setMappingFileKind(MappingFile::Kind K) { m_MappingKind = K; }

  std::string getMappedPath() const { return MappedPath; }

  void setMappedPath(std::string MP) { MappedPath = MP; }

  llvm::StringRef getContents() const { return Contents; }

  void setContents(llvm::StringRef S) { Contents = S; }

  llvm::StringRef getSlice(uint32_t S, uint32_t E) const {
    return Contents.slice(S, S + E);
  }

  const char *getCopyForWrite(uint32_t S, uint32_t E);

  size_t getSize() const { return Contents.size(); }

  void setToSkip() { m_Skip = true; }

  bool shouldSkipFile() const { return m_Skip; }

  virtual size_t getNumSections() const { return 0; }

  bool isBinaryFile() const { return m_Kind == Kind::BinaryFileKind; }

  virtual bool isThinArchive() const { return false; }

protected:
  Input *m_Input = nullptr;
  llvm::StringRef Contents;
  std::string MappedPath;
  Kind m_Kind = UnknownKind;
  MappingFile::Kind m_MappingKind = MappingFile::Kind::Other;
  DiagnosticEngine *m_DiagEngine = nullptr;
  bool m_Needed = false;
  bool m_Used = false;
  bool m_Skip = false;
  std::mutex Mutex;
};

} // namespace eld

#endif // ELD_INPUT_INPUTFILE_H
