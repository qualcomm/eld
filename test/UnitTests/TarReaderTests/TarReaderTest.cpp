//===- TarReaderTest.cpp---------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "eld/Support/InputTarReader.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/TarWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstdio>

using namespace eld;

namespace {

static std::string
createTar(llvm::ArrayRef<std::pair<llvm::StringRef, llvm::StringRef>> Files) {
  llvm::SmallString<256> TarPath;
  std::error_code EC =
      llvm::sys::fs::createTemporaryFile("eld-tar-reader", ".tar", TarPath);
  EXPECT_FALSE(EC);

  auto TarOrErr = llvm::TarWriter::create(TarPath, "eld-test");
  EXPECT_TRUE(static_cast<bool>(TarOrErr));
  if (!TarOrErr)
    return std::string(TarPath.str());

  for (const auto &It : Files)
    (*TarOrErr)->append(It.first, It.second);

  return std::string(TarPath.str());
}

static std::string readFile(llvm::StringRef Path) {
  auto BufferOrErr = llvm::MemoryBuffer::getFile(Path);
  EXPECT_TRUE(static_cast<bool>(BufferOrErr));
  if (!BufferOrErr)
    return "";
  return (*BufferOrErr)->getBuffer().str();
}

static llvm::StringRef lookupBySuffix(const InputTarReader::FileMap &Files,
                                      llvm::StringRef Suffix) {
  for (const auto &It : Files) {
    if (llvm::StringRef(It.getKey()).ends_with(Suffix))
      return It.getValue();
  }
  return "";
}

} // namespace

TEST(TarReaderTests, UntarFromMemory) {
  std::string TarPath = createTar({{"a.txt", "hello"}, {"dir/b.bin", "world"}});
  std::string TarData = readFile(TarPath);

  auto FilesOrErr = InputTarReader::untar(TarData);
  ASSERT_TRUE(static_cast<bool>(FilesOrErr));

  EXPECT_EQ(lookupBySuffix(*FilesOrErr, "/a.txt"), "hello");
  EXPECT_EQ(lookupBySuffix(*FilesOrErr, "/dir/b.bin"), "world");
}

TEST(TarReaderTests, UntarFromFile) {
  std::string TarPath = createTar({{"nested/path/c.txt", "content"}});

  auto FilesOrErr = InputTarReader::untarFile(TarPath);
  ASSERT_TRUE(static_cast<bool>(FilesOrErr));

  EXPECT_EQ(lookupBySuffix(*FilesOrErr, "/nested/path/c.txt"), "content");
}

TEST(TarReaderTests, RejectsTruncatedTar) {
  std::string TarPath = createTar({{"a.txt", "abc"}});
  std::string TarData = readFile(TarPath);
  ASSERT_GE(TarData.size(), 600u);
  TarData.resize(600);

  auto FilesOrErr = InputTarReader::untar(TarData);
  EXPECT_FALSE(static_cast<bool>(FilesOrErr));
}

TEST(TarReaderTests, EmitsEntryNames) {
  std::string TarPath = createTar({{"a.txt", "hello"}, {"dir/b.bin", "world"}});

  std::string Emitted;
  llvm::raw_string_ostream OS(Emitted);
  auto Res = InputTarReader::emitEntryNamesFile(TarPath, OS);
  EXPECT_TRUE(static_cast<bool>(Res));
  OS.flush();

  EXPECT_TRUE(Emitted.find("a.txt") != std::string::npos);
  EXPECT_TRUE(Emitted.find("dir/b.bin") != std::string::npos);
}

TEST(TarReaderTests, FindFileReturnsMemoryBuffer) {
  std::string TarPath = createTar({{"a.txt", "hello"}, {"dir/b.bin", "world"}});
  std::string TarData = readFile(TarPath);

  auto FileOrErr = InputTarReader::findFile(TarData, "a.txt");
  ASSERT_TRUE(static_cast<bool>(FileOrErr));
  EXPECT_EQ((*FileOrErr)->getBuffer(), "hello");
  EXPECT_EQ((*FileOrErr)->getBufferIdentifier(), "a.txt");

  auto MissingOrErr = InputTarReader::findFile(TarData, "missing.txt");
  EXPECT_FALSE(static_cast<bool>(MissingOrErr));
}

TEST(TarReaderTests, RecognizesTarArchives) {
  std::string TarData = readFile(createTar({{"a.txt", "hello"}}));
  EXPECT_TRUE(InputTarReader::isTarArchive(TarData));

  // A name that does not fit in a ustar header makes TarWriter start the
  // archive with a pax extended header.
  std::string LongName(200, 'n');
  EXPECT_TRUE(
      InputTarReader::isTarArchive(readFile(createTar({{LongName, "hello"}}))));

  // V7 headers have no magic: clear the fields that POSIX added to the first
  // header and recompute its checksum.
  std::string V7 = TarData;
  std::fill(V7.begin() + 257, V7.begin() + 512, '\0');
  std::fill(V7.begin() + 148, V7.begin() + 156, ' ');
  unsigned Sum = 0;
  for (size_t I = 0; I < 512; ++I)
    Sum += static_cast<unsigned char>(V7[I]);
  std::snprintf(&V7[148], 8, "%06o", Sum);
  EXPECT_TRUE(InputTarReader::isTarArchive(V7));
}

TEST(TarReaderTests, RejectsNonTarData) {
  std::string TarData = readFile(createTar({{"a.txt", "hello"}}));
  // Any change to the header invalidates its checksum.
  std::string Corrupted = TarData;
  Corrupted[0] ^= 1;
  EXPECT_FALSE(InputTarReader::isTarArchive(Corrupted));
  EXPECT_FALSE(
      InputTarReader::isTarArchive(llvm::StringRef(TarData).take_front(511)));
  EXPECT_FALSE(InputTarReader::isTarArchive(std::string(1024, '\0')));

  std::string Script;
  while (Script.size() < 1024)
    Script += "SECTIONS { .text : { *(.text) } }\n";
  EXPECT_FALSE(InputTarReader::isTarArchive(Script));
}
