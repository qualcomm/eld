//===- ObjectReader.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_OBJECTREADER_H
#define ELD_READERS_OBJECTREADER_H
#include "eld/Readers/Section.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"

namespace eld {

class Module;
class InputFile;

/** \class ObjectReader
 *  \brief ObjectReader provides an common interface for different object
 *  formats.
 */
class ObjectReader {
protected:
  ObjectReader() {}

public:
  // Group Signature Info.
  class GroupSignatureInfo {
  public:
    GroupSignatureInfo(ResolveInfo *R, ELFSection *S)
        : m_Info(R), m_Section(S) {}

    void setSection(ELFSection *S) { m_Section = S; }

    void setInfo(ResolveInfo *R) { m_Info = R; }

    ELFSection *getSection() const { return m_Section; }

    ResolveInfo *getInfo() const { return m_Info; }

  private:
    ResolveInfo *m_Info;
    ELFSection *m_Section;
  };
  virtual ~ObjectReader() {}

  virtual bool readHeader(InputFile &pFile, bool isPostLTOPhase = false) = 0;

  virtual bool readSymbols(InputFile &pFile, bool isPostLTOPhase = false) = 0;

  virtual bool readSections(InputFile &pFile, bool isPostLTOPhase = false) = 0;

  virtual bool readGroup(InputFile &pFile, bool isPostLTOPhase) { return true; }

  /// readRelocations - read relocation sections
  ///
  /// This function should be called after symbol resolution.
  virtual bool readRelocations(InputFile &pFile) = 0;
};

} // namespace eld

#endif
