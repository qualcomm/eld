//===- Section.h-----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef ELD_READERS_SECTION_H
#define ELD_READERS_SECTION_H

#include "eld/Config/GeneralOptions.h"
#include "eld/Input/InputFile.h"
#include "eld/Object/SectionMap.h"
#include "llvm/ADT/StringRef.h"
#include <string>

namespace eld {

class InputFile;
class DiagnosticEngine;

class Section {
public:
  enum Kind : uint8_t {
    Bitcode,
    CommonELF,
    EhFrame,
    EhFrameHdr,
    ELF,
  };

  Section(Kind k, const std::string &name, uint64_t size)
      : m_SectionKind(k), m_Name(name), m_Size(size), m_InputFile(nullptr) {
    m_sectionNameHash = llvm::hash_combine(name);
  }

  llvm::StringRef name() const { return m_Name.data(); }

  virtual std::string getDecoratedName(const GeneralOptions &) const {
    return m_Name;
  }

  virtual uint64_t size() { return m_Size; }

  virtual uint64_t size() const { return m_Size; }

  virtual void setSize(uint64_t size) { m_Size = size; }

  virtual ~Section() {}

  // Helper functions to find Section Kind.
  bool isELF() const {
    bool iself =
        m_SectionKind == Kind::CommonELF || m_SectionKind == Kind::EhFrame ||
        m_SectionKind == Kind::EhFrameHdr || m_SectionKind == Kind::ELF;
    return iself;
  }
  bool isBitcode() const { return m_SectionKind == Kind::Bitcode; }

  void setName(llvm::StringRef name) {
    m_Name = name.str();
    m_sectionNameHash = llvm::hash_combine(name);
  }

  void setInputFile(InputFile *I) { m_InputFile = I; }

  InputFile *getInputFile() const { return m_InputFile; }

  virtual bool hasOldInputFile() const { return false; }

  virtual InputFile *getOldInputFile() const { return nullptr; }

  InputFile *originalInput() const {
    if (hasOldInputFile())
      return getOldInputFile();
    return getInputFile();
  }

  virtual uint32_t getAddrAlign() const { return 0; }

  // ----------------- Speedup Linkerscript processing -----------------------
  void setOutputSection(OutputSectionEntry *output) {
    m_OutputSection = output;
  }

  virtual void setMatchedLinkerScriptRule(RuleContainer *input) {
    m_InputSection = input;
  }

  OutputSectionEntry *getOutputSection() const { return m_OutputSection; }

  RuleContainer *getMatchedLinkerScriptRule() const { return m_InputSection; }

  uint64_t sectionNameHash() const { return m_sectionNameHash; }

  virtual uint64_t getSectionHash() const {
    return llvm::hash_combine(m_Name, m_SectionKind,
                              m_InputFile->getInput()->decoratedPath());
  }

  Kind getSectionKind() const { return m_SectionKind; }

  //--------------------------gnu.linkonce signature ---------------------------
  llvm::StringRef getSignatureForLinkOnceSection() const;

protected:
  const Kind m_SectionKind;
  /// FIXME: this should be a StringRef
  std::string m_Name;
  /// Can this be removed entirely or made 32 bit?
  uint64_t m_Size;
  /// FIXME: Next four fields are only relevant to input sections
  InputFile *m_InputFile;
  uint64_t m_sectionNameHash = 0;
  OutputSectionEntry *m_OutputSection = nullptr;
  RuleContainer *m_InputSection = nullptr;
};
} // namespace eld

#endif
