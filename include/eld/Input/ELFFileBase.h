//===- ELFFileBase.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_INPUT_ELFFILEBASE_H
#define ELD_INPUT_ELFFILEBASE_H

#include "eld/Input/ObjectFile.h"
#include "llvm/Support/Casting.h"

namespace eld {

class ELFSection;
class DiagnosticPrinter;

/** \class InputFile
 *  \brief InputFile represents a real object file, a linker script or anything
 *  that the rest of the linker can work with.
 */
class ELFFileBase : public ObjectFile {
public:
  ELFFileBase(Input *I, DiagnosticEngine *DiagEngine,
              InputFile::InputFileKind K)
      : ObjectFile(I, K, DiagEngine) {}

  /// Casting support.
  static bool classof(const InputFile *E) {
    return ((E->getKind() == InputFile::ELFObjFileKind) ||
            (E->getKind() == InputFile::ELFDynObjFileKind) ||
            (E->getKind() == InputFile::ELFExecutableFileKind));
  }

  bool isRelocatable() const {
    return getKind() == eld::InputFile::ELFObjFileKind;
  }

  /// ------------ Symbol Table ---------------------------------------
  void setSymbolTable(ELFSection *SymTab) { SymbolTable = SymTab; }

  ELFSection *getSymbolTable() const { return SymbolTable; }

  /// ------------ String Table ---------------------------------------
  void setStringTable(ELFSection *SymTab) { StringTable = SymTab; }

  ELFSection *getStringTable() const { return StringTable; }

  llvm::StringRef getStringTableData() const;

  /// ------------ Extended Symbol Table ---------------------------------------
  void setExtendedSymbolTable(ELFSection *SymTab) {
    ExtendedSymbolTable = SymTab;
  }

  ELFSection *getExtendedSymbolTable() const { return ExtendedSymbolTable; }

  /// ------------------- Dynamic Section --------------------------------
  void setDynamic(ELFSection *SymTab) { Dynamic = SymTab; }

  ELFSection *getDynamic() const { return Dynamic; }

  /// ------------------- ELFSection Helpers --------------------------------
  ELFSection *getELFSection(uint32_t Index);

  /// Append a section to the section table.
  void addSection(ELFSection *S);

  const std::vector<ELFSection *> &getRelocationSections() const {
    return RelocationSections;
  }

  uint32_t getCurrentSectionIndex() const { return MSectionTable.size(); }

  virtual bool isELFNeeded() { return true; }

  virtual ~ELFFileBase() {}

protected:
  std::vector<ELFSection *> RelocationSections;
  ELFSection *SymbolTable = nullptr;
  ELFSection *StringTable = nullptr;
  ELFSection *ExtendedSymbolTable = nullptr;
  ELFSection *Dynamic = nullptr;
};

} // namespace eld

#endif // ELD_INPUT_ELFFILEBASE_H
