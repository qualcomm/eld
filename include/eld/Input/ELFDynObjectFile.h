//===- ELFDynObjectFile.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_INPUT_ELFDYNOBJECTFILE_H
#define ELD_INPUT_ELFDYNOBJECTFILE_H

#include "eld/Input/ELFFileBase.h"
#include "eld/Input/Input.h"

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

  // Cache parsed Verdef entries indexed by vd_ndx.
  void setVerDefs(std::vector<const void *> VDefs) { VerDefs = VDefs; }

  // Verdef table indexed by version index (vd_ndx).
  const std::vector<const void *> &getVerDefs() const { return VerDefs; }

  // Cache parsed Vernaux name offsets for needed versions indexed by vna_other.
  void setVerNeeds(std::vector<uint32_t> VNeeds) { VerNeeds = VNeeds; }

  // Verneed name offsets indexed by version index (vna_other & VERSYM_VERSION).
  const std::vector<uint32_t> &getVerNeeds() const { return VerNeeds; }

  // Cache parsed versym entries (vs_index) from the input DSO.
  void setVerSyms(std::vector<uint16_t> VSyms) { VerSyms = VSyms; }

  // Raw versym entries (vs_index) indexed by dynsym index.
  const std::vector<uint16_t> &getVerSyms() const { return VerSyms; }

  // Record the input DSO's .dynstr section used by verdef/verneed names.
  void setDynStrTabSection(ELFSection *S) { DynStrTabSection = S; }

  // Returns the contents of the input DSO's .dynstr (empty if missing).
  llvm::StringRef getDynStringTable() const;

  template <class ELFT>
  // Returns the version name associated with a dynsym entry.
  // For VER_NDX_LOCAL/VER_NDX_GLOBAL this returns an empty StringRef.
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

  // Returns the version index for a dynsym entry (clears VERSYM_HIDDEN).
  uint16_t getSymbolVersionID(uint32_t SymIdx) const {
    assert(SymIdx < VerSyms.size() && "Invalid SymIdx");
    return VerSyms[SymIdx] & llvm::ELF::VERSYM_VERSION;
  }

  // Returns true if the versym entry is not hidden (default version).
  bool isDefaultVersionedSymbol(uint32_t SymIdx) const {
    assert(SymIdx < VerSyms.size() && "Invalid SymIdx");
    return VerSyms[SymIdx] == getSymbolVersionID(SymIdx);
  }

  // Returns the output verneed index for an input version index.
  // The value of 0 means unassigned index.
  uint16_t getOutputVernAuxID(uint16_t InputVersionID) {
    if (InputVersionID >= OutputVernAuxIDMap.size())
      OutputVernAuxIDMap.resize(InputVersionID + 1, 0);
    return OutputVernAuxIDMap[InputVersionID];
  }

  // Assign an output verneed index for an input version index.
  void setOutputVernAuxID(uint16_t InputVersionID, uint16_t OutputVersionID) {
    if (InputVersionID >= OutputVernAuxIDMap.size())
      OutputVernAuxIDMap.resize(InputVersionID + 1, 0);
    OutputVernAuxIDMap[InputVersionID] = OutputVersionID;
  }

  // Returns the entire input->output version-id map for this DSO.
  const std::vector<uint16_t> &getOutputVernAuxIDMap() const {
    return OutputVernAuxIDMap;
  }

  // Returns a pointer to the parsed Verdef entry for an input version index.
  const void *getVerDef(uint16_t InputVersionID) const {
    assert(InputVersionID < VerDefs.size() && "Invalid InputVersionID");
    return VerDefs[InputVersionID];
  }

  bool hasSymbolVersioningInfo() const { return VerSymSection != nullptr; }

  // Record non-canonical symbols. For example, foo for `foo@@V1`.
  void addNonCanonicalSymbol(LDSymbol *Sym) {
    NonCanonicalSymbols.push_back(Sym);
  }

  // Returns the set of recorded non-canonical alias symbols
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
  /// It stores the output version ID for input version IDs.
  std::vector<uint16_t> OutputVernAuxIDMap;
  std::vector<LDSymbol *> NonCanonicalSymbols;
#endif
};

} // namespace eld

#endif // ELD_INPUT_ELFDYNOBJECTFILE_H
