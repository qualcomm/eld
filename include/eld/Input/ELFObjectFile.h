//===- ELFObjectFile.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_INPUT_ELFOBJECTFILE_H
#define ELD_INPUT_ELFOBJECTFILE_H

#include "eld/Input/ELFFileBase.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"

namespace eld {

class ELFSection;
class TimingSection;
class RelocMap;

/** \class InputFile
 *  \brief InputFile represents a real object file, a linker script or anything
 *  that the rest of the linker can work with.
 */
class ELFObjectFile : public ELFFileBase {
public:
  ELFObjectFile(Input *I, DiagnosticEngine *DiagEngine);

  /// Casting support.
  static bool classof(const InputFile *E) {
    return (E->getKind() == InputFile::ELFObjFileKind);
  }

  void setLTOObject() { IsResultFromLTO = true; }

  bool isLTOObject() const override { return IsResultFromLTO; }

  void setLLVMBCSection(ELFSection *S) { LLVMBCSection = S; }

  ELFSection *getLLVMBCSection() const { return LLVMBCSection; }

  TimingSection *getTimingSection() const { return TimingSection; }

  void setTimingSection(TimingSection *T) { TimingSection = T; }

  void setDynamicSections(ELFSection &GOT, ELFSection &GOTPLT, ELFSection &PLT,
                          ELFSection &RelDyn, ELFSection &RelPLT);

  ELFSection *getGOT() const { return GOT; }
  ELFSection *getGOTPLT() const { return GOTPLT; }
  ELFSection *getPLT() const { return PLT; }
  ELFSection *getRelaDyn() const { return RelaDyn; }
  ELFSection *getRelaPLT() const { return RelaPLT; }

  // Patching sections.
  void setPatchSections(ELFSection &GOTPatch, ELFSection &RelaPatch);

  ELFSection *getGOTPatch() const { return GOTPatch; }
  ELFSection *getRelaPatch() const { return RelaPatch; }

  ~ELFObjectFile() {}

  // --- DWARF Support

  void createDWARFContext(bool Is32);

  llvm::DWARFContext *getDWARFContext() { return DWARFContext.get(); }

  void deleteDWARFContext() { DWARFContext.reset(); }

  bool hasDWARFContext() { return !!DWARFContext; }

  void populateDebugSections();

  // --- SectionGroup Support
  void addSectionGroup(ELFSection *S) { GroupSections.push_back(S); }

  const std::vector<ELFSection *> &getELFSectionGroupSections() const {
    return GroupSections;
  }

private:
  eld::ELFSection *LLVMBCSection = nullptr;
  eld::TimingSection *TimingSection = nullptr;
  bool IsResultFromLTO = false;
  std::unique_ptr<llvm::DWARFContext> DWARFContext;
  std::vector<std::unique_ptr<llvm::MemoryBuffer>> DebugSections;
  std::vector<ELFSection *> GroupSections;
  ELFSection *GOT = nullptr;
  ELFSection *GOTPLT = nullptr;
  ELFSection *PLT = nullptr;
  ELFSection *RelaDyn = nullptr;
  ELFSection *RelaPLT = nullptr;
  ELFSection *GOTPatch = nullptr;
  ELFSection *RelaPatch = nullptr;
};

} // namespace eld

#endif // ELD_INPUT_ELFOBJECTFILE_H
