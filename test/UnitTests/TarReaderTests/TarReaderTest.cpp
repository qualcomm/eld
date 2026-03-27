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

TEST(TarReaderTests, FindFileReturnsMemoryBufferRef) {
  std::string TarPath = createTar({{"a.txt", "hello"}, {"dir/b.bin", "world"}});
  std::string TarData = readFile(TarPath);

  auto FileOrErr = InputTarReader::findFile(TarData, "a.txt");
  ASSERT_TRUE(static_cast<bool>(FileOrErr));
  EXPECT_EQ(FileOrErr->getBuffer(), "hello");

  auto MissingOrErr = InputTarReader::findFile(TarData, "missing.txt");
  EXPECT_FALSE(static_cast<bool>(MissingOrErr));
}
