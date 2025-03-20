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
  enum InputFileKind {
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

  InputFile(Input *I, DiagnosticEngine *DiagEngine,
            InputFileKind K = UnknownKind)
      : I(I), Kind(K), DiagEngine(DiagEngine) {}

  /// Create an Input File.
  static InputFile *create(Input *I, DiagnosticEngine *DiagEngine);

  /// Create an Input File.
  static InputFile *createEmbedded(Input *I, llvm::StringRef S,
                                   DiagnosticEngine *DiagEngine);

  /// Create an Input File with a specific kind
  static InputFile *create(Input *I, InputFileKind K,
                           DiagnosticEngine *DiagEngine);

  /// Get the type of Input File.
  static InputFileKind getInputFileKind(llvm::StringRef S);

  /// Casting support.
  static bool classof(const InputFile *I) { return true; }

  /// InputFile kind.
  InputFileKind getKind() const { return Kind; }

  /// Get the associated Input.
  Input *getInput() const { return I; }

  /// Nice accessor functions.
  bool isDynamicLibrary() const { return Kind == ELFDynObjFileKind; }

  bool isBitcode() const { return Kind == BitcodeFileKind; }

  bool isObjectFile() const { return Kind == ELFObjFileKind; }

  bool isInternal() const;

  bool isLinkerScript() const { return Kind == GNULinkerScriptKind; }

  bool isArchive() const { return Kind == GNUArchiveFileKind; }

  bool isExecutableELFFile() const { return Kind == ELFExecutableFileKind; }

  virtual bool isLTOObject() const { return false; }

  void setNeeded() { Needed = true; }

  bool isNeeded() const { return Needed; }

  bool isUsed();

  void setUsed(bool Use);

  bool hasMappedPath() const { return MappedPath.size() > 0; }

  MappingFile::Kind getMappingFileKind() const { return MappingKind; }

  void setMappingFileKind(MappingFile::Kind K) { MappingKind = K; }

  std::string getMappedPath() const { return MappedPath; }

  void setMappedPath(std::string MP) { MappedPath = MP; }

  llvm::StringRef getContents() const { return Contents; }

  void setContents(llvm::StringRef S) { Contents = S; }

  llvm::StringRef getSlice(uint32_t S, uint32_t E) const {
    return Contents.slice(S, S + E);
  }

  const char *getCopyForWrite(uint32_t S, uint32_t E);

  size_t getSize() const { return Contents.size(); }

  void setToSkip() { Skip = true; }

  bool shouldSkipFile() const { return Skip; }

  virtual size_t getNumSections() const { return 0; }

  bool isBinaryFile() const { return Kind == InputFileKind::BinaryFileKind; }

  virtual bool isThinArchive() const { return false; }

protected:
  Input *I = nullptr;
  llvm::StringRef Contents;
  std::string MappedPath;
  InputFileKind Kind = UnknownKind;
  MappingFile::Kind MappingKind = MappingFile::Kind::Other;
  DiagnosticEngine *DiagEngine = nullptr;
  bool Needed = false;
  bool Used = false;
  bool Skip = false;
  std::mutex Mutex;
};

} // namespace eld

#endif // ELD_INPUT_INPUTFILE_H
