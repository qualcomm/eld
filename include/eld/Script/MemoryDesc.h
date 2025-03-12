//===- MemoryDesc.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_MEMORYDESC_H
#define ELD_SCRIPT_MEMORYDESC_H

#include "eld/Script/Expression.h"
#include "eld/Script/ScriptCommand.h"
#include "eld/Script/StringList.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

/** \class MemorySpec
 *  \brief This class defines the interface for Memory specification.
 */

struct MemorySpec {

  MemorySpec(const StrToken *Name, const StrToken *Attributes,
             Expression *Origin, Expression *Length)
      : m_Name(Name), m_Attributes(Attributes), m_Origin(Origin),
        m_Length(Length) {}

  const std::string getMemoryDescriptor() const { return m_Name->name(); }

  const std::string getMemoryAttributes() const {
    if (m_Attributes)
      return m_Attributes->name();
    return std::string();
  }

  const StrToken *getMemoryDescriptorToken() const { return m_Name; }

  Expression *getOrigin() const { return m_Origin; }

  Expression *getLength() const { return m_Length; }

private:
  const StrToken *m_Name = nullptr;
  const StrToken *m_Attributes = nullptr;
  Expression *m_Origin = nullptr;
  Expression *m_Length = nullptr;
};

/** \class MemoryDesc
 *  \brief This class defines the interfaces to Memory specification
 * description.
 */

class MemoryDesc : public ScriptCommand {
public:
  MemoryDesc(const MemorySpec &pSpec);

  ~MemoryDesc();

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->isMemoryDesc();
  }

  MemorySpec *getMemorySpec() { return &m_Spec; }

  const MemorySpec *getMemorySpec() const { return &m_Spec; }

  void dump(llvm::raw_ostream &outs) const override;

  eld::Expected<void> activate(Module &pModule) override;

private:
  MemorySpec m_Spec;
};

} // namespace eld

#endif
