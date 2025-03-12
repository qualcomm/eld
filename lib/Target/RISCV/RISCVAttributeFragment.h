//===- RISCVAttributeFragment.h--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_FRAGMENT_RISCV_ATTRIBUTE_FRAGMENT_H
#define ELD_FRAGMENT_RISCV_ATTRIBUTE_FRAGMENT_H

#include "eld/Fragment/TargetFragment.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/TargetParser/RISCVISAInfo.h"
#include <string>
#include <vector>

namespace llvm {
class RISCVAttributeParser;
}

namespace eld {

class DiagEngine;
class InputFile;
class GNULDBackend;
class LinkerConfig;

class RISCVAttributeFragment : public TargetFragment {
public:
  enum class AttributeType { Hidden, Numeric, Text, NumericAndText };

  struct AttributeItem {
    AttributeType Type;
    unsigned Tag;
    unsigned IntValue;
    std::string StringValue;
  };

  RISCVAttributeFragment(ELFSection *O);

  virtual ~RISCVAttributeFragment();

  /// name - name of this stub
  virtual const std::string name() const override;

  virtual size_t size() const override;

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Target;
  }

  static bool classof(const RISCVAttributeFragment *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  bool updateInfo(GNULDBackend *G) override;

  virtual void dump(llvm::raw_ostream &OS) override;

  bool updateInfo(llvm::StringRef Contents, InputFile *I,
                  DiagnosticEngine *DiagEngine, bool ShowAttributeMixWarnings);

private:
  bool getStringAttribute(llvm::RISCVAttributeParser &, uint32_t Tag,
                          std::string &Value);

  bool getIntegerAttribute(llvm::RISCVAttributeParser &, uint32_t Tag,
                           uint32_t &Value);

  bool addAttributeIntegerItem(uint32_t Tag, uint32_t Value, uint32_t &OldVal);

  bool addAttributeStringItem(uint32_t Tag, std::string Value,
                              std::string &OldVal);

  eld::Expected<void>
  mergeArch(llvm::RISCVISAUtils::OrderedExtensionMap &mergedExts,
            unsigned &mergedXlen, llvm::StringRef s, InputFile *);

  eld::Expected<void>
  mergeRISCVAttributes(const llvm::RISCVAttributeParser &parser, InputFile *I);

  llvm::StringRef getTagStr(uint32_t Tag) const;

protected:
  llvm::StringRef CurrentVendor = "riscv";
  llvm::SmallVector<AttributeItem, 64> Contents;
  llvm::DenseMap<unsigned, unsigned> IntegerAttributes;
  llvm::DenseMap<unsigned, llvm::StringRef> StringAttributes;
  llvm::RISCVISAUtils::OrderedExtensionMap exts;
  unsigned xlen = 0;
  size_t m_Size = 0;
};

} // namespace eld

#endif
