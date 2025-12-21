//===- ELFReader.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_NEWREADERS_NEWELFREADER_H
#define ELD_NEWREADERS_NEWELFREADER_H
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/Readers/ELFReaderBase.h"
#include "eld/Readers/Relocation.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Object/ELF.h"
#include "llvm/Object/ELFTypes.h"
#include <optional>
#include <type_traits>

/// If val contains a LLVM error, then return diagnostic entry created from the
/// corresponding llvm::Error. Otherwise, assign the correponding value from val
/// to var.
#define LLVMEXP_EXTRACT_AND_CHECK(var, val)                                    \
  {                                                                            \
    auto exp = val;                                                            \
    LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(exp)                                     \
    var = exp.get();                                                           \
  }

namespace eld {
class ELFSection;
class InputFile;
class LDSymbol;
class LinkerConfig;

/// ELFReader provides low-level functions to read input ELF files.
/// It internally utilizes llvm::object::ELFFile to parse the elf input files.
///
/// Each instance of the object can only be used to read one input
/// file.
///
/// ELFReader propagates all the errors to the caller function using
/// eld::Expected.
template <class ELFT> class ELFReader : public ELFReaderBase {
public:
  LLVM_ELF_IMPORT_TYPES_ELFT(ELFT)
  using intX_t = std::conditional_t<ELFT::Is64Bits, int64_t, int32_t>;
  typedef std::vector<LDSymbol *> SymbolList;
  typedef const llvm::DenseMap<uint64_t, SymbolList> AliasMap;
  typedef llvm::DenseMap<uint64_t, LDSymbol *> GlobalSymbolMap;

  template <bool isRela>
  using RelType = std::conditional_t<isRela, typename ELFReader<ELFT>::Elf_Rela,
                                     typename ELFReader<ELFT>::Elf_Rel>;

  template <bool isRela>
  using RelRangeType =
      std::conditional_t<isRela, typename ELFReader<ELFT>::Elf_Rela_Range,
                         typename ELFReader<ELFT>::Elf_Rel_Range>;

  // defaulted copy constructor.
  ELFReader(const ELFReader &) = default;
  // defaulted move constructor.
  ELFReader(ELFReader &&) noexcept = default;

  // delete assignment operators.
  ELFReader &operator=(const ELFReader &) = delete;
  ELFReader &operator=(ELFReader &&) = delete;

  /// Return the e_machine field stored in the ELF header
  uint16_t getMachine() const override;

  /// Creates refined symbols by reading raw symbols.
  ///
  /// This function also adds references to all the created refined symbols in
  /// the input file.
  eld::Expected<bool> readSymbols() override;

  /// Checks that the flags present in e_flags member of ELF header are valid
  /// as per the targets.
  eld::Expected<bool> checkFlags() const override;

  /// Get a textual representation of flags present in e_flags member of ELF
  /// header.
  std::string getFlagString() const override;

  /// Checks that the below values are valid as per the target configuruation:
  /// - flags (e_flags)
  /// - machine (e_machine)
  /// - class (e_ident[EI_CLASS])
  eld::Expected<bool> isCompatible() const override;

  /// Record input actions if layout printer is available.
  void recordInputActions() const override;

protected:
  explicit ELFReader(Module &module, InputFile &inputFile,
                     plugin::DiagnosticEntry &diagEntry);

  // Check if the file contains executable sections for
  // compatibility checks on some ABI platforms
  bool hasExecutableSections() const;

  /// Creates and returns an instance of llvm::object::ELFFile<ELFT> for the
  /// inputFile.
  static std::optional<llvm::object::ELFFile<ELFT>>
  createLLVMELFFile(LinkerConfig &config, const InputFile &inputFile,
                    plugin::DiagnosticEntry &diagEntry);

  /// Return first section of type 'type' in the array 'sections'.
  const Elf_Shdr *findSection(llvm::ArrayRef<Elf_Shdr> sections, uint32_t type);

  /// Computes and returns the section name.
  eld::Expected<std::string> getSectionName(Elf_Shdr rawSectHdr);

  /// Computes and return the section kind.
  LDFileFormat::Kind getSectionKind(Elf_Shdr rawSectHdr,
                                    llvm::StringRef sectionName);

  /// Create and return eld::ELFSection from the raw section header.
  virtual eld::Expected<ELFSection *> createSection(Elf_Shdr rawSectHdr) = 0;

  /// Create and return eld::LDSymbol from the raw symbol.
  eld::Expected<LDSymbol *> createSymbol(llvm::StringRef stringTable,
                                         Elf_Sym rawSym, std::size_t idx,
                                         bool isPatchable);

  /// Set attributes to the section S.
  ///
  /// The following attributes are set to the section S:
  /// - sh_addralign
  /// - sh_size
  /// - sh_offset
  /// - sh_info
  /// - input file
  void setSectionAttributes(ELFSection *S, Elf_Shdr rawSectHdr);

  /// Input file stores references to special sections such as
  /// symbol table, string table etc. This function stores
  /// references to such sections in the input file.
  ///
  /// The complete list of such sections are:
  /// - SHT_DYNAMIC
  /// - SHT_DYNSYM
  /// - SHT_SYMTAB
  /// - SHT_SYMTAB_SHNDX
  /// - SHT_STRTAB
  virtual void setSectionInInputFile(ELFSection *S, Elf_Shdr rawSectHdr);

  /// Set link and info attributes to the sections.
  bool setLinkInfoAttributes();

  /// Finds and returns a symbol's index from the extended symbol table.
  eld::Expected<uint32_t> getExtendedSymTabIdx(Elf_Sym rawSym, uint32_t symIdx);

  // FIXME: Move this to DynamicELFReader
  /// Process symbol aliases and report them to the user
  void processAndReportSymbolAliases(
      const AliasMap &DynObjAliasSymbolMap,
      const GlobalSymbolMap &DynObjGlobalSymbolMap) const;

  eld::Expected<uint32_t> computeShndx(const Elf_Sym &rawSym, std::size_t idx);

  virtual eld::Expected<llvm::StringRef>
  computeSymbolName(const Elf_Sym &rawSym, const ELFSection *S,
                    llvm::StringRef stringTable, uint32_t st_shndx,
                    ResolveInfo::Type ldType) const;

  /// Checks that the machine value (e_machine) is valid as per the target
  /// configuration.
  bool checkMachine() const override;

  /// Checks that the ELF class value (e_ident[EI_CLASS]) is valid as per the
  /// target configuration.
  bool checkClass() const override;

  /// Checks that the ELF ABI value (e_ident[EI_OSABI]) is compatible as per the
  /// target configuration.
  void checkOSABI() const override;

  /// Verifies symbol.
  ///
  /// It implicitly reports non-error diagnostics. Error diagnostics
  /// are returned to the caller function.
  eld::Expected<bool> verifySymbol(const LDSymbol *sym);

  /// Reads compressed section.
  eld::Expected<bool> readCompressedSection(ELFSection *S) override;

  /// Reads merged string section.
  eld::Expected<bool> readMergeStringSection(ELFSection *S) override;

  /// Emit zero-sized fragment for non-zero sized symbol warning where
  /// appropriate.
  void checkAndMayBeReportZeroSizedSection(const LDSymbol *sym) const;

protected:
  const std::optional<llvm::object::ELFFile<ELFT>> m_LLVMELFFile;
  std::optional<llvm::ArrayRef<Elf_Shdr>> m_RawSectHdrs;

  /// Returns the explicit addend associated with the relocation.
  typename ELFReader<ELFT>::intX_t
  getAddend(const typename ELFReader<ELFT>::Elf_Rela &R) {
    return R.r_addend;
  }

  /// Returns the explicit addend associated with the relocation.
  typename ELFReader<ELFT>::intX_t
  getAddend(const typename ELFReader<ELFT>::Elf_Rel &R) {
    return 0;
  }

  /// Returns all relocation entries in rawSect.
  template <bool isRela>
  eld::Expected<RelRangeType<isRela>>
  getRelocations(const typename ELFReader<ELFT>::Elf_Shdr &rawSect) {
    if constexpr (isRela) {
      llvm::Expected<RelRangeType<isRela>> expRelRange =
          this->m_LLVMELFFile->relas(rawSect);
      LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expRelRange);
      return std::move(expRelRange.get());
    } else {
      llvm::Expected<RelRangeType<isRela>> expRelRange =
          this->m_LLVMELFFile->rels(rawSect);
      LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(expRelRange);
      return std::move(expRelRange.get());
    }
  }

  /// Returns relocation type.
  template <bool isRela>
  Relocation::Type getRelocationType(const RelType<isRela> &R) {
    using RelocationTypeCastType =
        std::conditional_t<ELFT::Is64Bits, uint32_t, unsigned char>;
    Relocation::Type rType =
        static_cast<RelocationTypeCastType>(R.getRInfo(/*isMips=*/false));
    return rType;
  }

  template <bool isRela>
  eld::Expected<bool> readRelocationSection(ELFSection *RS);
};
} // namespace eld

#endif
