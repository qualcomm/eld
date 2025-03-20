//===- OutputSectData.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_OUTPUTSECTDATA_H
#define ELD_SCRIPT_OUTPUTSECTDATA_H

#include "eld/Script/InputSectDesc.h"
#include "eld/Script/ScriptCommand.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {
class ELFSection;
class Expression;
class Fragment;
class Module;
class OutputSectDesc;

/// OutputSectData represents commands that are used to explicitly insert bytes
/// of data in an output section. These commands include: BYTE, SHORT, LONG,
/// QUAD, and SQUAD.
///
/// But why is OutputSectData inheriting from InputSectDesc?
///
/// Internally, we treat explicit output section data as a special input
/// section.
///
/// There are many motivations for this. Important ones are:
/// - It allows us to use the existing rule-matching framework for correctly
///   handling and placing explicit output section data.
/// - It allows the plugins to move and rearrange input content (sections
///   and explicit data) in a consistent and uniform manner.
/// - It allows us to use the existing diagnostic framework for sections and
///   fragments for explicit output section data.
///
/// This special input section needs a special input section description as
/// well. An OutputSectData object represents this special input section
/// description. Linker creates the section for the explicit data when this
/// special input section description is processed.
class OutputSectData : public InputSectDesc {
public:
  enum OSDKind { None, Byte, Short, Long, Quad, Squad };

  /// Creates an OutputSectData object.
  static OutputSectData *create(uint32_t ID, OutputSectDesc &OutSectDesc,
                                OSDKind Kind, Expression &Expr);

  // FIXME: Ideally, it should be private. Users of this class should only
  // use OutputSectData::Create function to create objects of this class.
  // It needs to be public so that eld::make can be used to handle lifetime
  // of the object.
  OutputSectData(uint32_t ID, InputSectDesc::Policy Policy,
                 const InputSectDesc::Spec Spec, OutputSectDesc &OutSectDesc,
                 OSDKind Kind, Expression &Expr);

  void dump(llvm::raw_ostream &Outs) const override;

  void dumpMap(llvm::raw_ostream &Outs, bool UseColor = false,
               bool UseNewLine = true, bool WithValues = false,
               bool AddIndent = true) const override;

  void dumpOnlyThis(llvm::raw_ostream &Outs) const override;

  /// It creates a section that contains the explicit output section data, and
  /// assign it to the output section.
  eld::Expected<void> activate(Module &) override;

  /// Returns the textual representation of output section data kind.
  llvm::StringRef getOSDKindAsStr() const;

  /// Returns the size of the data.
  std::size_t getDataSize() const;

  ELFSection *getELFSection() const { return ThisSectionion; }

  Expression &getExpr() { return ExpressionToEvaluate; }

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::OUTPUT_SECT_DATA;
  }

  static constexpr uint32_t DefaultSectionType = llvm::ELF::SHT_PROGBITS;
  static constexpr uint32_t DefaultSectionFlags = llvm::ELF::SHF_ALLOC;

private:
  /// Creates the section along with the required fragment for the output
  /// section data.
  ELFSection *createOSDSection(Module &Module);

  const OSDKind MOsdKind = OSDKind::None;
  Expression &ExpressionToEvaluate;
  ELFSection *ThisSectionion = nullptr;
};
} // namespace eld
#endif
