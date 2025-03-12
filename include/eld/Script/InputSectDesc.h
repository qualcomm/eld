//===- InputSectDesc.h-----------------------------------------------------===//
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
#ifndef ELD_SCRIPT_INPUTSECTDESC_H
#define ELD_SCRIPT_INPUTSECTDESC_H

#include "eld/Script/ExcludeFiles.h"
#include "eld/Script/ScriptCommand.h"
#include "eld/Script/StringList.h"
#include "eld/Script/WildcardPattern.h"

namespace eld {

class InputAction;
class OutputSectDesc;
class RuleContainer;
class WildcardPattern;

/** \class InputSectDesc
 *  \brief This class defines the interfaces to input section description.
 */

class InputSectDesc : public ScriptCommand {
public:
  /* Special rule markers that dictate whether a list of input sections should
   * be kept that are handled by the rule */
  enum Policy { Keep, NoKeep, SpecialNoKeep, SpecialKeep, Fixed, KeepFixed };

  struct Spec {
    void initialize() {
      m_pWildcardFile = nullptr;
      m_pArchiveMember = nullptr;
      m_pWildcardSections = nullptr;
      m_ExcludeFiles = nullptr;
      m_pIsArchive = false;
    }

    bool hasFile() const { return m_pWildcardFile != nullptr; }

    bool isArchive() const { return m_pIsArchive; }

    const WildcardPattern &file() const {
      assert(hasFile());
      return *m_pWildcardFile;
    }

    bool hasWildCard() const {
      if (!hasFile())
        return false;
      return ((*m_pWildcardFile).hasGlob());
    }

    bool hasArchiveMember() const { return m_pArchiveMember != nullptr; }

    const WildcardPattern &archiveMember() const { return *m_pArchiveMember; }

    const WildcardPattern *getFile() const { return m_pWildcardFile; }

    const WildcardPattern *getArchiveMember() const { return m_pArchiveMember; }

    bool hasSections() const {
      return m_pWildcardSections != nullptr && !m_pWildcardSections->empty();
    }

    const StringList &sections() const {
      assert(hasSections());
      return *m_pWildcardSections;
    }

    bool operator==(const Spec &pRHS) const {
      /* FIXME: currently I don't check the real content */
      if (this == &pRHS)
        return true;
      if (m_pWildcardFile != pRHS.m_pWildcardFile)
        return false;
      if (m_pWildcardSections != pRHS.m_pWildcardSections)
        return false;
      if (m_ExcludeFiles != pRHS.m_ExcludeFiles)
        return false;
      return true;
    }

    void initialize(const Spec &spec) {
      this->m_pWildcardFile = spec.m_pWildcardFile;
      this->m_pArchiveMember = spec.m_pArchiveMember;
      this->m_pWildcardSections = spec.m_pWildcardSections;
      this->m_pIsArchive = spec.m_pIsArchive;
      this->m_ExcludeFiles = spec.m_ExcludeFiles;
    }

    void setExcludeFiles(const ExcludeFiles *excludeFiles) {
      m_ExcludeFiles = excludeFiles;
    }

    const ExcludeFiles *getExcludeFiles() const { return m_ExcludeFiles; }

    bool hasExcludeFiles() const { return m_ExcludeFiles; }

    const WildcardPattern *m_pWildcardFile;
    const WildcardPattern *m_pArchiveMember;
    const StringList *m_pWildcardSections;
    // This stores the exclude files specified using the EXCLUDE_FILE directive
    // outside the section pattern. For example:
    // outSect : { EXCLUDE_FILES(...) *(*text*) }
    const ExcludeFiles *m_ExcludeFiles;
    bool m_pIsArchive;
  };

  bool isSpecial() const {
    return ((m_Policy == InputSectDesc::SpecialKeep) ||
            (m_Policy == InputSectDesc::SpecialNoKeep));
  }

  bool isFixed() const {
    return ((m_Policy == InputSectDesc::Fixed) ||
            (m_Policy == InputSectDesc::KeepFixed));
  }

public:
  InputSectDesc(uint32_t ID, Policy pPolicy, const Spec &pSpec,
                OutputSectDesc &pOutputDesc);

  InputSectDesc(ScriptCommand::Kind kind, uint32_t ID, Policy policy,
                const Spec &spec, OutputSectDesc &outputDesc);

  ~InputSectDesc();

  Policy policy() const { return m_Policy; }

  bool isEntry() const {
    return ((m_Policy == InputSectDesc::Keep) ||
            (m_Policy == InputSectDesc::SpecialKeep) ||
            (m_Policy == InputSectDesc::KeepFixed));
  }

  const Spec &spec() const { return m_Spec; }

  void dump(llvm::raw_ostream &outs) const override;

  void dumpOnlyThis(llvm::raw_ostream &outs) const override;

  void dumpSpec(llvm::raw_ostream &outs) const;

  void dumpMap(llvm::raw_ostream &outs = llvm::outs(), bool useColor = false,
               bool useNewLine = true, bool withValues = false,
               bool addIndent = true) const override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::INPUT_SECT_DESC ||
           pCmd->getKind() == ScriptCommand::OUTPUT_SECT_DATA;
  }

  eld::Expected<void> activate(Module &pModule) override;

  /// Rule Container.
  RuleContainer *getRuleContainer() const { return m_RuleContainer; }

  uint64_t getRuleHash() const { return m_Hash; }

  const OutputSectDesc &getOutputDesc() const { return m_OutputSectDesc; }

  void setExcludeFiles(const ExcludeFiles *EF) { m_Spec.setExcludeFiles(EF); }

protected:
  RuleContainer *m_RuleContainer;
  Policy m_Policy;
  Spec m_Spec;
  OutputSectDesc &m_OutputSectDesc;
  uint32_t ID;
  uint64_t m_Hash = 0;
  std::string m_RuleText;
};

} // namespace eld

#endif
