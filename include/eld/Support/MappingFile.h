//===- MappingFile.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_SUPPORT_MAPPINGFILE_H
#define ELD_SUPPORT_MAPPINGFILE_H

#include <string>

namespace eld {

class InputFile;

class MappingFile {
public:
  enum Kind {
    ObjectFile,
    LinkerScript,
    Plugin,
    Config,
    SymDef,
    Other,
    Archive,
    Bitcode,
    SharedLibrary,
    CacheFile,
    DynamicList,
    ExternList,
    VersionScript,
    Unknown
  };

  static std::string getMappingCategoryForInputFile(const InputFile *F);

  static std::string getDirectoryNameForMappingKind(MappingFile::Kind K);

  /// \return path as [directory]/[filename] where directory is determined by
  /// Kind
  static std::string getPathWithDirectory(std::string FileName,
                                          MappingFile::Kind K);

  /// Thin archive members are stored in the 'Object Files' section of the
  /// mapping file as '<ArchiveName>/<ThinArchiveMem>/<ArchiveMemberName>'.
  /// Here, '/<ThinArchiveMem>/ is used to separate the archive and the archive
  /// member. This function returns this separator.
  static std::string getThinArchiveMemSeparator();
};

} // namespace eld

#endif
