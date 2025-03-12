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
  ELFObjectFile(Input *I, DiagnosticEngine *diagEngine);

  /// Casting support.
  static bool classof(const InputFile *E) {
    return (E->getKind() == InputFile::ELFObjFileKind);
  }

  void setLTOObject() { m_isResultFromLTO = true; }

  bool isLTOObject() const override { return m_isResultFromLTO; }

  void setLLVMBCSection(ELFSection *S) { LLVMBCSection = S; }

  ELFSection *getLLVMBCSection() const { return LLVMBCSection; }

  TimingSection *getTimingSection() const { return TimingSection; }

  void setTimingSection(TimingSection *t) { TimingSection = t; }

  void setDynamicSections(ELFSection &GOT, ELFSection &GOTPLT, ELFSection &PLT,
                          ELFSection &RelDyn, ELFSection &RelPLT);

  ELFSection *getGOT() const { return m_GOT; }
  ELFSection *getGOTPLT() const { return m_GOTPLT; }
  ELFSection *getPLT() const { return m_PLT; }
  ELFSection *getRelaDyn() const { return m_RelaDyn; }
  ELFSection *getRelaPLT() const { return m_RelaPLT; }

  // Patching sections.
  void setPatchSections(ELFSection &GOTPatch, ELFSection &RelaPatch);

  ELFSection *getGOTPatch() const { return m_GOTPatch; }
  ELFSection *getRelaPatch() const { return m_RelaPatch; }

  ~ELFObjectFile() {}

  // --- DWARF Support

  void createDWARFContext(bool is32);

  llvm::DWARFContext *getDWARFContext() { return m_DWARFContext.get(); }

  void deleteDWARFContext() { m_DWARFContext.reset(); }

  bool hasDWARFContext() { return !!m_DWARFContext; }

  void populateDebugSections();

  // --- SectionGroup Support
  void addSectionGroup(ELFSection *S) { m_GroupSections.push_back(S); }

  const std::vector<ELFSection *> &getELFSectionGroupSections() const {
    return m_GroupSections;
  }

private:
  eld::ELFSection *LLVMBCSection = nullptr;
  eld::TimingSection *TimingSection = nullptr;
  bool m_isResultFromLTO = false;
  std::unique_ptr<llvm::DWARFContext> m_DWARFContext;
  std::vector<std::unique_ptr<llvm::MemoryBuffer>> m_DebugSections;
  std::vector<ELFSection *> m_GroupSections;
  ELFSection *m_GOT = nullptr;
  ELFSection *m_GOTPLT = nullptr;
  ELFSection *m_PLT = nullptr;
  ELFSection *m_RelaDyn = nullptr;
  ELFSection *m_RelaPLT = nullptr;
  ELFSection *m_GOTPatch = nullptr;
  ELFSection *m_RelaPatch = nullptr;
};

} // namespace eld

#endif // ELD_INPUT_ELFOBJECTFILE_H
