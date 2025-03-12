//===- OutputTarWriter.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_SUPPORT_OUTPUTTARWRITER_H
#define ELD_SUPPORT_OUTPUTTARWRITER_H

#include "eld/Input/ArchiveFile.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include "eld/Support/INIWriter.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/TarWriter.h"
#include <unordered_map>
#include <unordered_set>

namespace eld {

class LinkerConfig;
class MappingFile;
class Module;
class ProgressBar;

/// \class OutputTarWriter
/// \brief OutputTarWriter uses llvm::TarWriter to create --response tarball
/// output
class OutputTarWriter {
public:
  /// Create an OutputTarWriter
  /// \param OutputPath where the tarball should be written
  /// \param doCompress Compressed Tar files
  OutputTarWriter(LinkerConfig &Config);

  /// Create Output File.
  bool createOutput();

  /// Add an inputfile to this tarball
  /// \param file the file to add to this tarball
  void addInputFile(const eld::InputFile *file, bool isLTOObject);

  /// Adds mapping.ini file to this tarball
  bool createMappingFile();

  /// Adds response.txt file to this tarball
  bool createResponseFile(llvm::StringRef response);

  /// Adds version.txt file to this tarball
  bool createVersionFile();

  /// Compresses the tar ball in zlib format. Can be uncompressed with pigz
  bool doCompress();

  /// Write the tarball to disk
  bool writeOutput(bool showProgress);

  /// \returns the name of the version file in this tarball
  llvm::StringRef getVersionFileName() const;

  /// \returns the name of the output Tarball file
  llvm::StringRef getTarFileName() const;

  /// \returns the name of the mapping file between path and hash
  llvm::StringRef getMappingFileName() const;

  /// creates an inputfile from an existing file and adds it to the tarball
  void createAndAddFile(std::string filename, std::string resolvedPath,
                        MappingFile::Kind K, bool isLTOObject);

  void createAndAddPlugin(std::string filename, std::string resolvedPath);

  void createAndAddConfigFile(std::string filename, std::string resolvedPath);

  void createAndAddScriptFile(std::string filename, std::string resolvedPath);

  void createAndAddCacheFile(std::string filename);

  // writes a single file and increments the progress bar
  void writeFile(llvm::StringRef name, llvm::StringRef contents);

  /// \returns path rewritten as sha2 hash
  std::string rewritePath(llvm::StringRef s);

  const std::string getMappings() const;

  /// \returns vector of filenames of LTO objects
  std::vector<std::string> &getLTOObjects() { return LTOELFFiles; }

  operator bool() const { return !!Tar; }

  /// Adds all the members of the thin archive AF in the reproducer.
  void addThinArchiveMembers(const ArchiveFile *AF);

private:
  std::string getHashAndExtension(const Input *ipt) const;

  LinkerConfig &m_Config;
  eld::ProgressBar *m_ProgressBar = nullptr;
  std::unique_ptr<llvm::TarWriter> Tar = nullptr;
  std::unique_ptr<eld::INIWriter> INIFileWriter = nullptr;
  std::vector<std::string> LTOELFFiles;
  std::unordered_set<eld::Input *> plugins;
  llvm::StringMap<const eld::InputFile *> InputMap;
  llvm::StringRef mappingFileName = "mapping.ini";
  llvm::StringRef versionFileName = "version.txt";
  llvm::StringRef responseFileName = "response.txt";
  llvm::StringRef versionContents;
  llvm::StringRef responseContents;
  std::string TemporaryCacheFile;
  std::string tarFileName;
  std::string m_OutputPath;
  std::string tarFilePath;
  bool m_Compress = false;
  bool m_Verbose = false;
};
} // namespace eld

#endif
