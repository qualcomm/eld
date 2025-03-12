//===- ELFDynObjectFile.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_INPUT_ELFDYNOBJECTFILE_H
#define ELD_INPUT_ELFDYNOBJECTFILE_H

#include "eld/Input/ELFFileBase.h"
#include "eld/Input/Input.h"

namespace eld {

/** \class ELFDynObjFile
 *  \brief InputFile represents a dynamic shared library.
 */
class ELFDynObjectFile : public ELFFileBase {
public:
  ELFDynObjectFile(Input *I, DiagnosticEngine *diagEngine);

  /// Casting support.
  static bool classof(const InputFile *I) {
    return (I->getKind() == InputFile::ELFDynObjFileKind);
  }

  std::string getSOName() const { return getInput()->getName(); }

  void setSOName(std::string SOName) { getInput()->setName(SOName); }

  ELFSection *getDynSym() const { return m_SymbolTable; }

  bool isELFNeeded() override;

  virtual ~ELFDynObjectFile() {}

private:
  std::vector<ELFSection *> m_Sections;
};

} // namespace eld

#endif // ELD_INPUT_ELFDYNOBJECTFILE_H
