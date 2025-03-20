//===- RISCVAttributeFragment.cpp------------------------------------------===//
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

#include "RISCVAttributeFragment.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/Support/ELFAttributes.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/RISCVAttributeParser.h"
#include "llvm/Support/RISCVAttributes.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// RISCVAttributeFragment
//===----------------------------------------------------------------------===//

RISCVAttributeFragment::RISCVAttributeFragment(ELFSection *O)
    : TargetFragment(TargetFragment::Kind::Attributes, O, nullptr,
                     O->getAddrAlign(), 0) {}

RISCVAttributeFragment::~RISCVAttributeFragment() {}

const std::string RISCVAttributeFragment::name() const {
  return "Fragment for RISCV Attributes";
}

size_t RISCVAttributeFragment::size() const { return m_Size; }

eld::Expected<void> RISCVAttributeFragment::emit(MemoryRegion &mr, Module &M) {
  uint8_t *buf = reinterpret_cast<uint8_t *>(mr.begin());
  size_t sz = size();
  uint8_t *const end = buf + size();
  *buf = llvm::ELFAttrs::Format_Version;
  llvm::support::endian::write32le(buf + 1, sz - 1);
  buf += 5;

  memcpy(buf, CurrentVendor.data(), CurrentVendor.size());
  buf += CurrentVendor.size() + 1;

  *buf = llvm::ELFAttrs::File;
  llvm::support::endian::write32le(buf + 1, end - buf);
  buf += 5;

  for (auto &attr : IntegerAttributes) {
    if (attr.second == 0)
      continue;
    buf += llvm::encodeULEB128(attr.first, buf);
    buf += llvm::encodeULEB128(attr.second, buf);
  }
  for (auto &attr : StringAttributes) {
    if (attr.second.empty())
      continue;
    buf += llvm::encodeULEB128(attr.first, buf);
    memcpy(buf, attr.second.data(), attr.second.size());
    buf += attr.second.size() + 1;
  }
  return {};
}

bool RISCVAttributeFragment::updateInfo(GNULDBackend *G) { return true; }

llvm::StringRef RISCVAttributeFragment::getTagStr(uint32_t Tag) const {
  if (Tag == llvm::RISCVAttrs::STACK_ALIGN)
    return "StackAlignment";
  if (Tag == llvm::RISCVAttrs::ARCH)
    return "Architecture";
  if (Tag == llvm::RISCVAttrs::UNALIGNED_ACCESS)
    return "UnalignedAccess";
  if (Tag == llvm::RISCVAttrs::PRIV_SPEC)
    return "PrivSpec";
  if (Tag == llvm::RISCVAttrs::PRIV_SPEC_MINOR)
    return "PrivSpecMinor";
  if (Tag == llvm::RISCVAttrs::PRIV_SPEC_MINOR)
    return "PrivSpecRevision";
  return "Undefined";
}

void RISCVAttributeFragment::dump(llvm::raw_ostream &OS) {
  OS << "# Vendor : " << CurrentVendor << "\n";
  for (AttributeItem item : Contents) {
    switch (item.Type) {
    case AttributeType::Hidden:
      break;
    case AttributeType::Numeric:
      OS << "# " << getTagStr(item.Tag);
      OS << " : " << item.IntValue;
      OS << "\n";
      break;
    case AttributeType::Text:
      OS << "# " << getTagStr(item.Tag);
      OS << " : " << item.StringValue;
      OS << "\n";
      break;
    case AttributeType::NumericAndText:
      OS << "# " << getTagStr(item.Tag);
      OS << " : (" << item.IntValue;
      OS << " , " << item.StringValue;
      OS << " )";
      OS << "\n";
      break;
    }
  }
}

bool RISCVAttributeFragment::updateInfo(llvm::StringRef Contents,
                                        InputFile *InputFile,
                                        DiagnosticEngine *DiagEngine,
                                        bool ShowAttributeMixWarnings) {
  llvm::RISCVAttributeParser Parser;
  bool retval = true;
  llvm::ArrayRef<uint8_t> Data =
      llvm::ArrayRef((const uint8_t *)Contents.data(), Contents.size());
  if (llvm::Error E = Parser.parse(Data, llvm::endianness::little)) {
    DiagEngine->raise(Diag::attribute_parsing_error)
        << InputFile->getInput()->decoratedPath()
        << llvm::toString(std::move(E));
    retval = false;
    return retval;
  }
  std::string Str;
  std::string OldStr;
  uint32_t Val;
  uint32_t OldVal;
  static class InputFile *PreviousInputFile = nullptr;
  std::string PreviousInputFileDecoratedPath = "";

  if (PreviousInputFile)
    PreviousInputFileDecoratedPath =
        PreviousInputFile->getInput()->decoratedPath();

  auto DiagID = Diag::riscv_attribute_parsing_mix_warn;

  if (getStringAttribute(Parser, llvm::RISCVAttrs::ARCH, Str)) {
    if (!addAttributeStringItem(llvm::RISCVAttrs::ARCH, Str, OldStr) &&
        ShowAttributeMixWarnings) {
      DiagEngine->raise(DiagID)
          << PreviousInputFileDecoratedPath
          << InputFile->getInput()->decoratedPath() << "ARCH" << Str << OldStr;
      retval = false;
    }
  }
  if (getIntegerAttribute(Parser, llvm::RISCVAttrs::PRIV_SPEC, Val)) {
    if (!addAttributeIntegerItem(llvm::RISCVAttrs::PRIV_SPEC, Val, OldVal) &&
        ShowAttributeMixWarnings) {
      DiagEngine->raise(DiagID) << PreviousInputFileDecoratedPath
                                << InputFile->getInput()->decoratedPath()
                                << "PRIV_SPEC" << Val << OldVal;
      retval = false;
    }
  }
  if (getIntegerAttribute(Parser, llvm::RISCVAttrs::PRIV_SPEC_MINOR, Val)) {
    if (!addAttributeIntegerItem(llvm::RISCVAttrs::PRIV_SPEC_MINOR, Val,
                                 OldVal)) {
      DiagEngine->raise(DiagID) << PreviousInputFileDecoratedPath
                                << InputFile->getInput()->decoratedPath()
                                << "PRIV_SPEC_MINOR" << Val << OldVal;
      retval = false;
    }
  }
  if (getIntegerAttribute(Parser, llvm::RISCVAttrs::PRIV_SPEC_REVISION, Val)) {
    if (!addAttributeIntegerItem(llvm::RISCVAttrs::PRIV_SPEC_REVISION, Val,
                                 OldVal) &&
        ShowAttributeMixWarnings) {
      DiagEngine->raise(DiagID) << PreviousInputFileDecoratedPath
                                << InputFile->getInput()->decoratedPath()
                                << "PRIV_SPEC_REVISION" << Val << OldVal;
      retval = false;
    }
  }

  if (getIntegerAttribute(Parser, llvm::RISCVAttrs::STACK_ALIGN, Val)) {
    if (!addAttributeIntegerItem(llvm::RISCVAttrs::STACK_ALIGN, Val, OldVal)) {
      DiagEngine->raise(Diag::riscv_attribute_parsing_mix_error)
          << PreviousInputFileDecoratedPath
          << InputFile->getInput()->decoratedPath() << "STACK_ALIGN" << Val
          << OldVal;
      retval = false;
    }
  }
  if (getIntegerAttribute(Parser, llvm::RISCVAttrs::UNALIGNED_ACCESS, Val)) {
    if (!addAttributeIntegerItem(llvm::RISCVAttrs::UNALIGNED_ACCESS, Val,
                                 OldVal) &&
        ShowAttributeMixWarnings) {
      DiagEngine->raise(DiagID) << PreviousInputFileDecoratedPath
                                << InputFile->getInput()->decoratedPath()
                                << "UNALIGNED_ACCESS" << Val << OldVal;
      retval = false;
    }
  }
  eld::Expected<void> E = mergeRISCVAttributes(Parser, InputFile);
  if (!E)
    DiagEngine->raiseDiagEntry(std::move(E.error()));
  PreviousInputFile = InputFile;
  return retval;
}

eld::Expected<void> RISCVAttributeFragment::mergeArch(
    llvm::RISCVISAUtils::OrderedExtensionMap &mergedExts, unsigned &mergedXlen,
    llvm::StringRef s, InputFile *I) {
  auto maybeInfo = llvm::RISCVISAInfo::parseNormalizedArchString(s);
  if (!maybeInfo) {
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::DiagnosticEntry(Diag::attribute_parsing_error,
                                {I->getInput()->decoratedPath(),
                                 llvm::toString(maybeInfo.takeError())}));
  }

  // Merge extensions.
  llvm::RISCVISAInfo &info = **maybeInfo;
  if (mergedExts.empty()) {
    mergedExts = info.getExtensions();
    mergedXlen = info.getXLen();
  } else {
    for (const auto &ext : info.getExtensions()) {
      if (auto it = mergedExts.find(ext.first); it != mergedExts.end()) {
        if (std::tie(it->second.Major, it->second.Minor) >=
            std::tie(ext.second.Major, ext.second.Minor))
          continue;
      }
      mergedExts[ext.first] = ext.second;
    }
  }
  return eld::Expected<void>();
}

eld::Expected<void> RISCVAttributeFragment::mergeRISCVAttributes(
    const llvm::RISCVAttributeParser &parser, InputFile *I) {
  bool hasArch = false;

  // Collect all tags values from attributes section.
  const auto &attributesTags = llvm::RISCVAttrs::getRISCVAttributeTags();
  for (const auto &tag : attributesTags) {
    switch (llvm::RISCVAttrs::AttrType(tag.attr)) {
      // Integer attributes.
    case llvm::RISCVAttrs::STACK_ALIGN: {
      if (auto i = parser.getAttributeValue(tag.attr)) {
        IntegerAttributes.try_emplace(tag.attr, *i);
      }
      continue;
    }
    case llvm::RISCVAttrs::UNALIGNED_ACCESS: {
      if (auto i = parser.getAttributeValue(tag.attr))
        IntegerAttributes[tag.attr] |= *i;
      continue;
    }

      // String attributes.
    case llvm::RISCVAttrs::ARCH: {
      if (auto s = parser.getAttributeString(tag.attr)) {
        hasArch = true;
        auto E = mergeArch(exts, xlen, *s, I);
        if (!E)
          return E;
      }
      continue;
    }

      // Attributes which use the default handling.
    case llvm::RISCVAttrs::PRIV_SPEC:
    case llvm::RISCVAttrs::PRIV_SPEC_MINOR:
    case llvm::RISCVAttrs::PRIV_SPEC_REVISION:
    case llvm::RISCVAttrs::ATOMIC_ABI:
      break;
    }

    // Fallback for deprecated priv_spec* and other unknown attributes: retain
    // the attribute if all input sections agree on the value. GNU ld uses 0
    // and empty strings as default values which are not dumped to the output.
    // TODO Adjust after resolution to
    // https://github.com/riscv-non-isa/riscv-elf-psabi-doc/issues/352
    if (tag.attr % 2 == 0) {
      if (auto i = parser.getAttributeValue(tag.attr)) {
        auto r = IntegerAttributes.try_emplace(tag.attr, *i);
        if (!r.second && r.first->second != *i)
          r.first->second = 0;
      }
    } else if (auto s = parser.getAttributeString(tag.attr)) {
      auto r = StringAttributes.try_emplace(tag.attr, *s);
      if (!r.second && r.first->second != *s)
        r.first->second = {};
    }
  }

  if (hasArch && xlen != 0) {
    if (auto result = llvm::RISCVISAInfo::createFromExtMap(xlen, exts)) {
      llvm::StringRef R = eld::Saver.save((*result)->toString());
      StringAttributes[llvm::RISCVAttrs::ARCH] = R;
    } else {
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          Diag::attribute_parsing_error, {I->getInput()->decoratedPath(),
                                          llvm::toString(result.takeError())}));
    }
  }

  // The total size of headers: format-version [ <section-length> "vendor-name"
  // [ <file-tag> <size>.
  size_t size = 5 + CurrentVendor.size() + 1 + 5;
  for (auto &attr : IntegerAttributes)
    if (attr.second != 0)
      size +=
          llvm::getULEB128Size(attr.first) + llvm::getULEB128Size(attr.second);
  for (auto &attr : StringAttributes)
    if (!attr.second.empty())
      size += llvm::getULEB128Size(attr.first) + attr.second.size() + 1;
  m_Size = size;
  return eld::Expected<void>();
}

bool RISCVAttributeFragment::getIntegerAttribute(
    llvm::RISCVAttributeParser &Parser, uint32_t Tag, uint32_t &Value) {
  std::optional<uint32_t> Val = Parser.getAttributeValue(Tag);
  if (Val) {
    Value = *Val;
    return true;
  }
  return false;
}

bool RISCVAttributeFragment::getStringAttribute(
    llvm::RISCVAttributeParser &Parser, uint32_t Tag, std::string &Value) {
  std::optional<llvm::StringRef> Val = Parser.getAttributeString(Tag);
  if (Val) {
    Value = Val->str();
    return true;
  }
  return false;
}

bool RISCVAttributeFragment::addAttributeIntegerItem(uint32_t Tag,
                                                     uint32_t Value,
                                                     uint32_t &OldValue) {
  AttributeItem Item;
  Item.Type = RISCVAttributeFragment::AttributeType::Numeric;
  Item.Tag = Tag;
  Item.IntValue = Value;
  Item.StringValue = "";
  for (auto &C : Contents) {
    if (C.Type != Item.Type)
      continue;
    if (C.Tag != Item.Tag)
      continue;
    if (C.IntValue != Value) {
      OldValue = C.IntValue;
      return false;
    } else
      return true;
  }
  Contents.push_back(Item);
  return true;
}

bool RISCVAttributeFragment::addAttributeStringItem(uint32_t Tag,
                                                    std::string Value,
                                                    std::string &OldValue) {
  AttributeItem Item;
  Item.Type = RISCVAttributeFragment::AttributeType::Text;
  Item.Tag = Tag;
  Item.IntValue = 0;
  Item.StringValue = Value;
  for (auto &C : Contents) {
    if (C.Type != Item.Type)
      continue;
    if (C.Tag != Item.Tag)
      continue;
    if (C.StringValue != Value) {
      OldValue = C.StringValue;
      return false;
    } else
      return true;
  }
  Contents.push_back(Item);
  return true;
}
