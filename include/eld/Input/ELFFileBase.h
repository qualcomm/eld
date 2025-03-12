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
  ELFFileBase(Input *I, DiagnosticEngine *diagEngine, InputFile::Kind K)
      : ObjectFile(I, K, diagEngine) {}

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
  void setSymbolTable(ELFSection *SymTab) { m_SymbolTable = SymTab; }

  ELFSection *getSymbolTable() const { return m_SymbolTable; }

  /// ------------ String Table ---------------------------------------
  void setStringTable(ELFSection *SymTab) { m_StringTable = SymTab; }

  ELFSection *getStringTable() const { return m_StringTable; }

  /// ------------ Extended Symbol Table ---------------------------------------
  void setExtendedSymbolTable(ELFSection *SymTab) {
    m_ExtendedSymbolTable = SymTab;
  }

  ELFSection *getExtendedSymbolTable() const { return m_ExtendedSymbolTable; }

  /// ------------------- Dynamic Section --------------------------------
  void setDynamic(ELFSection *SymTab) { m_Dynamic = SymTab; }

  ELFSection *getDynamic() const { return m_Dynamic; }

  /// ------------------- ELFSection Helpers --------------------------------
  ELFSection *getELFSection(uint32_t Index);

  /// Append a section to the section table.
  void addSection(ELFSection *S);

  const std::vector<ELFSection *> &getRelocationSections() const {
    return m_RelocationSections;
  }

  uint32_t getCurrentSectionIndex() const { return m_SectionTable.size(); }

  virtual bool isELFNeeded() { return true; }

  virtual ~ELFFileBase() {}

protected:
  std::vector<ELFSection *> m_RelocationSections;
  ELFSection *m_SymbolTable = nullptr;
  ELFSection *m_StringTable = nullptr;
  ELFSection *m_ExtendedSymbolTable = nullptr;
  ELFSection *m_Dynamic = nullptr;
};

} // namespace eld

#endif // ELD_INPUT_ELFFILEBASE_H
