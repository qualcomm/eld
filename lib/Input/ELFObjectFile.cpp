//===- ELFObjectFile.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/ELFObjectFile.h"
#include "ELDDWARFObject.h"
#include "eld/Diagnostics/Diagnostic.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/Support/Compression.h"

using namespace eld;

ELFObjectFile::ELFObjectFile(Input *I, DiagnosticEngine *DiagEngine)
    : ELFFileBase(I, DiagEngine, InputFile::ELFObjFileKind) {
  if (I->getSize())
    Contents = I->getFileContents();
}

void ELFObjectFile::createDWARFContext() {
  if (!getInput() || DWARFContextCreationAttempted)
    return;
  DWARFContextCreationAttempted = true;

  // LLVM can create an empty context for an object without debug data, so
  // check for debug sections before creating the context.
  bool HasDebugSections = false;
  bool HasGNUCompressedDebugSections = false;
  for (const Section *S : getSections()) {
    const auto *ELFS = llvm::dyn_cast_or_null<ELFSection>(S);
    if (!ELFS)
      continue;
    llvm::StringRef SectionName = ELFS->name();
    if (SectionName.starts_with(".zdebug_"))
      HasGNUCompressedDebugSections = true;
    if (SectionName.starts_with(".debug") ||
        SectionName.starts_with(".zdebug_")) {
      HasDebugSections = true;
    }
  }
  if (!HasDebugSections)
    return;
  if (HasGNUCompressedDebugSections &&
      !llvm::compression::zlib::isAvailable()) {
    if (DiagnosticEngine *DiagEngine = getDiagEngine())
      DiagEngine->raise(Diag::verbose_dwarf_object_unavailable)
          << getInput()->getName()
          << "zlib support is unavailable for GNU compressed debug sections";
    return;
  }

  eld::Expected<std::unique_ptr<const llvm::DWARFObject>> DWARFObjectOrError =
      detail::createELDDWARFObject(*this);
  if (!DWARFObjectOrError) {
    if (DiagnosticEngine *DiagEngine = getDiagEngine())
      DiagEngine->raiseDiagEntry(std::move(DWARFObjectOrError.error()));
    return;
  }

  DWARFContext =
      std::make_unique<llvm::DWARFContext>(std::move(*DWARFObjectOrError));
}

void ELFObjectFile::addSectionAnnotation(const ELFSection &S,
                                        const std::string &Annotation) {
  SectionAnnotationsBySection[&S].push_back(Annotation);
}

bool ELFObjectFile::hasSectionAnnotations(const ELFSection &S) const {
  auto It = SectionAnnotationsBySection.find(&S);
  return It != SectionAnnotationsBySection.end() && !It->second.empty();
}

llvm::ArrayRef<std::string>
ELFObjectFile::getSectionAnnotations(const ELFSection &S) const {
  auto It = SectionAnnotationsBySection.find(&S);
  if (It == SectionAnnotationsBySection.end())
    return {};
  return It->second;
}
