//===- TargetInfo.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_TARGETINFO_H
#define ELD_TARGET_TARGETINFO_H
#include "eld/Config/LinkerConfig.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/TargetParser/Triple.h"

namespace eld {

class Module;
class InputFile;
class Relocation;

/** \class TargetInfo
 *  \brief TargetInfo records ELF-dependent and target-dependnet data fields
 */
class TargetInfo {
public:
  struct TargetRelocationType {
    Relocation::Type CopyRelocType;
  };

  TargetInfo(LinkerConfig &m_Config);

  virtual ~TargetInfo() {}

  virtual void initializeAttributes(InputBuilder &builder) { return; }

  virtual bool InitializeDefaultMappings(Module &pModule);

  /// ELFVersion - the value of e_ident[EI_VERSION]
  virtual uint8_t ELFVersion() const { return llvm::ELF::EV_CURRENT; }

  /// The return value of machine() it the same as e_machine in the ELF header
  virtual uint32_t machine() const = 0;

  // Return the string representation of the machine.
  virtual std::string getMachineStr() const = 0;

  /// OSABI - the value of e_ident[EI_OSABI]
  virtual uint8_t OSABI() const;

  /// ABIVersion - the value of e_ident[EI_ABIVRESION]
  virtual uint8_t ABIVersion() const { return 0x0; }

  virtual uint64_t startAddr(bool linkerScriptHasSectionsCommand,
                             bool isDynExec, bool loadPhdr) const = 0;

  virtual bool checkFlags(uint64_t flags, const InputFile *pInput) const;

  /// flags - the value of ElfXX_Ehdr::e_flags
  virtual uint64_t flags() const = 0;

  virtual std::string flagString(uint64_t pFlag) const;

  /// entry - the symbol name of the entry point
  virtual const char *entry() const { return "_start"; }

  /// dyld - the name of the default dynamic linker
  /// target may override this function if needed.
  /// @ref gnu ld, bfd/elf32-i386.c:521
  virtual const char *dyld() const { return "/usr/lib/libc.so.1"; }

  /// commonPageSize - the common page size of the target machine, and we set it
  /// to 4K here. If target favors the different size, please override this
  virtual uint64_t commonPageSize() const { return 0x1000; }

  /// abiPageSize - the abi page size of the target machine, and we set it to 4K
  /// here. If target favors the different size, please override this function
  virtual uint64_t abiPageSize(bool linkerScriptHasSectionsCommand) const {
    return 0x1000;
  }

  /// needEhdr - load ELF header into memory
  virtual bool needEhdr(Module &pModule, bool linkerScriptHasSectionsCommand,
                        bool isPhdr) {
    return isPhdr & false;
  }

  /// processNoteGNUSTACK - target will output any GNUSTACK segment based
  /// on .note.GNU-stack or not
  virtual bool processNoteGNUSTACK() {
    return !m_Config.options().noGnuStack();
  }

  virtual int32_t cmdLineFlag() const { return 0; }

  virtual int32_t outputFlag() const { return 0; }

  virtual llvm::StringRef getOutputMCPU() const;

  virtual bool initialize() { return true; }

  uint8_t ELFClass() const;

  TargetRelocationType getTargetRelocationType() const;

protected:
  LinkerConfig &m_Config;

private:
  TargetRelocationType relocType;
};

} // namespace eld

#endif
