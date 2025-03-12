//===- DWARF.cpp-----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "DWARF.h"
#include "PluginADT.h"
#include "PluginBase.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Input/InputFile.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"

using namespace llvm::dwarf;
using namespace eld;

//
//-----------------------------DWARFInfo-------------------------------
//

plugin::DWARFInfo::DWARFInfo(llvm::DWARFContext *DC) { m_DWARFContext = DC; }

// FIXME: Remove
plugin::DWARFInfo::DWARFInfo(plugin::InputFile F) {
  llvm::MemoryBufferRef Buffer(F.getInputFile()->getContents(),
                               F.getFileName());

  llvm::Expected<std::unique_ptr<llvm::object::ObjectFile>> ObjectFileOrError =
      llvm::object::ObjectFile::createObjectFile(Buffer);

  if (!ObjectFileOrError) {
    llvm::errs() << ObjectFileOrError.takeError() << "\n";
    return;
  }

  m_ObjectFile = ObjectFileOrError->release();

  std::unique_ptr<llvm::DWARFContext> Context =
      llvm::DWARFContext::create(*m_ObjectFile);

  m_DWARFContext = Context.release();
  ShouldDeleteDWARFContext = true;
}

plugin::DWARFInfo::~DWARFInfo() {
  if (ShouldDeleteDWARFContext)
    delete m_DWARFContext;
  delete m_ObjectFile;
}

std::vector<plugin::DWARFUnit> plugin::DWARFInfo::getDWARFUnits() const {
  std::vector<plugin::DWARFUnit> DUs;
  for (auto &DU : m_DWARFContext->compile_units())
    DUs.push_back(plugin::DWARFUnit(DU.get()));
  return DUs;
}

//
//-----------------------------DWARFUnit-------------------------------
//

plugin::DWARFUnit::DWARFUnit(llvm::DWARFUnit *DU) { m_DWARFUnit = DU; }

std::vector<plugin::DWARFDie> plugin::DWARFUnit::getDIEs() const {
  std::vector<plugin::DWARFDie> Dies;
  for (auto &Die : m_DWARFUnit->dies())
    Dies.push_back(plugin::DWARFDie(m_DWARFUnit, &Die));
  return Dies;
}

bool plugin::DWARFUnit::isCompileUnit() const {
  return m_DWARFUnit && !m_DWARFUnit->isTypeUnit();
}

plugin::DWARFDie plugin::DWARFUnit::getCompileUnitDIE() const {
  llvm::DWARFDie Die = m_DWARFUnit->getUnitDIE(false);
  return plugin::DWARFDie(
      const_cast<llvm::DWARFUnit *>(Die.getDwarfUnit()),
      const_cast<llvm::DWARFDebugInfoEntry *>(Die.getDebugInfoEntry()));
}

//
//-----------------------------DWARFDie-------------------------------
//

plugin::DWARFDie::DWARFDie(llvm::DWARFUnit *U, llvm::DWARFDebugInfoEntry *Die) {
  m_DWARFUnit = U;
  m_DWARFDebugInfoEntry = Die;
}

std::string plugin::DWARFDie::getCompDir() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  std::optional<llvm::DWARFFormValue> Attribute = Die.find(DW_AT_comp_dir);
  if (!Attribute)
    return "";
  llvm::Expected<const char *> Str = Attribute->getAsCString();
  if (!Str) {
    consumeError(Str.takeError());
    return "";
  }
  return *Str;
}

std::string plugin::DWARFDie::toString() const {
  std::string SS;
  llvm::raw_string_ostream OS(SS);
  llvm::DWARFDie(m_DWARFUnit, m_DWARFDebugInfoEntry).dump(OS);
  return SS;
}

bool plugin::DWARFDie::isSubprogramDIE() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.isSubprogramDIE();
}

std::vector<plugin::DWARFAttribute> plugin::DWARFDie::getAttributes() const {
  std::vector<plugin::DWARFAttribute> Attributes;
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  for (const llvm::DWARFAttribute &attr : Die.attributes()) {
    Attributes.push_back(plugin::DWARFAttribute(&attr));
  }
  return Attributes;
}

const std::string plugin::DWARFDie::getName() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  std::optional<llvm::DWARFFormValue> V = Die.find(DW_AT_name);
  if (!V)
    return "";

  auto Value = V->getAsCString();
  return Value ? *Value : "";
}

const std::string plugin::DWARFDie::getDeclFile() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getDeclFile(
      llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath);
}

uint64_t plugin::DWARFDie::getDeclLine() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getDeclLine();
}

uint64_t plugin::DWARFDie::getOffset() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getOffset();
}

uint32_t plugin::DWARFDie::getTagAsUnsigned() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag();
}

bool plugin::DWARFDie::isType() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return llvm::dwarf::isType(Die.getTag());
}

bool plugin::DWARFDie::getLowAndHighPC(uint64_t &LowPC,
                                       uint64_t &HighPC) const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  uint64_t Dummy;
  return Die.getLowAndHighPC(LowPC, HighPC, Dummy);
}

const std::string plugin::DWARFDie::getFingerPrint() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  if (!Die.isSubprogramDIE())
    return "";
  bool FingerPrintIsNext = false;
  for (const llvm::DWARFAttribute &attr : Die.attributes()) {
    if (attr.Attr == DW_AT_name) {
      if (FingerPrintIsNext) {
        llvm::Expected<const char *> Fingerprint = attr.Value.getAsCString();
        if (!Fingerprint) {
          consumeError(Fingerprint.takeError());
          return "";
        }
        return *Fingerprint;
      } else {
        FingerPrintIsNext = true;
      }
    }
  }
  return "";
}

bool plugin::DWARFDie::getTotalArraySize(uint64_t &Size) const {
  Size = 0;
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  if (Die.getTag() != DW_TAG_array_type)
    return false;
  // Each DW_TAG_array_type DIE has a child DIE of type DW_TAG_subrange_type
  // with DW_AT_count describing the size of that dimension. Return the product
  // of these dimensions Eg: return 50 for the total count of the below array x
  // int x[5][10];
  // DW_TAG_array_type
  //               DW_AT_type      (0x00000041 "int")
  //
  // 0x00000034:     DW_TAG_subrange_type
  //                   DW_AT_type    (0x00000048 "__ARRAY_SIZE_TYPE__")
  //                   DW_AT_count   (0x05)
  //
  // 0x0000003a:     DW_TAG_subrange_type
  //                   DW_AT_type    (0x00000048 "__ARRAY_SIZE_TYPE__")
  //                   DW_AT_count   (0x0a)
  uint64_t total = 1;
  for (const llvm::DWARFDie &Child : Die.children()) {
    if (Child.getTag() != DW_TAG_subrange_type)
      continue;
    std::optional<uint64_t> count = toUnsigned(Child.find(DW_AT_count));
    if (!count)
      return false;
    total *= *count;
  }
  Size = total;
  return true;
}

bool getSize(llvm::DWARFDie &Die, uint64_t &Size, uint32_t PointerSize) {
  if (!Die)
    return false;
  switch (Die.getTag()) {
  case DW_TAG_base_type:
  case DW_TAG_enumeration_type:
  case DW_TAG_structure_type:
  case DW_TAG_class_type:
  case DW_TAG_union_type: {
    std::optional<uint64_t> V = toUnsigned(Die.find(DW_AT_byte_size));
    if (!V)
      return false;
    Size = *V;
    return true;
  }

  case DW_TAG_pointer_type:
  case DW_TAG_reference_type:
  case DW_TAG_unspecified_type:
  case DW_TAG_subroutine_type:
  case DW_TAG_ptr_to_member_type: {
    Size = PointerSize;
    return true;
  }

  case DW_TAG_restrict_type:
  case DW_TAG_const_type:
  case DW_TAG_volatile_type:
  case DW_TAG_typedef:
  case DW_TAG_member: {
    llvm::DWARFDie ReferencedDie =
        Die.getAttributeValueAsReferencedDie(DW_AT_type);
    return ::getSize(ReferencedDie, Size, PointerSize);
  }
  case DW_TAG_array_type: {
    plugin::DWARFDie Array(
        const_cast<llvm::DWARFUnit *>(Die.getDwarfUnit()),
        const_cast<llvm::DWARFDebugInfoEntry *>(Die.getDebugInfoEntry()));
    uint64_t ArrayCount = -1;
    if (!Array.getTotalArraySize(ArrayCount))
      return false;
    uint64_t BaseTypeSize = -1;
    llvm::DWARFDie ReferencedDie =
        Die.getAttributeValueAsReferencedDie(DW_AT_type);
    if (!::getSize(ReferencedDie, BaseTypeSize, PointerSize))
      return false;
    Size = ArrayCount * BaseTypeSize;
    return true;
  }
  default:
    ASSERT(false, "found unknown type " + TagString(Die.getTag()).str() +
                      " @Offset 0x" + llvm::utohexstr(Die.getOffset(), true));
  }
}

std::vector<plugin::DWARFDie> plugin::DWARFDie::getInlines() const {
  std::vector<plugin::DWARFDie> Inlines;
  for (const plugin::DWARFDie &D : getChildren())
    if (D.isInlinedSubroutine())
      Inlines.push_back(D);
  return Inlines;
}

plugin::DWARFDie plugin::DWARFDie::getReferencedOrigin() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  llvm::DWARFDie Referenced =
      Die.getAttributeValueAsReferencedDie(DW_AT_abstract_origin);
  if (!Referenced)
    return plugin::DWARFDie(nullptr, nullptr);
  return plugin::DWARFDie(
      const_cast<llvm::DWARFUnit *>(Referenced.getDwarfUnit()),
      const_cast<llvm::DWARFDebugInfoEntry *>(Referenced.getDebugInfoEntry()));
}

plugin::DWARFDie plugin::DWARFDie::getReferencedType() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  llvm::DWARFDie Referenced = Die.getAttributeValueAsReferencedDie(DW_AT_type);
  if (!Referenced)
    return plugin::DWARFDie(nullptr, nullptr);
  return plugin::DWARFDie(
      const_cast<llvm::DWARFUnit *>(Referenced.getDwarfUnit()),
      const_cast<llvm::DWARFDebugInfoEntry *>(Referenced.getDebugInfoEntry()));
}

bool plugin::DWARFDie::isDeclaration() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return !!Die.find(DW_AT_declaration);
}

bool plugin::DWARFDie::isSubrange() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_subrange_type;
}

bool plugin::DWARFDie::isUnspecified() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_unspecified_type;
}

bool plugin::DWARFDie::isVariable() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_variable;
}

bool plugin::DWARFDie::isFormalParameter() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_formal_parameter;
}

bool plugin::DWARFDie::isLexicalBlock() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == llvm::dwarf::DW_TAG_lexical_block;
}

bool plugin::DWARFDie::isInlinedSubroutine() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == llvm::dwarf::DW_TAG_inlined_subroutine;
}

plugin::DWARFDie::InlineInfo plugin::DWARFDie::getInlineInfo() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  std::optional<llvm::DWARFFormValue> I = Die.find(DW_AT_inline);
  if (!I || !I->getAsUnsignedConstant())
    return InlineInfo::Not_Inlined;
  return static_cast<InlineInfo>(*I->getAsUnsignedConstant());
}

bool plugin::DWARFDie::isArray() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_array_type;
}

bool plugin::DWARFDie::isSubroutine() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_subroutine_type;
}

bool plugin::DWARFDie::isConst() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_const_type;
}

bool plugin::DWARFDie::isVolatile() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_volatile_type;
}

bool plugin::DWARFDie::isRestrict() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_restrict_type;
}

bool plugin::DWARFDie::isPointer() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_pointer_type;
}

bool plugin::DWARFDie::isTypeDef() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_typedef;
}

bool plugin::DWARFDie::isC() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  std::optional<llvm::DWARFFormValue> Language = Die.find(DW_AT_language);
  if (!Language)
    return false;
  std::optional<uint64_t> Constant = Language->getAsUnsignedConstant();
  if (!Constant)
    return false;
  switch (*Constant) {
  case DW_LANG_C:
  case DW_LANG_C89:
  case DW_LANG_C99:
  case DW_LANG_C11:
    return true;
  default:
    return false;
  }
}

bool plugin::DWARFDie::isUsed() const { return true; }

bool plugin::DWARFDie::isCompileUnit() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_compile_unit;
}

std::vector<uint32_t> plugin::DWARFDie::getArrayCount() const {
  std::vector<uint32_t> Counts;
  if (!isArray())
    return Counts;
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  for (auto Child : Die.children()) {
    if (Child.getTag() != DW_TAG_subrange_type)
      continue;
    std::optional<uint64_t> Size = toUnsigned(Child.find(DW_AT_count));
    if (!Size)
      continue;
    Counts.push_back(*Size);
  }
  return Counts;
}

bool plugin::DWARFDie::getSize(uint64_t &Size, uint32_t PointerSize) const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return ::getSize(Die, Size, PointerSize);
}

bool plugin::DWARFDie::isStructureType() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_structure_type;
}

bool plugin::DWARFDie::isUnionType() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_union_type;
}

bool plugin::DWARFDie::isEnumerationType() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_enumeration_type;
}

bool plugin::DWARFDie::isMember() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  return Die.getTag() == DW_TAG_member;
}

bool plugin::DWARFDie::getMemberOffset(uint64_t &Offset) const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  if (std::optional<uint64_t> S =
          toUnsigned(Die.find(DW_AT_data_member_location))) {
    Offset = *S;
    return true;
  }
  return false;
}

bool plugin::DWARFDie::getByteSize(uint64_t &ByteSize) const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  if (std::optional<uint64_t> S = toUnsigned(Die.find(DW_AT_byte_size))) {
    ByteSize = *S;
    return true;
  }
  return false;
}

bool plugin::DWARFDie::getBitSize(uint64_t &BitSize) const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  if (std::optional<uint64_t> S = toUnsigned(Die.find(DW_AT_bit_size))) {
    BitSize = *S;
    return true;
  }
  return false;
}

bool plugin::DWARFDie::getBitOffset(uint64_t &Offset) const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  if (std::optional<uint64_t> S = toUnsigned(Die.find(DW_AT_bit_offset))) {
    Offset = *S;
    return true;
  }
  return false;
}

bool plugin::DWARFDie::getDataBitOffset(uint64_t &Offset) const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  if (std::optional<uint64_t> S = toUnsigned(Die.find(DW_AT_data_bit_offset))) {
    Offset = *S;
    return true;
  }
  return false;
}

bool plugin::DWARFDie::getSignedConstValue(int64_t &Value) const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  if (std::optional<int64_t> S = toSigned(Die.find(DW_AT_const_value))) {
    Value = *S;
    return true;
  }
  return false;
}

std::vector<plugin::DWARFDie> plugin::DWARFDie::getMembers() const {
  std::vector<plugin::DWARFDie> Members;
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  for (const llvm::DWARFDie &M : Die.children())
    if (M.getTag() == DW_TAG_member)
      Members.push_back(plugin::DWARFDie(
          const_cast<llvm::DWARFUnit *>(M.getDwarfUnit()),
          const_cast<llvm::DWARFDebugInfoEntry *>(M.getDebugInfoEntry())));
  return Members;
}

std::vector<plugin::DWARFDie> plugin::DWARFDie::getEnumerators() const {
  std::vector<plugin::DWARFDie> Enumerators;
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  for (const llvm::DWARFDie &M : Die.children())
    if (M.getTag() == DW_TAG_enumerator)
      Enumerators.push_back(plugin::DWARFDie(
          const_cast<llvm::DWARFUnit *>(M.getDwarfUnit()),
          const_cast<llvm::DWARFDebugInfoEntry *>(M.getDebugInfoEntry())));
  return Enumerators;
}

std::vector<plugin::DWARFDie> plugin::DWARFDie::getChildren() const {
  std::vector<plugin::DWARFDie> Children;
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  for (const llvm::DWARFDie &M : Die.children())
    Children.push_back(plugin::DWARFDie(
        const_cast<llvm::DWARFUnit *>(M.getDwarfUnit()),
        const_cast<llvm::DWARFDebugInfoEntry *>(M.getDebugInfoEntry())));
  return Children;
}

plugin::DWARFDie plugin::DWARFDie::getParent() const {
  llvm::DWARFDie Die(m_DWARFUnit, m_DWARFDebugInfoEntry);
  llvm::DWARFDie Parent = Die.getParent();
  return (plugin::DWARFDie(
      const_cast<llvm::DWARFUnit *>(Parent.getDwarfUnit()),
      const_cast<llvm::DWARFDebugInfoEntry *>(Parent.getDebugInfoEntry())));
}

//
//--------------------------DWARFAttribute----------------------------
//

plugin::DWARFAttribute::DWARFAttribute(const llvm::DWARFAttribute *Attribute)
    : m_DWARFAttribute(new llvm::DWARFAttribute()) {
  m_DWARFAttribute->Offset = Attribute->Offset;
  m_DWARFAttribute->ByteSize = Attribute->ByteSize;
  m_DWARFAttribute->Attr = Attribute->Attr;
  m_DWARFAttribute->Value = Attribute->Value;
}

plugin::DWARFAttribute::~DWARFAttribute() { delete m_DWARFAttribute; }

plugin::DWARFAttribute::operator bool() const {
  return m_DWARFAttribute->isValid();
}

const std::string plugin::DWARFAttribute::getAttributeName() const {
  return AttributeString(m_DWARFAttribute->Attr).str();
}

plugin::DWARFValue plugin::DWARFAttribute::getValue() const {
  return plugin::DWARFValue(&m_DWARFAttribute->Value);
}

//
//----------------------------DWARFValue------------------------------
//

plugin::DWARFValue::DWARFValue(const llvm::DWARFFormValue *V) : m_Value(V) {}

const std::string
plugin::DWARFValue::getValueAsString(const std::string Default) const {
  llvm::Expected<const char *> Value = m_Value->getAsCString();
  if (!Value) {
    consumeError(Value.takeError());
    return Default;
  }
  return *Value;
}
