//===- Input.cpp-----------------------------------------------------------===//
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

#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/InputFile.h"
#include "eld/Input/InputTree.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/Support/INIReader.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Support/FileSystem.h"
#include <filesystem>

using namespace eld;

static unsigned int order = 0;

llvm::StringRef Input::toString(Input::Type Type) {
  switch (Type) {
  case Archive:
    return "archive";
  case DynObj:
    return "dynamic object";
  case Script:
    return "linker script";
  case Namespec:
    return "namespec";
  case ArchiveMember:
    return "archive member";
  case Internal:
    return "internal";
  case Default:
    return "default";
  }
  llvm_unreachable("Input::Type out of range");
}

//===----------------------------------------------------------------------===//
// eld::Input
//===----------------------------------------------------------------------===//
Input::Input(std::string pName, DiagnosticEngine *diagEngine, Input::Type pType)
    : m_FileName(pName), m_pAttr(), m_InputOrdinal(order++),
      m_ResolvedPathHash(0), m_MemberNameHash(0), m_Type(pType),
      m_DiagEngine(diagEngine) {}

Input::Input(std::string pName, const Attribute &Attr,
             DiagnosticEngine *diagEngine, Input::Type pType)
    : m_FileName(pName), m_pAttr(Attr), m_InputOrdinal(order++),
      m_ResolvedPathHash(0), m_MemberNameHash(0), m_Type(pType),
      m_DiagEngine(diagEngine) {}

bool Input::resolvePathMappingFile(const LinkerConfig &pConfig) {
  m_ResolvedPath = eld::sys::fs::Path(pConfig.getFileFromHash(m_FileName));
  std::string resolvedPathStr = m_ResolvedPath->native();
  if (!isPathValid(m_FileName)) {
    return false;
  }
  m_ResolvedPathHash = computeFilePathHash(resolvedPathStr);
  m_MemberNameHash = computeFilePathHash(m_FileName);
  MemoryArea *inputMem =
      Input::getMemoryAreaForPath(m_FileName, pConfig.getDiagEngine());
  if (!inputMem)
    inputMem = createMemoryArea(m_FileName, pConfig.getDiagEngine());
  setMemArea(inputMem);
  // All queries to return the name of the Input return FileName for the main
  // driver.
  m_Name = m_FileName;
  return true;
}

bool Input::isPathValid(const std::string &Path) const {
  if (llvm::sys::fs::is_directory(Path)) {
    m_DiagEngine->raise(diag::fatal_cannot_read_input_err)
        << Path << "Is a directory";
    return false;
  }
  return true;
}

/// \return True if path able to be resolved, otherwise false
bool Input::resolvePath(const LinkerConfig &pConfig) {
  if (m_ResolvedPath)
    return true;
  if (pConfig.options().hasMappingFile() && !isInternal())
    return resolvePathMappingFile(pConfig);
  auto &pSearchDirs = pConfig.directories();
  switch (m_Type) {
  default:
    m_ResolvedPath = eld::sys::fs::Path(m_FileName);
    break;
  case Input::Internal:
    m_ResolvedPath = eld::sys::fs::Path(m_FileName);
    return true;
  }

  if (m_Type == Input::Script) {
    if (pSearchDirs.hasSysRoot() &&
        (m_FileName.size() > 0 && m_FileName[0] == '/')) {
      m_ResolvedPath = pSearchDirs.sysroot();
      m_ResolvedPath->append(m_FileName);
    }
    if (!llvm::sys::fs::exists(m_ResolvedPath->native())) {
      const sys::fs::Path *p = pSearchDirs.find(m_FileName, Input::Script);
      if (p != nullptr)
        m_ResolvedPath = *p;
    }
  }
  if (m_Type == Input::Namespec) {
    const sys::fs::Path *NameSpecPath = nullptr;
    if (m_pAttr.isStatic()) {
      // with --static, we must search an archive.
      NameSpecPath = pSearchDirs.find(m_FileName, Input::Archive);
    } else {
      // otherwise, with --Bdynamic, we can find either an archive or a
      // shared object.
      NameSpecPath = pSearchDirs.find(m_FileName, Input::DynObj);
    }
    if (nullptr == NameSpecPath) {
      m_DiagEngine->raise(diag::err_cannot_find_namespec) << m_FileName;
      return false;
    }
    m_ResolvedPath = *NameSpecPath;
  }

  std::string resolvedPathStr = m_ResolvedPath->native();
  if (!isPathValid(resolvedPathStr)) {
    return false;
  }

  m_ResolvedPathHash = computeFilePathHash(resolvedPathStr);
  m_MemberNameHash = computeFilePathHash(m_FileName);
  MemoryArea *inputMem =
      Input::getMemoryAreaForPath(resolvedPathStr, pConfig.getDiagEngine());
  if (!inputMem)
    inputMem =
        Input::createMemoryArea(resolvedPathStr, pConfig.getDiagEngine());
  if (!inputMem)
    return false;
  setMemArea(inputMem);
  // All queries to return the name of the Input return FileName for the main
  // driver.
  m_Name = m_FileName;
  return true;
}

void Input::setInputFile(InputFile *Inp) { m_InputFile = Inp; }

void Input::overrideInputFile(InputFile *Inp) { m_InputFile = Inp; }

llvm::StringRef Input::getFileContents() const {
  // FIXME: The assert should instead be:
  // ASSERT(m_pMemArea, "Missing memory buffer!");
  ASSERT(m_pMemArea && m_pMemArea->size(), "zero sized!");
  return m_pMemArea->getContents();
}

void Input::releaseMemory(bool isVerbose) {
  if (isVerbose)
    m_DiagEngine->raise(diag::release_file) << decoratedPath();
  isReleased = true;
}

Input::~Input() {
  if (isArchiveMember())
    return;
  releaseMemory(false);
  m_InputFile = nullptr;
}

void Input::addMemberMatchedPattern(const WildcardPattern *W, bool R) {
  MemberPatternMap[W] = R;
}

bool Input::findMemberMatchedPattern(const WildcardPattern *W, bool &result) {
  auto F = MemberPatternMap.find(W);
  if (F == MemberPatternMap.end())
    return false;
  result = F->second;
  return true;
}

uint64_t Input::getWildcardPatternSize() {
  return (sizeof(FilePatternMap) * FilePatternMap.size()) +
         (sizeof(MemberPatternMap) * MemberPatternMap.size());
}

void Input::resize(uint32_t N) {
  FilePatternMap.reserve(N);
  MemberPatternMap.reserve(N);
  PatternMapInitialized = true;
}

void Input::clear() {
  PatternMapInitialized = false;
  FilePatternMap.clear();
  MemberPatternMap.clear();
}

std::string Input::getDecoratedRelativePath(const std::string &basepath) const {
  if (isInternal())
    return decoratedPath(/*showAbsolute=*/false);
  std::error_code ec;
  std::filesystem::path p =
      std::filesystem::relative(getResolvedPath().getFullPath(), basepath, ec);
  if (ec || !std::filesystem::exists(basepath)) {
    m_DiagEngine->raise(diag::warn_unable_to_compute_relpath)
        << getResolvedPath().native() << basepath;
    return decoratedPath(/*showAbsolute=*/false);
  }
  return p.string();
}

llvm::hash_code Input::computeFilePathHash(llvm::StringRef filePath) {
  return llvm::hash_combine(filePath);
}

std::unordered_map<std::string, MemoryArea *>
    Input::m_ResolvedPathToMemoryAreaMap;

MemoryArea *Input::getMemoryAreaForPath(const std::string &filepath,
                                        DiagnosticEngine *diagEngine) {
  auto iter = m_ResolvedPathToMemoryAreaMap.find(filepath);
  if (iter == m_ResolvedPathToMemoryAreaMap.end())
    return nullptr;
  diagEngine->raise(diag::verbose_reusing_prev_memory_mapping) << filepath;
  return iter->second;
}

MemoryArea *Input::createMemoryArea(const std::string &filepath,
                                    DiagnosticEngine *diagEngine) {
  diagEngine->raise(diag::verbose_mapping_file_into_memory) << filepath;
  MemoryArea *inputMem = make<MemoryArea>(filepath);
  if (!inputMem->Init(diagEngine))
    return nullptr;
  m_ResolvedPathToMemoryAreaMap[filepath] = inputMem;
  return inputMem;
}
