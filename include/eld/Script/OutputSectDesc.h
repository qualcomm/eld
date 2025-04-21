//===- OutputSectDesc.h----------------------------------------------------===//
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
#ifndef ELD_SCRIPT_OUTPUTSECTDESC_H
#define ELD_SCRIPT_OUTPUTSECTDESC_H

#include "eld/Script/Plugin.h"
#include "eld/Script/PluginCmd.h"
#include "eld/Script/StrToken.h"
#include "eld/Script/StringList.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include <string>
#include <vector>

namespace eld {

class Expression;
class ScriptMemoryRegion;

/** \class OutputSectDesc
 *  \brief This class defines the interfaces to output section description.
 */

class OutputSectDesc : public ScriptCommand {
public:
  enum Type {
    LOAD, // ALLOC
    NOLOAD,
    DSECT,
    COPY,
    INFO,
    OVERLAY,
    PROGBITS,
    UNINIT, /* Baremetal: Allow the section to be uninitialized */
    DEFAULT_TYPE = 0XFF
  };

  // Section permisions.
  enum Permissions {
    DEFAULT_PERMISSIONS = 0,
    R = llvm::ELF::SHF_ALLOC,
    RW = llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
    RX = llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
    RWX =
        llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE | llvm::ELF::SHF_EXECINSTR,
  };

  enum Constraint { NO_CONSTRAINT, ONLY_IF_RO, ONLY_IF_RW };

  struct Prolog {
    bool hasVMA() const { return OutputSectionVMA != nullptr; }
    const Expression &vma() const {
      assert(hasVMA());
      return *OutputSectionVMA;
    }
    Expression &vma() {
      assert(hasVMA());
      return *OutputSectionVMA;
    }

    void setType(Type AssignmentType) { ThisType = AssignmentType; }

    Type type() const { return ThisType; }

    bool hasFlag() const { return (SectionFlag != DEFAULT_PERMISSIONS); }

    void setFlag(uint32_t PPerm) { SectionFlag = PPerm; }

    uint32_t flag() const { return SectionFlag; }

    bool hasLMA() const { return OutputSectionLMA != nullptr; }
    const Expression &lma() const {
      assert(hasLMA());
      return *OutputSectionLMA;
    }
    Expression &lma() {
      assert(hasLMA());
      return *OutputSectionLMA;
    }
    void setLMA(Expression *PLma) { OutputSectionLMA = PLma; }

    bool hasAlign() const { return Alignment != nullptr; }
    const Expression &align() const {
      assert(hasAlign());
      return *Alignment;
    }
    Expression &align() {
      assert(hasAlign());
      return *Alignment;
    }

    bool hasSubAlign() const { return OutputSectionSubaAlign != nullptr; }
    const Expression &subAlign() const {
      assert(hasSubAlign());
      return *OutputSectionSubaAlign;
    }
    Expression &subAlign() {
      assert(hasSubAlign());
      return *OutputSectionSubaAlign;
    }

    Constraint constraint() const { return SectionConstraint; }

    bool hasPlugin() const { return PluginCmd != nullptr; }

    eld::Plugin *getPlugin() const {
      if (PluginCmd)
        return PluginCmd->getPlugin();
      return ThisPlugin;
    }

    void setPlugin(eld::PluginCmd *P) { PluginCmd = P; }

    void setPlugin(eld::Plugin *P) { ThisPlugin = P; }

    bool operator==(const Prolog &RHS) const {
      /* FIXME: currently I don't check the real content */
      if (this == &RHS)
        return true;
      if (OutputSectionVMA != RHS.OutputSectionVMA)
        return false;
      if (ThisType != RHS.ThisType)
        return false;
      if (OutputSectionLMA != RHS.OutputSectionLMA)
        return false;
      if (Alignment != RHS.Alignment)
        return false;
      if (OutputSectionSubaAlign != RHS.OutputSectionSubaAlign)
        return false;
      if (SectionConstraint != RHS.SectionConstraint)
        return false;
      if (PluginCmd != RHS.PluginCmd)
        return false;
      if (ThisPlugin != RHS.ThisPlugin)
        return false;
      return true;
    }

    void init() {
      OutputSectionVMA = nullptr;
      ThisType = OutputSectDesc::Type::DEFAULT_TYPE;
      SectionFlag = OutputSectDesc::Permissions::DEFAULT_PERMISSIONS;
      OutputSectionLMA = nullptr;
      Alignment = nullptr;
      OutputSectionSubaAlign = nullptr;
      SectionConstraint = OutputSectDesc::Constraint::NO_CONSTRAINT;
      PluginCmd = nullptr;
      ThisPlugin = nullptr;
      HasAlignWithInput = false;
    }

    void setAlignWithInput() { HasAlignWithInput = true; }

    bool hasAlignWithInput() const { return HasAlignWithInput; }

    Expression *OutputSectionVMA;
    Type ThisType;
    uint32_t SectionFlag;
    Expression *OutputSectionLMA;
    Expression *Alignment;
    Expression *OutputSectionSubaAlign;
    Constraint SectionConstraint;
    eld::PluginCmd *PluginCmd;
    eld::Plugin *ThisPlugin;
    bool HasAlignWithInput;
  };

  struct Epilog {
    bool hasRegion() const { return OutputSectionMemoryRegion != nullptr; }
    const eld::ScriptMemoryRegion &region() const {
      assert(hasRegion());
      return *ScriptVMAMemoryRegion;
    }

    bool hasLMARegion() const { return OutputSectionLMARegion != nullptr; }
    const eld::ScriptMemoryRegion &lmaRegion() const {
      assert(hasLMARegion());
      return *ScriptLMAMemoryRegion;
    }

    eld::ScriptMemoryRegion &region() {
      assert(hasRegion());
      return *ScriptVMAMemoryRegion;
    }

    eld::ScriptMemoryRegion &lmaRegion() {
      assert(hasLMARegion());
      return *ScriptLMAMemoryRegion;
    }

    llvm::StringRef getVMARegionName() const {
      return OutputSectionMemoryRegion->name();
    }

    llvm::StringRef getLMARegionName() const {
      return OutputSectionLMARegion->name();
    }

    bool hasPhdrs() const {
      return (ScriptPhdrs != nullptr && !(ScriptPhdrs->empty()));
    }
    StringList *phdrs() const {
      assert(hasPhdrs());
      return ScriptPhdrs;
    }

    bool hasFillExp() const { return FillExpression != nullptr; }

    Expression *fillExp() const { return FillExpression; }

    bool operator==(const Epilog &RHS) const {
      /* FIXME: currently I don't check the real content */
      if (this == &RHS)
        return true;
      if (OutputSectionMemoryRegion != RHS.OutputSectionMemoryRegion)
        return false;
      if (OutputSectionLMARegion != RHS.OutputSectionLMARegion)
        return false;
      if (ScriptPhdrs != RHS.ScriptPhdrs)
        return false;
      if (FillExpression != RHS.FillExpression)
        return false;
      return true;
    }

    void setRegion(ScriptMemoryRegion *S, const StrToken *R) {
      ScriptVMAMemoryRegion = S;
      OutputSectionMemoryRegion = R;
    }

    void setLMARegion(ScriptMemoryRegion *S, const StrToken *R) {
      ScriptLMAMemoryRegion = S;
      OutputSectionLMARegion = R;
    }

    void init() {
      OutputSectionMemoryRegion = nullptr;
      OutputSectionLMARegion = nullptr;
      ScriptVMAMemoryRegion = nullptr;
      ScriptLMAMemoryRegion = nullptr;
      ScriptPhdrs = nullptr;
      FillExpression = nullptr;
    }

    const StrToken *OutputSectionMemoryRegion;
    const StrToken *OutputSectionLMARegion;
    ScriptMemoryRegion *ScriptVMAMemoryRegion;
    ScriptMemoryRegion *ScriptLMAMemoryRegion;
    mutable StringList *ScriptPhdrs;
    Expression *FillExpression;
  };

  typedef std::vector<ScriptCommand *> OutputSectCmds;
  typedef OutputSectCmds::const_iterator const_iterator;
  typedef OutputSectCmds::iterator iterator;
  typedef OutputSectCmds::const_reference const_reference;
  typedef OutputSectCmds::reference reference;

public:
  OutputSectDesc(const std::string &PName);
  ~OutputSectDesc();

  const_iterator begin() const { return OutputSectionCommands.begin(); }
  iterator begin() { return OutputSectionCommands.begin(); }
  const_iterator end() const { return OutputSectionCommands.end(); }
  iterator end() { return OutputSectionCommands.end(); }

  const_reference front() const { return OutputSectionCommands.front(); }
  reference front() { return OutputSectionCommands.front(); }
  const_reference back() const { return OutputSectionCommands.back(); }
  reference back() { return OutputSectionCommands.back(); }

  std::string name() const { return Name; }

  size_t size() const { return OutputSectionCommands.size(); }

  bool empty() const { return OutputSectionCommands.empty(); }

  void dump(llvm::raw_ostream &Outs) const override;

  void dumpOnlyThis(llvm::raw_ostream &Outs) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::OUTPUT_SECT_DESC;
  }

  eld::Expected<void> activate(Module &CurModule) override;

  void pushBack(ScriptCommand *PCommand) override;

  void setProlog(const Prolog &PEpilog);

  eld::Expected<void> setEpilog(const Epilog &PEpilog);

  const Prolog &prolog() const { return OutpuSectDescProlog; }

  const Epilog &epilog() const { return OutpuSectDescEpilog; }

  Prolog &prolog() { return OutpuSectDescProlog; }

  Epilog &epilog() { return OutpuSectDescEpilog; }

  void initialize();

  OutputSectCmds &getOutputSectCommands() { return OutputSectionCommands; }

  void dumpEpilogue(llvm::raw_ostream &Outs) const;

private:
  OutputSectCmds OutputSectionCommands;
  std::string Name;
  Prolog OutpuSectDescProlog;
  Epilog OutpuSectDescEpilog;
};

} // namespace eld

#endif
