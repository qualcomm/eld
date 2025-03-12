//===- MappingFile.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Support/MappingFile.h"
#include "eld/Input/InputFile.h"

using namespace eld;

std::string MappingFile::getMappingCategoryForInputFile(const InputFile *F) {
  switch (F->getMappingFileKind()) {
  case Kind::Archive:
    return "Archive Files";
  case Kind::Bitcode:
    return "Bitcode Files";
  case Kind::Config:
    return "Config Files";
  case Kind::DynamicList:
    return "Dynamic List";
  case Kind::ExternList:
    return "Extern List";
  case Kind::VersionScript:
    return "Version Script";
  case Kind::LinkerScript:
    return "Linker Scripts";
  case Kind::ObjectFile:
    return "Object Files";
  case Kind::Plugin:
    return "Plugins";
  case Kind::SharedLibrary:
    return "Shared Libraries";
  case Kind::SymDef:
    return "Symdef Files";
  case Kind::Unknown:
    return "Unknown";
  default:
    return "Other Files";
  }
}

std::string MappingFile::getDirectoryNameForMappingKind(MappingFile::Kind K) {
  switch (K) {
  case Kind::Archive:
    return "Archives";
  case Kind::Bitcode:
    return "Bitcode";
  case Kind::Config:
    return "Config";
  case Kind::DynamicList:
    return "DynamicList";
  case Kind::ExternList:
    return "ExternList";
  case Kind::VersionScript:
    return "VersionScript";
  case Kind::LinkerScript:
    return "LinkerScript";
  case Kind::ObjectFile:
    return "Object";
  case Kind::Plugin:
    return "Plugins";
  case Kind::SharedLibrary:
    return "SharedLibrary";
  case Kind::SymDef:
    return "SymDef";
  case Kind::Unknown:
    return "Unknown";
  default:
    return "Other";
  }
}

std::string MappingFile::getPathWithDirectory(std::string FileName,
                                              MappingFile::Kind K) {
  return getDirectoryNameForMappingKind(K) + '/' + FileName;
}

std::string MappingFile::getThinArchiveMemSeparator() {
  return "/<ThinArchiveMem>/";
}