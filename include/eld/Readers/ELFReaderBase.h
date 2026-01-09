//===- ELFReaderBase.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_ELFREADERBASE_H
#define ELD_READERS_ELFREADERBASE_H
#include "eld/PluginAPI/Expected.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include <memory>

namespace eld {
class InputFile;
class Module;

/// ELFReaderBase provides low-level functions to parse/process input ELF
/// files.
///
/// This class only provides low-level functions to parse/process components of
/// input ELF files. The actual parsing must be driven by reader driver
/// classes such as ELFDynObjParser and NewRelocatableObjReader.
///
/// Each instance of the object can only be used to read one input file.
///
/// ELFReaderBase propagates all the errors to the caller function using
/// eld::Expected.
class ELFReaderBase {
public:
  explicit ELFReaderBase(Module &module, InputFile &inputFile);

  // defaulted copy-constructor.
  ELFReaderBase(const ELFReaderBase &other) = default;

  virtual ~ELFReaderBase() {}

  /// Creates refined section headers by reading raw section headers.
  ///
  /// This function also adds references to all the created refined section
  /// headers in the input file.
  virtual eld::Expected<bool> readSectionHeaders() = 0;

  // Return e_machine from ELF header
  virtual uint16_t getMachine() const = 0;

  /// Creates refined symbols by reading raw symbols.
  ///
  /// This function also adds references to all the created refined symbols in
  /// the input file.
  virtual eld::Expected<bool> readSymbols() = 0;

  /// Checks that the flags present in e_flags member of ELF header are valid
  /// as per the targets.
  virtual eld::Expected<bool> checkFlags() const = 0;

  /// Get a textual representation of flags present in e_flags member of ELF
  /// header.
  virtual std::string getFlagString() const = 0;

  /// Set symbol alias information for dynamic objects.
  ///
  /// \note This function must only be used by dynamic object files.
  virtual void setSymbolsAliasInfo();

  /// Returns the corresponding input file for the reader.
  InputFile *getInputFile() { return &m_InputFile; }

  /// Record input actions if layout printer is available.
  virtual void recordInputActions() const = 0;

  /// Checks that the below values are valid as per the target configuruation:
  /// - flags (e_flags)
  /// - machine (e_machine)
  /// - class (e_ident[EI_CLASS])
  virtual eld::Expected<bool> isCompatible() const = 0;

  /// Returns the group signature.
  virtual eld::Expected<ResolveInfo *>
  computeGroupSignature(const ELFSection *S) const;

  /// Returns section indices of the group members.
  virtual eld::Expected<std::vector<uint32_t>>
  getGroupMemberIndices(const ELFSection *S) const;

  /// Returns the group flag.
  virtual eld::Expected<uint32_t> getGroupFlag(const ELFSection *S) const;

  /// Reads compressed section.
  virtual eld::Expected<bool> readCompressedSection(ELFSection *S) = 0;

  /// Reads merged string section.
  virtual eld::Expected<bool> readMergeStringSection(ELFSection *S) = 0;

  /// Creates and returns an instance of a reader class.
  ///
  /// The reader class depends on the input type and the target
  /// configuration. For example, if an input is a dyanmic input library
  /// and the target configuration is little-endian 32-bits architecture,
  /// then the reader class would be
  /// DynamicELFReader<llvm::object::ELF32LE>.
  static eld::Expected<std::unique_ptr<ELFReaderBase>>
  Create(Module &module, InputFile &inputFile);

  /// Sets the correct origin for post LTO common symbols.
  virtual void setPostLTOCommonSymbolsOrigin() const;

  /// Reads one group section.
  virtual eld::Expected<bool> readOneGroup(ELFSection *S);

  virtual eld::Expected<bool> readRelocationSection(ELFSection *RS);

  virtual eld::Expected<void> readSections();

  /// Returns the symbol type.
  static ResolveInfo::Type getSymbolType(uint8_t info, uint32_t shndx);

  /// Returns the symbol description.
  static ResolveInfo::Desc getSymbolDesc(const GNULDBackend &backend,
                                         uint32_t shndx, uint8_t binding,
                                         InputFile &inputFile, bool isOrdinary);

  /// Returns the symbol binding information.
  static ResolveInfo::Binding getSymbolBinding(uint8_t binding, uint32_t shndx,
                                               ELFFileBase &EFileBase);

  static eld::Expected<ObjectFile::ELFKind> inspectELFKind(const InputFile &I);

protected:
  /// Checks that the machine value (e_machine) is valid as per the target
  /// configuration.
  virtual bool checkMachine() const = 0;

  /// Checks that the ELF class value (e_ident[EI_CLASS]) is valid as per the
  /// target configuration.
  virtual bool checkClass() const = 0;

  /// Checks that the ELF OS/ABI value (e_ident[EI_OSABI]) is compatible as per
  /// the target configuration.
  virtual void checkOSABI() const = 0;

protected:
  /// Creates and returns an instance of a reader class.
  template <class ELFT>
  static eld::Expected<std::unique_ptr<ELFReaderBase>>
  createImpl(Module &module, InputFile &inputFile);

protected:
  Module &m_Module;
  InputFile &m_InputFile;
};
} // namespace eld

#endif
