//===- HexagonAttributeFragment.cpp----------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "HexagonAttributeFragment.h"
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/HexagonAttributeParser.h"
#include "llvm/Support/LEB128.h"

using namespace eld;

HexagonAttributeFragment::HexagonAttributeFragment(ELFSection *O)
    : TargetFragment(TargetFragment::Kind::Attributes, O, nullptr,
                     O->getAddrAlign(), 0) {}

HexagonAttributeFragment::~HexagonAttributeFragment() {}

llvm::ArrayRef<uint8_t> HexagonAttributeFragment::getContent() const {
  return {};
}

bool HexagonAttributeFragment::classof(const Fragment *F) {
  return F->getKind() == Fragment::Target &&
         F->getOwningSection()->getType() == llvm::ELF::SHT_HEXAGON_ATTRIBUTES;
}

size_t HexagonAttributeFragment::size() const { return Size; }

eld::Expected<void> HexagonAttributeFragment::emit(MemoryRegion &R, Module &M) {
  const uint64_t Size = size();
  uint8_t *Buf = R.begin() + getOffset(M.getConfig().getDiagEngine());
  uint8_t *End = Buf + Size;

  *Buf = llvm::ELFAttrs::Format_Version;
  llvm::support::endian::write32le(Buf + 1, Size - 1);
  Buf += 5;

  memcpy(Buf, Vendor.begin(), Vendor.size());
  Buf += Vendor.size() + 1;

  *Buf = llvm::ELFAttrs::File;
  llvm::support::endian::write32le(Buf + 1, End - Buf);
  Buf += 5;

  for (auto &[Tag, Value] : Attrs) {
    if (!Value)
      continue;
    Buf += (llvm::encodeULEB128(Tag, Buf));
    Buf += (llvm::encodeULEB128(Value, Buf));
  }

  assert(Buf == End);
  return {};
}

static void addFeatures(llvm::TagNameItem Tag, unsigned Value, ObjectFile &O) {
  std::string FeatureStr;
  switch (Tag.attr) {
  case llvm::HexagonAttrs::ARCH:
    FeatureStr = "v" + std::to_string(Value);
    break;
  case llvm::HexagonAttrs::HVXARCH:
    FeatureStr = "hvxv" + std::to_string(Value);
    break;
  case llvm::HexagonAttrs::HVXIEEEFP:
    if (Value)
      FeatureStr = "hvx-ieee-fp";
    break;
  case llvm::HexagonAttrs::HVXQFLOAT:
    if (Value)
      FeatureStr = "hvx-qfloat";
    break;
  case llvm::HexagonAttrs::ZREG:
  case llvm::HexagonAttrs::AUDIO:
  case llvm::HexagonAttrs::CABAC:
    if (Value)
      FeatureStr = Tag.tagName.drop_front(4);
    break;
  default:
    FeatureStr = "unknown" + std::to_string(Value);
    break;
  }
  O.recordFeature(FeatureStr);
}

bool HexagonAttributeFragment::update(ELFSection &S, DiagnosticEngine &Engine,
                                      ObjectFile &O, bool AddFeatures) {
  llvm::HexagonAttributeParser Parser;
  llvm::ArrayRef<uint8_t> Data(
      reinterpret_cast<const uint8_t *>(S.getContents().data()), S.size());
  if (auto E = Parser.parse(Data, llvm::endianness::little)) {
    Engine.raise(Diag::warn_attribute_parse_fail)
        << S.getInputFile()->getInput()->decoratedPath() << S.name()
        << llvm::toString(std::move(E));
  }
  for (const auto &Tag : llvm::HexagonAttrs::getHexagonAttributeTags()) {
    if (auto V = Parser.getAttributeValue(Tag.attr)) {
      /// FIXME: Error/Warning when attributes are incompatible
      if (Attrs.try_emplace(Tag.attr, *V).second)
        Size += llvm::getULEB128Size(Tag.attr) + llvm::getULEB128Size(*V);
      else if (*V > Attrs.at(Tag.attr))
        Attrs[Tag.attr] = *V;
      if (AddFeatures)
        addFeatures(Tag, *V, O);
    }
  }
  return true;
}
