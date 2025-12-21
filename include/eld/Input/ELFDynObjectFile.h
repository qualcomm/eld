//===- ELFDynObjectFile.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_INPUT_ELFDYNOBJECTFILE_H
#define ELD_INPUT_ELFDYNOBJECTFILE_H

#include "eld/Input/ELFFileBase.h"
#include "eld/Input/Input.h"
#ifdef ELD_ENABLE_SYMBOL_VERSIONING
// FIXME: Are we using these headers?
#include "llvm/BinaryFormat/ELF.h"
#include <cassert>
#include <sys/types.h>
#endif

namespace eld {

/** \class ELFDynObjFile
 *  \brief InputFile represents a dynamic shared library.
 */
class ELFDynObjectFile : public ELFFileBase {
public:
  ELFDynObjectFile(Input *I, DiagnosticEngine *DiagEngine);

  /// Casting support.
  static bool classof(const InputFile *I) {
    return (I->getKind() == InputFile::ELFDynObjFileKind);
  }

  std::string getSOName() const { return getInput()->getName(); }

  void setSOName(std::string SOName) { getInput()->setName(SOName); }

  ELFSection *getDynSym() const { return SymbolTable; }

  bool isELFNeeded() override;

  virtual ~ELFDynObjectFile() {}

  std::string getFallbackSOName() const;

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  void setVerDefSection(ELFSection *S) { VerDefSection = S; }

  ELFSection *getVerDefSection() const { return VerDefSection; }

  void setVerNeedSection(ELFSection *S) { VerNeedSection = S; }

  ELFSection *getVerNeedSection() const { return VerNeedSection; }

  void setVerSymSection(ELFSection *S) { VerSymSection = S; }

  ELFSection *getVerSymSection() const { return VerSymSection; }

  void setVerDefs(std::vector<const void *> VDefs) { VerDefs = VDefs; }

  const std::vector<const void *> &getVerDefs() const { return VerDefs; }

  void setVerNeeds(std::vector<uint32_t> VNeeds) { VerNeeds = VNeeds; }

  const std::vector<uint32_t> &getVerNeeds() const { return VerNeeds; }

  void setVerSyms(std::vector<uint16_t> VSyms) { VerSyms = VSyms; }

  const std::vector<uint16_t> &getVerSyms() const { return VerSyms; }

  void setDynStrTabSection(ELFSection *S) { DynStrTabSection = S; }

  llvm::StringRef getDynStringTable() const;

  template <class ELFT>
  llvm::StringRef getSymbolVersionName(uint32_t SymIdx,
                                       ResolveInfo::Desc SymDesc) const {
    uint16_t SymVerID = getSymbolVersionID(SymIdx);
    llvm::StringRef VerName;
    if (SymVerID == llvm::ELF::VER_NDX_LOCAL ||
        SymVerID == llvm::ELF::VER_NDX_GLOBAL)
      return VerName;
    if (SymDesc == ResolveInfo::Desc::Undefined)
      VerName = getDynStringTable().data() + VerNeeds[SymVerID];
    else {
      auto VerNameOffset =
          reinterpret_cast<const typename ELFT::Verdef *>(VerDefs[SymVerID])
              ->getAux()
              ->vda_name;
      VerName = getDynStringTable().data() + VerNameOffset;
    }
    return VerName;
  }

  uint16_t getSymbolVersionID(uint32_t SymIdx) const {
    assert(SymIdx < VerSyms.size() && "Invalid SymIdx");
    return VerSyms[SymIdx] & ~llvm::ELF::VERSYM_HIDDEN;
  }

  bool isDefaultVersionedSymbol(uint32_t SymIdx) const {
    assert(SymIdx < VerSyms.size() && "Invalid SymIdx");
    return VerSyms[SymIdx] == getSymbolVersionID(SymIdx);
  }

  uint16_t getOutputVernAuxID(uint16_t InputVerID) {
    if (InputVerID >= OutputVernAuxIDMap.size())
      OutputVernAuxIDMap.resize(InputVerID + 1, 0);
    return OutputVernAuxIDMap[InputVerID];
  }

  void setOutputVernAuxID(uint16_t InputVerID, uint16_t OutputVerID) {
    if (InputVerID >= OutputVernAuxIDMap.size())
      OutputVernAuxIDMap.resize(InputVerID + 1, 0);
    OutputVernAuxIDMap[InputVerID] = OutputVerID;
  }

  const std::vector<uint16_t> &getOutputVernAuxIDMap() const {
    return OutputVernAuxIDMap;
  }

  const void *getVerDef(uint16_t InputVerID) {
    assert(InputVerID < VerDefs.size() && "Invalid InputVerID");
    return VerDefs[InputVerID];
  }

  bool hasSymbolVersioningInfo() const { return VerSymSection != nullptr; }

  void addNonCanonicalSymbol(LDSymbol *Sym) {
    NonCanonicalSymbols.push_back(Sym);
  }

  const std::vector<LDSymbol *> &getNonCanonicalSymbols() const {
    return NonCanonicalSymbols;
  }
#endif

private:
  std::vector<ELFSection *> Sections;
#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  ELFSection *VerDefSection = nullptr;
  ELFSection *VerNeedSection = nullptr;
  ELFSection *VerSymSection = nullptr;
  ELFSection *DynStrTabSection = nullptr;
  std::vector<const void *> VerDefs;
  std::vector<uint32_t> VerNeeds;
  std::vector<uint16_t> VerSyms;

  std::vector<uint16_t> OutputVernAuxIDMap;

  std::vector<LDSymbol *> NonCanonicalSymbols;
#endif
};

} // namespace eld

#endif // ELD_INPUT_ELFDYNOBJECTFILE_H
