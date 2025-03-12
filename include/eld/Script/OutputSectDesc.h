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
    bool hasVMA() const { return m_pVMA != nullptr; }
    const Expression &vma() const {
      assert(hasVMA());
      return *m_pVMA;
    }
    Expression &vma() {
      assert(hasVMA());
      return *m_pVMA;
    }

    void setType(Type pType) { m_Type = pType; }

    Type type() const { return m_Type; }

    bool hasFlag() const { return (m_Flag != DEFAULT_PERMISSIONS); }

    void setFlag(uint32_t pPerm) { m_Flag = pPerm; }

    uint32_t flag() const { return m_Flag; }

    bool hasLMA() const { return m_pLMA != nullptr; }
    const Expression &lma() const {
      assert(hasLMA());
      return *m_pLMA;
    }
    Expression &lma() {
      assert(hasLMA());
      return *m_pLMA;
    }
    void setLMA(Expression *pLMA) { m_pLMA = pLMA; }

    bool hasAlign() const { return m_pAlign != nullptr; }
    const Expression &align() const {
      assert(hasAlign());
      return *m_pAlign;
    }
    Expression &align() {
      assert(hasAlign());
      return *m_pAlign;
    }

    bool hasSubAlign() const { return m_pSubAlign != nullptr; }
    const Expression &subAlign() const {
      assert(hasSubAlign());
      return *m_pSubAlign;
    }
    Expression &subAlign() {
      assert(hasSubAlign());
      return *m_pSubAlign;
    }

    Constraint constraint() const { return m_Constraint; }

    bool hasPlugin() const { return m_pPluginCmd != nullptr; }

    eld::Plugin *getPlugin() const {
      if (m_pPluginCmd)
        return m_pPluginCmd->getPlugin();
      return m_pPlugin;
    }

    void setPlugin(eld::PluginCmd *P) { m_pPluginCmd = P; }

    void setPlugin(eld::Plugin *P) { m_pPlugin = P; }

    bool operator==(const Prolog &pRHS) const {
      /* FIXME: currently I don't check the real content */
      if (this == &pRHS)
        return true;
      if (m_pVMA != pRHS.m_pVMA)
        return false;
      if (m_Type != pRHS.m_Type)
        return false;
      if (m_pLMA != pRHS.m_pLMA)
        return false;
      if (m_pAlign != pRHS.m_pAlign)
        return false;
      if (m_pSubAlign != pRHS.m_pSubAlign)
        return false;
      if (m_Constraint != pRHS.m_Constraint)
        return false;
      if (m_pPluginCmd != pRHS.m_pPluginCmd)
        return false;
      if (m_pPlugin != pRHS.m_pPlugin)
        return false;
      return true;
    }

    void init() {
      m_pVMA = nullptr;
      m_Type = OutputSectDesc::Type::DEFAULT_TYPE;
      m_Flag = OutputSectDesc::Permissions::DEFAULT_PERMISSIONS;
      m_pLMA = nullptr;
      m_pAlign = nullptr;
      m_pSubAlign = nullptr;
      m_Constraint = OutputSectDesc::Constraint::NO_CONSTRAINT;
      m_pPluginCmd = nullptr;
      m_pPlugin = nullptr;
    }

    Expression *m_pVMA;
    Type m_Type;
    uint32_t m_Flag;
    Expression *m_pLMA;
    Expression *m_pAlign;
    Expression *m_pSubAlign;
    Constraint m_Constraint;
    eld::PluginCmd *m_pPluginCmd;
    eld::Plugin *m_pPlugin;
  };

  struct Epilog {
    bool hasRegion() const { return m_pRegion != nullptr; }
    const eld::ScriptMemoryRegion &region() const {
      assert(hasRegion());
      return *m_pScriptMemoryRegion;
    }

    bool hasLMARegion() const { return m_pLMARegion != nullptr; }
    const eld::ScriptMemoryRegion &lmaRegion() const {
      assert(hasLMARegion());
      return *m_pScriptMemoryLMARegion;
    }

    eld::ScriptMemoryRegion &region() {
      assert(hasRegion());
      return *m_pScriptMemoryRegion;
    }

    eld::ScriptMemoryRegion &lmaRegion() {
      assert(hasLMARegion());
      return *m_pScriptMemoryLMARegion;
    }

    llvm::StringRef getVMARegionName() const { return m_pRegion->name(); }

    llvm::StringRef getLMARegionName() const { return m_pLMARegion->name(); }

    bool hasPhdrs() const {
      return (m_pPhdrs != nullptr && !(m_pPhdrs->empty()));
    }
    StringList *phdrs() const {
      assert(hasPhdrs());
      return m_pPhdrs;
    }

    bool hasFillExp() const { return m_pFillExp != nullptr; }

    Expression *fillExp() const { return m_pFillExp; }

    bool operator==(const Epilog &pRHS) const {
      /* FIXME: currently I don't check the real content */
      if (this == &pRHS)
        return true;
      if (m_pRegion != pRHS.m_pRegion)
        return false;
      if (m_pLMARegion != pRHS.m_pLMARegion)
        return false;
      if (m_pPhdrs != pRHS.m_pPhdrs)
        return false;
      if (m_pFillExp != pRHS.m_pFillExp)
        return false;
      return true;
    }

    void setRegion(ScriptMemoryRegion *S, const StrToken *R) {
      m_pScriptMemoryRegion = S;
      m_pRegion = R;
    }

    void setLMARegion(ScriptMemoryRegion *S, const StrToken *R) {
      m_pScriptMemoryLMARegion = S;
      m_pLMARegion = R;
    }

    void init() {
      m_pRegion = nullptr;
      m_pLMARegion = nullptr;
      m_pScriptMemoryRegion = nullptr;
      m_pScriptMemoryLMARegion = nullptr;
      m_pPhdrs = nullptr;
      m_pFillExp = nullptr;
    }

    const StrToken *m_pRegion;
    const StrToken *m_pLMARegion;
    ScriptMemoryRegion *m_pScriptMemoryRegion;
    ScriptMemoryRegion *m_pScriptMemoryLMARegion;
    mutable StringList *m_pPhdrs;
    Expression *m_pFillExp;
  };

  typedef std::vector<ScriptCommand *> OutputSectCmds;
  typedef OutputSectCmds::const_iterator const_iterator;
  typedef OutputSectCmds::iterator iterator;
  typedef OutputSectCmds::const_reference const_reference;
  typedef OutputSectCmds::reference reference;

public:
  OutputSectDesc(const std::string &pName);
  ~OutputSectDesc();

  const_iterator begin() const { return m_OutputSectCmds.begin(); }
  iterator begin() { return m_OutputSectCmds.begin(); }
  const_iterator end() const { return m_OutputSectCmds.end(); }
  iterator end() { return m_OutputSectCmds.end(); }

  const_reference front() const { return m_OutputSectCmds.front(); }
  reference front() { return m_OutputSectCmds.front(); }
  const_reference back() const { return m_OutputSectCmds.back(); }
  reference back() { return m_OutputSectCmds.back(); }

  std::string name() const { return m_Name; }

  size_t size() const { return m_OutputSectCmds.size(); }

  bool empty() const { return m_OutputSectCmds.empty(); }

  void dump(llvm::raw_ostream &outs) const override;

  void dumpOnlyThis(llvm::raw_ostream &outs) const override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::OUTPUT_SECT_DESC;
  }

  eld::Expected<void> activate(Module &pModule) override;

  void push_back(ScriptCommand *pCommand) override;

  void setProlog(const Prolog &pEpilog);

  eld::Expected<void> setEpilog(const Epilog &pEpilog);

  const Prolog &prolog() const { return m_Prolog; }

  const Epilog &epilog() const { return m_Epilog; }

  Prolog &prolog() { return m_Prolog; }

  Epilog &epilog() { return m_Epilog; }

  void initialize();

  OutputSectCmds &getOutputSectCommands() { return m_OutputSectCmds; }

  void dumpEpilogue(llvm::raw_ostream &outs) const;

private:
  OutputSectCmds m_OutputSectCmds;
  std::string m_Name;
  Prolog m_Prolog;
  Epilog m_Epilog;
};

} // namespace eld

#endif
