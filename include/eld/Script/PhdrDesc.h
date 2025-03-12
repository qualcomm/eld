//===- PhdrDesc.h----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_PHDRDESC_H
#define ELD_SCRIPT_PHDRDESC_H

#include "eld/Script/Expression.h"
#include "eld/Script/ScriptCommand.h"
#include "eld/Script/StringList.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

/** \class PhdrSpec
 *  \brief This class defines the interface for Phdr specification.
 */

struct PhdrSpec {

  const std::string name() const { return m_Name->name(); }

  uint32_t type() const { return m_Type; }

  bool hasFileHdr() const { return m_HasFileHdr; }

  bool hasPhdr() const { return m_HasPhdr; }

  Expression *atAddress() const { return m_AtAddress; }

  bool lmaSet() const { return m_AtAddress != nullptr; }

  Expression *flags() const { return m_Flags; }

  void init() {
    m_Name = nullptr;
    m_Type = 0;
    m_HasFileHdr = false;
    m_HasPhdr = false;
    m_AtAddress = nullptr;
    m_Flags = nullptr;
  }

  const StrToken *m_Name;
  uint32_t m_Type;
  bool m_HasFileHdr;
  bool m_HasPhdr;
  Expression *m_AtAddress;
  Expression *m_Flags;
};

/** \class PhdrDesc
 *  \brief This class defines the interfaces to Phdr specification description.
 */

class PhdrDesc : public ScriptCommand {
public:
  PhdrDesc(const PhdrSpec &pSpec);

  ~PhdrDesc();

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::PHDR_DESC;
  }

  const PhdrSpec &spec() const { return m_Spec; }

  const PhdrSpec *getPhdrSpec() const { return &m_Spec; }

  void dump(llvm::raw_ostream &outs) const override;

  eld::Expected<void> activate(Module &pModule) override;

private:
  PhdrSpec m_Spec;
};

} // namespace eld

#endif
