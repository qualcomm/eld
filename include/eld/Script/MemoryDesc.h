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
      : Name(Name), MemoryAttributesString(Attributes),
        OriginExpression(Origin), LengthExpression(Length) {}

  const std::string getMemoryDescriptor() const { return Name->name(); }

  const std::string getMemoryAttributes() const {
    if (MemoryAttributesString)
      return MemoryAttributesString->name();
    return std::string();
  }

  const StrToken *getMemoryDescriptorToken() const { return Name; }

  Expression *getOrigin() const { return OriginExpression; }

  Expression *getLength() const { return LengthExpression; }

private:
  const StrToken *Name = nullptr;
  const StrToken *MemoryAttributesString = nullptr;
  Expression *OriginExpression = nullptr;
  Expression *LengthExpression = nullptr;
};

/** \class MemoryDesc
 *  \brief This class defines the interfaces to Memory specification
 * description.
 */

class MemoryDesc : public ScriptCommand {
public:
  MemoryDesc(const MemorySpec &PSpec);

  ~MemoryDesc();

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->isMemoryDesc();
  }

  MemorySpec *getMemorySpec() { return &InputSpec; }

  const MemorySpec *getMemorySpec() const { return &InputSpec; }

  void dump(llvm::raw_ostream &Outs) const override;

  eld::Expected<void> activate(Module &CurModule) override;

private:
  MemorySpec InputSpec;
};

} // namespace eld

#endif
