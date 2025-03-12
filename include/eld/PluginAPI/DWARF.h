//===- DWARF.h-------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_PLUGINAPI_DWARF_H
#define ELD_PLUGINAPI_DWARF_H

#include "Defines.h"
#include <string>
#include <vector>

namespace llvm {
struct DWARFAttribute;
class DWARFContext;
class DWARFCompileUnit;
class DWARFDebugInfoEntry;
class DWARFFormValue;
class DWARFUnit;
namespace object {
class ObjectFile;
}
} // namespace llvm

namespace eld::plugin {
struct DLL_A_EXPORT DWARFAttribute;
struct DLL_A_EXPORT DWARFDie;
struct DLL_A_EXPORT DWARFInfo;
struct DLL_A_EXPORT DWARFUnit;
struct DLL_A_EXPORT DWARFValue;
struct DLL_A_EXPORT InputFile;

// A class that can parse information from debug sections in DWARF format
struct DLL_A_EXPORT DWARFInfo {

  explicit DWARFInfo(llvm::DWARFContext *DC);

  // FIXME: remove this constructor in favor of the above
  explicit DWARFInfo(InputFile IF);

  ~DWARFInfo();

  bool hasDWARFContext() const { return m_DWARFContext != nullptr; }

  ///
  /// Get the compile unit
  ///
  std::vector<plugin::DWARFUnit> getDWARFUnits() const;

  /// Conversion Operator
  operator bool() const { return m_DWARFContext != nullptr; }

private:
  llvm::DWARFContext *m_DWARFContext = nullptr;
  // FIXME: Remove these members
  bool ShouldDeleteDWARFContext = false;
  llvm::object::ObjectFile *m_ObjectFile = nullptr;
};

// A class that can parse DWARF Compile Unit
struct DLL_A_EXPORT DWARFUnit {
  explicit DWARFUnit(llvm::DWARFUnit *Unit);

  ///
  /// Get the DWARF die's
  ///
  std::vector<plugin::DWARFDie> getDIEs() const;

  /// Conversion Operator
  operator bool() const { return m_DWARFUnit != nullptr; }

  /// Is DWARF unit a compile unit ?
  bool isCompileUnit() const;

  /// Get the compile unit die.
  plugin::DWARFDie getCompileUnitDIE() const;

private:
  llvm::DWARFUnit *m_DWARFUnit = nullptr;
};

// A class that can parse DWARF Compile Unit
struct DLL_A_EXPORT DWARFDie {

  explicit DWARFDie(llvm::DWARFUnit *U, llvm::DWARFDebugInfoEntry *Entry);

  /// Conversion Operator
  operator bool() const {
    return m_DWARFDebugInfoEntry != nullptr && m_DWARFUnit != nullptr;
  }

  /// Returns true if DIE represents a subprogram (not inlined).
  bool isSubprogramDIE() const;

  ///  Dump the DWARF Die
  std::string toString() const;

  /// Return DW_AT_comp_dir as string or empty string if not present
  std::string getCompDir() const;

  /// Get Attributes for this DIE
  std::vector<DWARFAttribute> getAttributes() const;

  /// Get DW_AT_name as a string for this DIE
  const std::string getName() const;

  /// Get the DW_AT_decl_file as a string for this DIE or empty string if not
  /// present
  const std::string getDeclFile() const;

  /// Returns the declaration line (start line) for a DIE, assuming it specifies
  /// a subprogram.
  uint64_t getDeclLine() const;

  /// Returns the DIE offset for this DIE
  uint64_t getOffset() const;

  /// Returns true if this DIE denotes a type
  bool isType() const;

  /// Get the DWARF tag for this DIE as a string or "DW_TAG_null"
  const std::string getTagString() const;

  /// Get the DWARF tag as an unsigned value according to DWARF spec
  uint32_t getTagAsUnsigned() const;

  /// Retrieve high and low pc and return true if both values are
  /// present in this DIE
  bool getLowAndHighPC(uint64_t &LowPC, uint64_t &HighPC) const;

  /// is this DW_TAG_structure_type
  bool isStructureType() const;

  /// is this DW_TAG_union_type
  bool isUnionType() const;

  /// is this DW_TAG_enumeration_type
  bool isEnumerationType() const;

  /// Place DW_AT_data_member_location in Offset
  /// Return true an offset was found
  bool getMemberOffset(uint64_t &Offset) const;

  /// Place DW_AT_bit_offset in Offset
  /// Return true if DW_AT_bit_offset was found
  bool getBitOffset(uint64_t &Offset) const;

  /// Place DW_AT_data_bit_offset in Offset
  /// Return true if DW_AT_data_bit_offset was found
  bool getDataBitOffset(uint64_t &Offset) const;

  /// Place DW_AT_byte_size in Offset
  /// Return true if byte_size was found
  bool getByteSize(uint64_t &ByteSize) const;

  /// Place DW_AT_bit_size in BitSize
  /// Return true if bit_size was found
  bool getBitSize(uint64_t &BitSize) const;

  /// Place value of DW_AT_const_value interpreted as a signed constant in Value
  /// Return true if a DW_AT_const_value was found
  bool getSignedConstValue(int64_t &Value) const;

  /// Get a vector of child DIE's of this DIE that are DW_TAG_member
  /// i.e. get the members of a structure, class, or union
  std::vector<plugin::DWARFDie> getMembers() const;

  /// Get a vector of child DIE's of this DIE that are DW_TAG_enumerator
  /// i.e get the enumerators of an enum
  std::vector<plugin::DWARFDie> getEnumerators() const;

  /// Get the children of this DIE as a vector
  std::vector<plugin::DWARFDie> getChildren() const;

  /// Get the parnent DIE of this DIE or an invalid DIE if no parent
  plugin::DWARFDie getParent() const;

  /// is this DW_TAG_member?
  bool isMember() const;

  /// Returns the value for the second at_name or a empty string if not present
  const std::string getFingerPrint() const;

  /// If this DIE represents a type, populate Size with the size of the type and
  /// return true
  bool getSize(uint64_t &Size, uint32_t PointerSize) const;

  /// Return true if DW_AT_declaration is present in this DIE
  bool isDeclaration() const;

  /// Return true if DW_TAG_subrange_type is presnet in this DIE
  bool isSubrange() const;

  /// Return true if DW_TAG_unspecified_type is presnet in this DIE
  bool isUnspecified() const;

  /// Returns true if DW_TAG_variable is present in this DIE
  bool isVariable() const;

  /// Returns true if DW_TAG_formal_parameter is present in this DIE
  bool isFormalParameter() const;

  /// Returns true if DW_TAG_lexical_block is present in this DIE
  bool isLexicalBlock() const;

  /// Returns true if DW_TAG_inlined_subroutine is present in this DIE
  bool isInlinedSubroutine() const;

  enum InlineInfo {
    Not_Inlined = 0x0,
    Inlined = 0x1,
    Declared_Not_Inlined = 0x2,
    Declared_Inlined = 0x3,
  };

  /// Returns the value for DW_AT_inlined
  InlineInfo getInlineInfo() const;

  /// Returns true if DW_TAG_array_type is present in this DIE
  bool isArray() const;

  /// Returns true if DW_TAG_subroutine_type is present in this DIE
  bool isSubroutine() const;

  /// Returns true if DW_TAG_const_type is present in this DIE
  bool isConst() const;

  /// Returns true if DW_TAG_restrict_type is present in this DIE
  bool isRestrict() const;

  /// Returns true if DW_TAG_restrict_type is present in this DIE
  bool isVolatile() const;

  /// Returns true if DW_TAG_pointer is present in this DIE
  bool isPointer() const;

  /// Returns true if DW_TAG_typedef is present in this DIE
  bool isTypeDef() const;

  /// If this DIE represents a compile unit, return true if DW_AT_language
  /// specifies the C language otherwise return false
  bool isC() const;

  /// Return true if DW_AT_used is present
  bool isUsed() const;

  /// Return true if this DIE Is DW_TAG_compile_unit
  bool isCompileUnit() const;

  /// Returns the DIE referenced by DW_AT_type
  /// Returns a valid DIE if found else an invalid DIE
  plugin::DWARFDie getReferencedType() const;

  /// Returns the DIE referenced by DW_AT_abstract_origin
  /// Returns a valid DIE if found else an invalid DIE
  plugin::DWARFDie getReferencedOrigin() const;

  /// Return a vector of DIEs corresponding to inlined subroutines contained
  /// within this DIE if this DIE is a subprogram
  std::vector<plugin::DWARFDie> getInlines() const;

  /// If this is DW_TAG_array_type, returns a vector of each array size
  /// dimension according to DW_AT_count
  std::vector<uint32_t> getArrayCount() const;

  /// Return the product of each array dimension
  bool getTotalArraySize(uint64_t &Size) const;

private:
  llvm::DWARFDebugInfoEntry *m_DWARFDebugInfoEntry = nullptr;
  llvm::DWARFUnit *m_DWARFUnit = nullptr;
};

/// Represents DWARF Attributes
struct DLL_A_EXPORT DWARFAttribute {

  /// Create a DWARFAttribute from llvm::DWARFAttribute
  explicit DWARFAttribute(const llvm::DWARFAttribute *Attribute);

  DWARFAttribute(const DWARFAttribute &Other)
      : DWARFAttribute(Other.m_DWARFAttribute) {}

  /// Destroy this DWARFAttribute
  ~DWARFAttribute();

  /// Is this DWARFAttribute valid?
  explicit operator bool() const;

  /// Get the name of this DWARF attribute as a string
  const std::string getAttributeName() const;

  plugin::DWARFValue getValue() const;

private:
  llvm::DWARFAttribute *m_DWARFAttribute;
};

/// The associated value for an attribute
struct DLL_A_EXPORT DWARFValue {
  explicit DWARFValue(const llvm::DWARFFormValue *V);

  /// If the value for this DWARF attribute is a string, return it. Otherwise
  /// return the specified default value
  const std::string getValueAsString(const std::string Default = "") const;

private:
  const llvm::DWARFFormValue *m_Value;
};

} // namespace eld::plugin
#endif
