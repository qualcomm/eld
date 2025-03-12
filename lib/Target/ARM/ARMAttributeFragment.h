//===- ARMAttributeFragment.h----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


#ifndef ELD_FRAGMENT_ARM_ATTRIBUTE_FRAGMENT_H
#define ELD_FRAGMENT_ARM_ATTRIBUTE_FRAGMENT_H

#include "eld/Fragment/TargetFragment.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/ARMAttributeParser.h"
#include <string>

namespace llvm {
class ARMAttributeParser;
}

namespace eld {

class ARMGNULDBackend;
class ObjectFile;
class GNULDBackend;
class LinkerConfig;
class Module;

enum class ARMVFPArgKind { Default, Base, VFP, ToolChain };

struct OutputARMAttributes {
  bool armHasBlx = false;
  bool armJ1J2BranchEncoding = false;
  bool armHasMovtMovw = false;
  std::optional<unsigned> armR9Args;
  std::optional<unsigned> armEnumSize;
  std::optional<unsigned> ABI_PCS_RW_data;
  std::optional<unsigned> ABI_PCS_RO_data;
  std::optional<unsigned> cpuArchProfile;
  ARMVFPArgKind armVFPArgs = ARMVFPArgKind::Default;
};

class ARMAttributeFragment : public TargetFragment {
public:
  ARMAttributeFragment(ELFSection *O);

  virtual ~ARMAttributeFragment();

  /// name - name of this stub
  virtual const std::string name() const override;

  virtual size_t size() const override;

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Target;
  }

  static bool classof(const ARMAttributeFragment *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  virtual void dump(llvm::raw_ostream &OS) override;

  bool updateAttributes(llvm::StringRef Contents, eld::Module &M, ObjectFile *I,
                        const LinkerConfig &);

  bool isBLXSupported() const { return OutputAttributes.armHasBlx; }

  bool isCPUProfileMicroController() const;

  bool hasJ1J2Encoding() const;

  bool hasMovtMovW() const;

private:
  size_t calculateContentSize() const;

  void updateSupportedARMFeatures(const llvm::ARMAttributeParser &Parser,
                                  eld::Module &M, ObjectFile *f);

  bool updateARMVFPArgs(const llvm::ARMAttributeParser &attributes,
                        eld::Module &M, ObjectFile *f, const LinkerConfig &);

  bool updatePCS(const llvm::ARMAttributeParser &attributes, eld::Module &M,
                 ObjectFile *f, const LinkerConfig &);

  bool updatePCSRO(const llvm::ARMAttributeParser &attributes, eld::Module &M,
                   ObjectFile *f, const LinkerConfig &);

  bool updatePCSRW(const llvm::ARMAttributeParser &attributes, eld::Module &M,
                   ObjectFile *f, const LinkerConfig &);

  bool updateEnumSize(const llvm::ARMAttributeParser &attributes,
                      eld::Module &M, ObjectFile *f, const LinkerConfig &);

  bool updateCPUArchProfile(const llvm::ARMAttributeParser &attributes,
                            eld::Module &M, ObjectFile *f,
                            const LinkerConfig &);

protected:
  llvm::StringRef ARMAttributeContents;
  OutputARMAttributes OutputAttributes;
};

} // namespace eld

#endif
