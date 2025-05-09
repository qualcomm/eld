//===- BitcodeReader.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/BitcodeReader.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/LayoutMap/TextLayoutPrinter.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Object/SectionMap.h"
#include "eld/PluginAPI/LinkerPlugin.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/NamePool.h"
#include "eld/SymbolResolver/SymbolResolutionInfo.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/Magic.h"
#include "llvm/LTO/LTO.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/xxhash.h"
#include <cassert>
#include <string>
#include <utility>

using namespace eld;
using namespace llvm;

static ResolveInfo::Type getSymType(const llvm::lto::InputFile::Symbol &Sym) {
  if (Sym.isExecutable())
    return ResolveInfo::Function;
  // Handle TLS symbols.
  if (Sym.isTLS())
    return ResolveInfo::ThreadLocal;
  return ResolveInfo::Object;
}

static ResolveInfo::Desc getSymDesc(const llvm::lto::InputFile::Symbol &Sym) {
  if (Sym.isUndefined())
    return ResolveInfo::Undefined;
  else if (Sym.isCommon())
    return ResolveInfo::Common;

  return ResolveInfo::Define;
}

static ResolveInfo::Binding
getSymBinding(const llvm::lto::InputFile::Symbol &Sym) {
  if (Sym.isWeak())
    return ResolveInfo::Weak;
  return ResolveInfo::Global;
}

static ResolveInfo::Visibility
getSymVisibility(const llvm::lto::InputFile::Symbol &Sym) {
  llvm::GlobalValue::VisibilityTypes Vis = Sym.getVisibility();

  switch (Vis) {
  case llvm::GlobalValue::DefaultVisibility:
    return ResolveInfo::Default;
  case llvm::GlobalValue::HiddenVisibility:
    return ResolveInfo::Hidden;
  case llvm::GlobalValue::ProtectedVisibility:
    return ResolveInfo::Protected;
  }
}

static size_t getSymSize(const llvm::lto::InputFile::Symbol &Sym) {
  if (!Sym.isCommon())
    return 0;

  return Sym.getCommonSize();
}

/// BitcodeReader

/// constructor
BitcodeReader::BitcodeReader(GNULDBackend &pBackend, eld::IRBuilder &pBuilder,
                             LinkerConfig &pConfig)
    : m_Backend(pBackend), m_Builder(pBuilder), m_Config(pConfig),
      m_TraceLTO(false) {
  m_TraceLTO = m_Config.options().traceLTO();
}

BitcodeReader::~BitcodeReader() {}

bool BitcodeReader::readInput(InputFile &InputFile,
                              plugin::LinkerPlugin *LTOPlugin) {
  LayoutInfo *layoutInfo = m_Builder.getModule().getLayoutInfo();
  if (layoutInfo) {
    std::string action =
        "LOAD " + InputFile.getInput()->decoratedPath() + " [Bitcode]";
    layoutInfo->recordInputActions(LayoutInfo::Load, InputFile.getInput());
  }

  if (m_Builder.getModule().getPrinter()->traceFiles())
    m_Config.raise(Diag::trace_file) << InputFile.getInput()->decoratedPath();

  BitcodeFile *BitcodeFile = llvm::dyn_cast<eld::BitcodeFile>(&InputFile);
  assert(BitcodeFile);

  Input *I = InputFile.getInput();

  /// Get a human-readable descriptor for this file. This is either the
  /// filename or the archive name, offset, and filename (if input is part of
  /// an archive).
  std::string PathWithArchiveOffset = I->getResolvedPath().native();
  std::string NameWithArchiveOffset = I->getResolvedPath().filename().native();

  if (ArchiveMemberInput *AMI = llvm::dyn_cast<eld::ArchiveMemberInput>(I)) {
    // Archive (must include offset because there could be multiple files
    // with the same name in the archive)
    NameWithArchiveOffset += (":" + llvm::Twine(AMI->getChildOffset()) + "(" +
                              llvm::Twine(AMI->getMemberName()) + ")")
                                 .str();
    PathWithArchiveOffset +=
        (llvm::Twine('\0') + llvm::Twine(AMI->getChildOffset())).str();
  }

  // Construct namespace ID for this file. We set the path (ModuleID) to
  // this, so we can later identify locals coming from this file. If this is
  // ThinLTO, we use the Module Hash instead since it is stable for files
  // inside archives.
  std::optional<uint64_t> PluginLTOHash;
  if (LTOPlugin) {
    auto PluginLTOHashExp = LTOPlugin->OverrideLTOModuleHash(
        plugin::BitcodeFile(*BitcodeFile), PathWithArchiveOffset);
    if (!m_Config.getDiagEngine()->diagnose())
      return false;
    if (PluginLTOHashExp)
      PluginLTOHash = *PluginLTOHashExp;
  }

  // TODO: xxHash64 used previously, for non-archive members.
  uint64_t ModuleHash =
      PluginLTOHash
          ? *PluginLTOHash
          : (uint64_t)Input::computeFilePathHash(PathWithArchiveOffset);

  // Only the latter part (hash) is actually used for namespace identification.
  // The front is just for user-friendly debugging.
  std::string ModuleID =
      (Twine(NameWithArchiveOffset) + "^^" + Twine::utohexstr(ModuleHash))
          .str();

  if (!BitcodeFile->createLTOInputFile(ModuleID))
    return false;

  if (LTOPlugin) {
    if (!BitcodeFile->createPluginModule(*LTOPlugin, ModuleHash))
      return false;
  }

  llvm::StringMap<std::vector<LDSymbol *>> entrySectionsToSymbols;

  // TODO: If the comdat table comes from normal LTO, does it only include
  // globals?
  const auto &signatureList = BitcodeFile->getInputFile().getComdatTable();
  auto &signatureMap = m_Backend.getModule().signatureMap();
  for (int i = 0; i < (int)signatureList.size(); i++) {
    auto signature = Saver.save(signatureList[i].first);
    auto info = signatureMap.find(signature);
    bool GroupRejected =
        info != signatureMap.end() &&
        (*info).second->getInfo()->resolvedOrigin() != &InputFile;

    BitcodeFile->addKeptComdat(i, !GroupRejected);
    if (!GroupRejected) {
      // Set this file as the origin of this group and add symbol
      ResolveInfo *info = make<ResolveInfo>(signature);
      info->setResolvedOrigin(&InputFile);
      signatureMap[signature] =
          make<ObjectReader::GroupSignatureInfo>(info, nullptr);
    }
  }

  // If there is a plugin corresponding to the input file, delegate the reading
  // to the plugin.
  if (LTOPlugin) {
    LTOPlugin->ReadSymbols(*BitcodeFile->getPluginModule());
    if (!m_Config.getDiagEngine()->diagnose())
      return false;
  } else {
    // Read global symbols from input file.
    unsigned Idx = 0;
    for (const auto &Sym : BitcodeFile->getInputFile().symbols()) {
      StringRef symbol = Sym.getName();
      bool keptComdat = BitcodeFile->findIfKeptComdat(Sym.getComdatIndex());
      LDSymbol *sym = m_Backend.getModule().addSymbolFromBitCode(
          *BitcodeFile, symbol.str(), getSymType(Sym),
          keptComdat ? getSymDesc(Sym) : ResolveInfo::Undefined,
          getSymBinding(Sym), getSymSize(Sym), getSymVisibility(Sym), Idx++);

      if (!sym)
        return false;
    }
  }

  // TODO: Assume locals cannot be wrapped so the code that reads locals is
  // moved later.
  for (auto &wrapper : m_Config.options().renameMap()) {
    llvm::StringRef name = wrapper.first();
    if (!m_Builder.getModule().hasWrapReference(name) ||
        name.starts_with("__real_"))
      continue;
    LDSymbol *sym = m_Builder.getModule().getNamePool().findSymbol(name.str());
    if (sym && !sym->resolveInfo()->isBitCode())
      continue;
    std::string wrapName = std::string("__wrap_") + name.str();
    if (m_Config.getPrinter()->traceWrapSymbols())
      m_Config.raise(Diag::insert_wrapper) << wrapName;
    Resolver::Result result;
    m_Builder.getModule().getNamePool().insertSymbol(
        &InputFile, wrapName, false, eld::ResolveInfo::NoType,
        eld::ResolveInfo::Undefined, eld::ResolveInfo::Global, 0, 0,
        eld::ResolveInfo::Default, nullptr, result, false, false, 0,
        false /* isPatchable */, m_Builder.getModule().getPrinter());
    sym = make<LDSymbol>(result.Info, false);
    if (result.Overriden || !result.Info->outSymbol())
      result.Info->setOutSymbol(sym);
  }

  return true;
}
