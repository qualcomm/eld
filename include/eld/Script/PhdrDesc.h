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

  const std::string name() const { return Name->name(); }

  uint32_t type() const { return ThisType; }

  bool hasFileHdr() const { return ScriptHasFileHdr; }

  bool hasPhdr() const { return ScriptHasPhdr; }

  Expression *atAddress() const { return FixedAddress; }

  bool lmaSet() const { return FixedAddress != nullptr; }

  Expression *flags() const { return SectionFlags; }

  const StrToken *Name = nullptr;
  uint32_t ThisType = 0;
  bool ScriptHasFileHdr = false;
  bool ScriptHasPhdr = false;
  Expression *FixedAddress = nullptr;
  Expression *SectionFlags = nullptr;
};

/** \class PhdrDesc
 *  \brief This class defines the interfaces to Phdr specification description.
 */

class PhdrDesc : public ScriptCommand {
public:
  PhdrDesc(const PhdrSpec &PSpec);



  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::PHDR_DESC;
  }

  const PhdrSpec &spec() const { return InputSpec; }

  const PhdrSpec *getPhdrSpec() const { return &InputSpec; }

  void dump(llvm::raw_ostream &Outs) const override;

  eld::Expected<void> activate(Module &CurModule) override;

private:
  PhdrSpec InputSpec;
};

} // namespace eld

#endif
