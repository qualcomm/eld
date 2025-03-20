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
      WildcardFilePattern = nullptr;
      InputArchiveMember = nullptr;
      WildcardSectionPattern = nullptr;
      ExcludeFilesRule = nullptr;
      InputIsArchive = false;
    }

    bool hasFile() const { return WildcardFilePattern != nullptr; }

    bool isArchive() const { return InputIsArchive; }

    const WildcardPattern &file() const {
      assert(hasFile());
      return *WildcardFilePattern;
    }

    bool hasWildCard() const {
      if (!hasFile())
        return false;
      return ((*WildcardFilePattern).hasGlob());
    }

    bool hasArchiveMember() const { return InputArchiveMember != nullptr; }

    const WildcardPattern &archiveMember() const { return *InputArchiveMember; }

    const WildcardPattern *getFile() const { return WildcardFilePattern; }

    const WildcardPattern *getArchiveMember() const {
      return InputArchiveMember;
    }

    bool hasSections() const {
      return WildcardSectionPattern != nullptr &&
             !WildcardSectionPattern->empty();
    }

    const StringList &sections() const {
      assert(hasSections());
      return *WildcardSectionPattern;
    }

    bool operator==(const Spec &RHS) const {
      /* FIXME: currently I don't check the real content */
      if (this == &RHS)
        return true;
      if (WildcardFilePattern != RHS.WildcardFilePattern)
        return false;
      if (WildcardSectionPattern != RHS.WildcardSectionPattern)
        return false;
      if (ExcludeFilesRule != RHS.ExcludeFilesRule)
        return false;
      return true;
    }

    void initialize(const Spec &Spec) {
      this->WildcardFilePattern = Spec.WildcardFilePattern;
      this->InputArchiveMember = Spec.InputArchiveMember;
      this->WildcardSectionPattern = Spec.WildcardSectionPattern;
      this->InputIsArchive = Spec.InputIsArchive;
      this->ExcludeFilesRule = Spec.ExcludeFilesRule;
    }

    void setExcludeFiles(const ExcludeFiles *ExcludeFiles) {
      ExcludeFilesRule = ExcludeFiles;
    }

    const ExcludeFiles *getExcludeFiles() const { return ExcludeFilesRule; }

    bool hasExcludeFiles() const { return ExcludeFilesRule; }

    const WildcardPattern *WildcardFilePattern;
    const WildcardPattern *InputArchiveMember;
    const StringList *WildcardSectionPattern;
    // This stores the exclude files specified using the EXCLUDE_FILE directive
    // outside the section pattern. For example:
    // outSect : { EXCLUDE_FILES(...) *(*text*) }
    const ExcludeFiles *ExcludeFilesRule;
    bool InputIsArchive;
  };

  bool isSpecial() const {
    return ((InputSpecPolicy == InputSectDesc::SpecialKeep) ||
            (InputSpecPolicy == InputSectDesc::SpecialNoKeep));
  }

  bool isFixed() const {
    return ((InputSpecPolicy == InputSectDesc::Fixed) ||
            (InputSpecPolicy == InputSectDesc::KeepFixed));
  }

public:
  InputSectDesc(uint32_t ID, Policy PPolicy, const Spec &PSpec,
                OutputSectDesc &POutputDesc);

  InputSectDesc(ScriptCommand::Kind Kind, uint32_t ID, Policy Policy,
                const Spec &Spec, OutputSectDesc &OutputDesc);

  ~InputSectDesc();

  Policy policy() const { return InputSpecPolicy; }

  bool isEntry() const {
    return ((InputSpecPolicy == InputSectDesc::Keep) ||
            (InputSpecPolicy == InputSectDesc::SpecialKeep) ||
            (InputSpecPolicy == InputSectDesc::KeepFixed));
  }

  const Spec &spec() const { return InputSpec; }

  void dump(llvm::raw_ostream &Outs) const override;

  void dumpOnlyThis(llvm::raw_ostream &Outs) const override;

  void dumpSpec(llvm::raw_ostream &Outs) const;

  void dumpMap(llvm::raw_ostream &Outs = llvm::outs(), bool UseColor = false,
               bool UseNewLine = true, bool WithValues = false,
               bool AddIndent = true) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::INPUT_SECT_DESC ||
           LinkerScriptCommand->getKind() == ScriptCommand::OUTPUT_SECT_DATA;
  }

  eld::Expected<void> activate(Module &CurModule) override;

  /// Rule Container.
  RuleContainer *getRuleContainer() const { return ThisRuleContainer; }

  uint64_t getRuleHash() const { return Hash; }

  const OutputSectDesc &getOutputDesc() const {
    return OutputSectionDescription;
  }

  void setExcludeFiles(const ExcludeFiles *EF) {
    InputSpec.setExcludeFiles(EF);
  }

protected:
  RuleContainer *ThisRuleContainer;
  Policy InputSpecPolicy;
  Spec InputSpec;
  OutputSectDesc &OutputSectionDescription;
  uint32_t ID;
  uint64_t Hash = 0;
  std::string RuleText;
};

} // namespace eld

#endif
