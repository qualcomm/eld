//===- ARMAttributeFragment.cpp--------------------------------------------===//
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

#include "ARMAttributeFragment.h"
#include "ARMLDBackend.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ObjectFile.h"
#include "llvm/Support/ARMAttributeParser.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// ARMAttributeFragment
//===----------------------------------------------------------------------===//

namespace {
#define R9StrCase(S)                                                           \
  case llvm::ARMBuildAttrs::R9Is##S:                                           \
    return #S;
static std::string R9Str(uint32_t R9) {
  switch (R9) {
    R9StrCase(GPR);
    R9StrCase(SB);
    R9StrCase(TLSPointer);
  case llvm::ARMBuildAttrs::R9Reserved:
    return "Reserved";
  default:
    break;
  }
  return std::to_string(R9);
}
#undef R9StrCase
#define PCS_RWCase(S)                                                          \
  case llvm::ARMBuildAttrs::AddressRW##S:                                      \
    return #S;
static std::string PCS_RWStr(uint32_t Base) {
  switch (Base) {
    PCS_RWCase(PCRel);
    PCS_RWCase(SBRel);
    PCS_RWCase(None);
  default:
    break;
  }
  return std::to_string(Base);
}
#undef PCS_RWCase
#define PCS_ROCase(S)                                                          \
  case llvm::ARMBuildAttrs::AddressRO##S:                                      \
    return #S;
static std::string PCS_ROStr(uint32_t Base) {
  switch (Base) {
    PCS_ROCase(PCRel);
    PCS_ROCase(None);
  default:
    break;
  }
  return std::to_string(Base);
}
#undef PCS_ROCase

#define CPUArchProfileCase(S)                                                  \
  case llvm::ARMBuildAttrs::S##Profile:                                        \
    return #S;
static std::string CPUArchProfileStr(uint32_t CPUProfile) {
  switch (CPUProfile) {
    CPUArchProfileCase(Application);
    CPUArchProfileCase(RealTime);
    CPUArchProfileCase(MicroController);
    CPUArchProfileCase(System);
  default:
    break;
  }
  return "";
}
#undef CPUArchProfileCase
} // namespace

ARMAttributeFragment::ARMAttributeFragment(ELFSection *O)
    : TargetFragment(TargetFragment::Kind::Attributes, O, nullptr,
                     O->getAddrAlign(), 0) {}

ARMAttributeFragment::~ARMAttributeFragment() {}

const std::string ARMAttributeFragment::name() const {
  return "Fragment for ARM Attributes";
}

size_t ARMAttributeFragment::size() const {
  return ARMAttributeContents.size();
}

eld::Expected<void> ARMAttributeFragment::emit(MemoryRegion &mr, Module &M) {
  if (!size())
    return {};
  uint8_t *out = mr.begin() + getOffset(M.getConfig().getDiagEngine());
  memcpy(out, ARMAttributeContents.begin(), size());
  return {};
}

void ARMAttributeFragment::dump(llvm::raw_ostream &OS) {}

void ARMAttributeFragment::updateSupportedARMFeatures(
    const llvm::ARMAttributeParser &Parser, eld::Module &M, ObjectFile *Obj) {
  DiagnosticEngine *DiagEngine = M.getConfig().getDiagEngine();
  std::optional<unsigned> attr =
      Parser.getAttributeValue(llvm::ARMBuildAttrs::CPU_arch);
  if (!attr.has_value())
    return;
  auto arch = attr.value();
  switch (arch) {
  case llvm::ARMBuildAttrs::Pre_v4:
  case llvm::ARMBuildAttrs::v4:
  case llvm::ARMBuildAttrs::v4T:
    // Architectures prior to v5 do not support BLX instruction
    break;
  case llvm::ARMBuildAttrs::v5T:
  case llvm::ARMBuildAttrs::v5TE:
  case llvm::ARMBuildAttrs::v5TEJ:
  case llvm::ARMBuildAttrs::v6:
  case llvm::ARMBuildAttrs::v6KZ:
  case llvm::ARMBuildAttrs::v6K: {
    if (M.getPrinter()->isVerbose())
      DiagEngine->raise(diag::record_arm_attribute)
          << "BLX" << Obj->getInput()->decoratedPath();
    OutputAttributes.armHasBlx = true;
    Obj->recordFeature("blx");
    // Architectures used in pre-Cortex processors do not support
    // The J1 = 1 J2 = 1 Thumb branch range extension, with the exception
    // of Architecture v6T2 (arm1156t2-s and arm1156t2f-s) that do.
    break;
  }
  default:
    // All other Architectures have BLX and extended branch encoding
    OutputAttributes.armHasBlx = true;
    Obj->recordFeature("blx");
    if (M.getPrinter()->isVerbose())
      DiagEngine->raise(diag::record_arm_attribute)
          << "BLX" << Obj->getInput()->decoratedPath();
    OutputAttributes.armJ1J2BranchEncoding = true;
    Obj->recordFeature("j1j2");
    if (M.getPrinter()->isVerbose())
      DiagEngine->raise(diag::record_arm_attribute)
          << "J1J2" << Obj->getInput()->decoratedPath();
    if (arch != llvm::ARMBuildAttrs::v6_M &&
        arch != llvm::ARMBuildAttrs::v6S_M) {
      // All Architectures used in Cortex processors with the exception
      // of v6-M and v6S-M have the MOVT and MOVW instructions.
      OutputAttributes.armHasMovtMovw = true;
      Obj->recordFeature("movtmovw");
      if (M.getPrinter()->isVerbose())
        DiagEngine->raise(diag::record_arm_attribute)
            << "MovtMovw" << Obj->getInput()->decoratedPath();
      break;
    }
  }
}

// For ARM only, to set the EF_ARM_ABI_FLOAT_SOFT or EF_ARM_ABI_FLOAT_HARD
// flag in the ELF Header we need to look at Tag_ABI_VFP_args to find out how
// the input objects have been compiled.
bool ARMAttributeFragment::updateARMVFPArgs(
    const llvm::ARMAttributeParser &attributes, eld::Module &M, ObjectFile *f,
    const LinkerConfig &Config) {
  DiagnosticEngine *DiagEngine = Config.getDiagEngine();
  std::optional<unsigned> attr =
      attributes.getAttributeValue(llvm::ARMBuildAttrs::ABI_VFP_args);
  if (!attr.has_value())
    // If an ABI tag isn't present then it is implicitly given the value of 0
    // which maps to ARMBuildAttrs::BaseAAPCS. However many assembler files,
    // including some in glibc that don't use FP args (and should have value 3)
    // don't have the attribute so we do not consider an implicit value of 0
    // as a clash.
    return true;

  unsigned vfpArgs = attr.value();
  ARMVFPArgKind arg;
  switch (vfpArgs) {
  case llvm::ARMBuildAttrs::BaseAAPCS:
    arg = ARMVFPArgKind::Base;
    break;
  case llvm::ARMBuildAttrs::HardFPAAPCS:
    arg = ARMVFPArgKind::VFP;
    break;
  case llvm::ARMBuildAttrs::ToolChainFPPCS:
    // Tool chain specific convention that conforms to neither AAPCS variant.
    arg = ARMVFPArgKind::ToolChain;
    break;
  case llvm::ARMBuildAttrs::CompatibleFPAAPCS:
    // Object compatible with all conventions.
    return true;
  default: {
    if (!Config.options().noWarnMismatch()) {
      std::string ErrorMsg =
          "unknown Tag_ABI_VFP_args value: " + std::to_string(vfpArgs);
      DiagEngine->raise(diag::attribute_parsing_error)
          << f->getInput()->decoratedPath() << ErrorMsg;
      return false;
    }
  }
  }
  // Follow ld.bfd and error if there is a mix of calling conventions.
  if (!Config.options().noWarnMismatch() &&
      (OutputAttributes.armVFPArgs != arg &&
       OutputAttributes.armVFPArgs != ARMVFPArgKind::Default)) {
    DiagEngine->raise(diag::attribute_parsing_error)
        << f->getInput()->decoratedPath() << "incompatible Tag_ABI_VFP_args";
    return false;
  }
  std::string VFPStr = "ARM VFP " + std::to_string((uint32_t)arg);
  f->recordFeature(VFPStr);
  if (M.getPrinter()->isVerbose()) {
    DiagEngine->raise(diag::record_arm_attribute)
        << VFPStr << f->getInput()->decoratedPath();
  }
  OutputAttributes.armVFPArgs = arg;
  return true;
}

bool ARMAttributeFragment::updatePCS(const llvm::ARMAttributeParser &attributes,
                                     eld::Module &M, ObjectFile *f,
                                     const LinkerConfig &Config) {
  DiagnosticEngine *DiagEngine = Config.getDiagEngine();
  std::optional<unsigned> attr =
      attributes.getAttributeValue(llvm::ARMBuildAttrs::ABI_PCS_R9_use);
  if (!attr.has_value())
    return true;
  unsigned r9args = attr.value();
  std::string R9_Str = R9Str(r9args);
  f->recordFeature(R9_Str);
  if (M.getPrinter()->isVerbose()) {
    DiagEngine->raise(diag::record_arm_attribute)
        << R9_Str << f->getInput()->decoratedPath();
  }
  if (!OutputAttributes.armR9Args) {
    OutputAttributes.armR9Args = r9args;
    return true;
  }
  if (!Config.options().noWarnMismatch() &&
      (OutputAttributes.armR9Args != r9args)) {
    DiagEngine->raise(diag::err_mismatch_r9_use)
        << R9Str(*OutputAttributes.armR9Args) << R9Str(r9args)
        << f->getInput()->decoratedPath();
    return false;
  }
  return true;
}

bool ARMAttributeFragment::updatePCSRO(
    const llvm::ARMAttributeParser &attributes, eld::Module &M, ObjectFile *f,
    const LinkerConfig &Config) {
  std::optional<unsigned> attr =
      attributes.getAttributeValue(llvm::ARMBuildAttrs::ABI_PCS_RO_data);
  if (!attr.has_value())
    return true;
  unsigned ABI_PCS_RO_data_val = attr.value();
  std::string PCS_RO_data_str = PCS_ROStr(ABI_PCS_RO_data_val);
  f->recordFeature(PCS_RO_data_str);
  if (M.getPrinter()->isVerbose()) {
    Config.raise(diag::record_arm_attribute)
        << PCS_RO_data_str << f->getInput()->decoratedPath();
  }
  if (!OutputAttributes.ABI_PCS_RO_data) {
    OutputAttributes.ABI_PCS_RO_data = ABI_PCS_RO_data_val;
    return true;
  }
  if (!Config.options().noWarnMismatch() &&
      (OutputAttributes.ABI_PCS_RO_data != ABI_PCS_RO_data_val)) {
    Config.raise(diag::err_mismatch_r9_use)
        << PCS_ROStr(*OutputAttributes.ABI_PCS_RO_data)
        << PCS_ROStr(ABI_PCS_RO_data_val) << f->getInput()->decoratedPath();
    return false;
  }
  return true;
}

bool ARMAttributeFragment::updatePCSRW(
    const llvm::ARMAttributeParser &attributes, eld::Module &M, ObjectFile *f,
    const LinkerConfig &Config) {
  DiagnosticEngine *DiagEngine = Config.getDiagEngine();
  std::optional<unsigned> attr =
      attributes.getAttributeValue(llvm::ARMBuildAttrs::ABI_PCS_RW_data);
  if (!attr.has_value())
    return true;
  unsigned ABI_PCS_RW_data_val = attr.value();
  std::string PCS_RW_data_str = PCS_RWStr(ABI_PCS_RW_data_val);
  f->recordFeature(PCS_RW_data_str);
  if (M.getPrinter()->isVerbose()) {
    DiagEngine->raise(diag::record_arm_attribute)
        << PCS_RW_data_str << f->getInput()->decoratedPath();
  }
  if (!OutputAttributes.ABI_PCS_RW_data) {
    OutputAttributes.ABI_PCS_RW_data = ABI_PCS_RW_data_val;
    return true;
  }
  if (!Config.options().noWarnMismatch() &&
      (OutputAttributes.ABI_PCS_RW_data != ABI_PCS_RW_data_val)) {
    DiagEngine->raise(diag::err_mismatch_r9_use)
        << PCS_RWStr(*OutputAttributes.ABI_PCS_RW_data)
        << PCS_RWStr(ABI_PCS_RW_data_val) << f->getInput()->decoratedPath();
    return false;
  }
  return true;
}

bool ARMAttributeFragment::updateEnumSize(
    const llvm::ARMAttributeParser &attributes, eld::Module &M, ObjectFile *f,
    const LinkerConfig &Config) {
  DiagnosticEngine *DiagEngine = Config.getDiagEngine();
  std::optional<unsigned> attr =
      attributes.getAttributeValue(llvm::ARMBuildAttrs::ABI_enum_size);
  if (!attr.has_value())
    return true;
  unsigned enumSize = attr.value();
  std::string enumSizeStr = "EnumSize " + std::to_string(enumSize);
  f->recordFeature(enumSizeStr);
  if (M.getPrinter()->isVerbose()) {
    DiagEngine->raise(diag::record_arm_attribute)
        << enumSizeStr << f->getInput()->decoratedPath();
  }
  if (!OutputAttributes.armEnumSize) {
    OutputAttributes.armEnumSize = enumSize;
    return true;
  }
  if (!Config.options().noWarnMismatch() &&
      (OutputAttributes.armEnumSize != enumSize)) {
    DiagEngine->raise(diag::warn_mismatch_enum_size)
        << f->getInput()->decoratedPath() << enumSize
        << *OutputAttributes.armEnumSize;
    return false;
  }
  return true;
}

bool ARMAttributeFragment::updateCPUArchProfile(
    const llvm::ARMAttributeParser &attributes, eld::Module &M, ObjectFile *f,
    const LinkerConfig &Config) {
  DiagnosticEngine *DiagEngine = Config.getDiagEngine();
  std::optional<unsigned> attr =
      attributes.getAttributeValue(llvm::ARMBuildAttrs::CPU_arch_profile);
  if (!attr.has_value())
    return true;
  unsigned cpuArchProfile = attr.value();
  f->recordFeature(CPUArchProfileStr(cpuArchProfile));
  if (M.getPrinter()->isVerbose()) {
    std::string cpuArchProfileStr =
        "CPUArchProfile " + CPUArchProfileStr(cpuArchProfile);
    DiagEngine->raise(diag::record_arm_attribute)
        << cpuArchProfileStr << f->getInput()->decoratedPath();
  }
  if (!OutputAttributes.cpuArchProfile) {
    OutputAttributes.cpuArchProfile = cpuArchProfile;
    return true;
  }
  if (OutputAttributes.cpuArchProfile != cpuArchProfile) {
    DiagEngine->raise(diag::err_mismatch_attr)
        << "CPU Arch Profile" << f->getInput()->decoratedPath()
        << CPUArchProfileStr(cpuArchProfile)
        << CPUArchProfileStr(*OutputAttributes.cpuArchProfile);
    return false;
  }
  return true;
}

bool ARMAttributeFragment::updateAttributes(llvm::StringRef Contents,
                                            eld::Module &M, ObjectFile *Obj,
                                            const LinkerConfig &Config) {
  DiagnosticEngine *DiagEngine = Config.getDiagEngine();
  llvm::ARMAttributeParser Parser;
  bool retval = true;
  llvm::ArrayRef<uint8_t> Data =
      llvm::ArrayRef((const uint8_t *)Contents.data(), Contents.size());
  if (llvm::Error E = Parser.parse(Data, llvm::endianness::little)) {
    DiagEngine->raise(diag::attribute_parsing_error)
        << Obj->getInput()->decoratedPath() << llvm::toString(std::move(E));
    // Ignore error
    (void)(std::move(E));
    retval = false;
    return retval;
  }
  updateSupportedARMFeatures(Parser, M, Obj);
  if (!updateARMVFPArgs(Parser, M, Obj, Config))
    retval = false;
  if (!updatePCS(Parser, M, Obj, Config))
    retval = false;
  if (!updatePCSRO(Parser, M, Obj, Config))
    retval = false;
  if (!updatePCSRW(Parser, M, Obj, Config))
    retval = false;
  if (!updateEnumSize(Parser, M, Obj, Config))
    retval = false;
  if (!updateCPUArchProfile(Parser, M, Obj, Config))
    retval = false;
  if (!ARMAttributeContents.size())
    ARMAttributeContents = Contents;
  return retval;
}

bool ARMAttributeFragment::isCPUProfileMicroController() const {
  if (!OutputAttributes.cpuArchProfile)
    return false;
  return (*OutputAttributes.cpuArchProfile ==
          llvm::ARMBuildAttrs::MicroControllerProfile);
}

bool ARMAttributeFragment::hasJ1J2Encoding() const {
  return OutputAttributes.armJ1J2BranchEncoding;
}

bool ARMAttributeFragment::hasMovtMovW() const {
  return OutputAttributes.armHasMovtMovw;
}
