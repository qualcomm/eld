//===- ScriptMemoryRegion.h------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_OBJECT_SCRIPTMEMORYREGION_H
#define ELD_OBJECT_SCRIPTMEMORYREGION_H

#include "eld/PluginAPI/Expected.h"
#include "eld/Script/MemoryDesc.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

namespace eld {

class ELFSection;
class OutputSectionEntry;
class MemoryDesc;

class ScriptMemoryRegion {
public:
  enum MemoryAttributes {
    NoAttributes = 0,
    Write = 0x1,
    Alloc = 0x2,
    Execute = 0x4,
    Progbits = 0x8
  };
  explicit ScriptMemoryRegion(MemoryDesc *MemoryDesc);

  void addOutputSectionVMA(const OutputSectionEntry *O);

  void addOutputSectionLMA(const OutputSectionEntry *O);

  eld::Expected<void> verifyMemoryUsage(LinkerConfig &Config);

  eld::Expected<uint64_t> getOrigin() const;

  eld::Expected<uint64_t> getLength() const;

  uint64_t getAddr();

  void dumpMap(llvm::raw_ostream &O);

  void dumpMemoryUsage(llvm::raw_ostream &O);

  const MemoryDesc *getMemoryDesc() const { return MMemoryDesc; }

  void clearMemoryRegion() {
    MOutputSections.clear();
    CurrentCursor.reset();
    FirstOutputSectionExceededLimit = nullptr;
  }

  bool
  checkCompatibilityAndAssignMemorySpecToOutputSection(OutputSectionEntry *O);

  eld::Expected<void> parseMemoryAttributes();

  // --print-memory-usage
  eld::Expected<void> printMemoryUsage(llvm::raw_ostream &OS) const;

  // Print the header for dumping memory usage information
  static void printHeaderForMemoryUsage(llvm::raw_ostream &OS);

  // Retrieve the name of the memory region
  std::string getName() const;

  // Check if the output section contains a VMA address specified by the
  // user for any override. If the VMA is within the memory region it will
  // be included, if not it will not be part of the image layout but not
  // counted against the memory region
  bool containsVMA(uint64_t Addr) const;

private:
  size_t getSize() const;

  bool isCompatible(const ELFSection *S) const;

  void addOutputSection(const OutputSectionEntry *O);

private:
  MemoryDesc *MMemoryDesc = nullptr;
  llvm::SmallVector<const OutputSectionEntry *, 4> MOutputSections;
  uint32_t AttrFlags = 0;
  uint32_t AttrNegFlags = 0;
  uint32_t AttrInvertedFlags = 0;
  uint32_t AttrInvertedNegFlags = 0;
  std::optional<uint64_t> CurrentCursor;
  const OutputSectionEntry *FirstOutputSectionExceededLimit = nullptr;
};

} // end namespace eld

#endif
