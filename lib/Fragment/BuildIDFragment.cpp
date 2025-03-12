//===- BuildIDFragment.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


#include "eld/Fragment/BuildIDFragment.h"
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/BLAKE3.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Parallel.h"
#include "llvm/Support/RandomNumberGenerator.h"
#include "llvm/Support/xxhash.h"

using namespace eld;

BuildIDFragment::BuildIDFragment(ELFSection *O)
    : Fragment(Fragment::BuildID, O) {
  m_BuildIDKind = BuildIDKind::NONE;
}

eld::Expected<void>
BuildIDFragment::setBuildIDStyle(const LinkerConfig &Config) {
  std::string buildID;
  if (!Config.options().hasBuildIDValue())
    buildID = "fast";
  else
    buildID = Config.options().getBuildID().lower();
  if (buildID == "fast")
    m_BuildIDKind = BuildIDKind::FAST;
  else if (buildID == "md5")
    m_BuildIDKind = BuildIDKind::MD5;
  else if (buildID == "sha1" || buildID == "tree")
    m_BuildIDKind = BuildIDKind::SHA1;
  else if (buildID == "uuid") {
    m_BuildIDKind = BuildIDKind::UUID;
  } else if (llvm::StringRef(buildID).starts_with("0x")) {
    m_BuildIDKind = BuildIDFragment::BuildIDKind::HEXSTRING;
    if (!llvm::all_of(llvm::StringRef(buildID.substr(2)), llvm::isHexDigit)) {
      return std::make_unique<plugin::DiagnosticEntry>(
          diag::error_build_id_not_hex, std::vector<std::string>{buildID});
    }
    m_BuildID = llvm::fromHex(buildID.substr(2));
  } else if (buildID == "none") {
    m_BuildIDKind = BuildIDKind::NONE;
  } else {
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::error_unknown_build_id, std::vector<std::string>{buildID});
  }
  return eld::Expected<void>();
}

BuildIDFragment::~BuildIDFragment() {}

size_t BuildIDFragment::size() const {
  size_t HashSize = getHashSize();
  if (!HashSize)
    return 0;
  return HashSize + 16;
}

size_t BuildIDFragment::getHashSize() const {
  switch (m_BuildIDKind) {
  case BuildIDKind::FAST:
    return 8;
  case BuildIDKind::MD5:
  case BuildIDKind::UUID:
    return 16;
  case BuildIDKind::SHA1:
    return 20;
  case BuildIDKind::HEXSTRING:
    return m_BuildID.size();
  default:
    break;
  }
  return 0;
}

void BuildIDFragment::dump(llvm::raw_ostream &OS) {}

eld::Expected<void> BuildIDFragment::emit(MemoryRegion &mr, Module &M) {
  if (m_BuildIDKind == BuildIDKind::NONE)
    return eld::Expected<void>();
  uint8_t *out = mr.begin() + getOffset(M.getConfig().getDiagEngine());
  llvm::support::endian::write32le(out, 4);                 // Name size
  llvm::support::endian::write32le(out + 4, getHashSize()); // Content size
  llvm::support::endian::write32le(out + 8, llvm::ELF::NT_GNU_BUILD_ID); // Type
  std::memcpy(out + 12, "GNU", 4); // Name string
  std::memcpy(out + 16, m_BuildID.c_str(), m_BuildID.size());
  return eld::Expected<void>();
}

namespace {

// Split one uint8 array into small pieces of uint8 arrays.
std::vector<llvm::ArrayRef<uint8_t>> split(llvm::ArrayRef<uint8_t> arr,
                                           size_t chunkSize) {
  std::vector<llvm::ArrayRef<uint8_t>> ret;
  while (arr.size() > chunkSize) {
    ret.push_back(arr.take_front(chunkSize));
    arr = arr.drop_front(chunkSize);
  }
  if (!arr.empty())
    ret.push_back(arr);
  return ret;
}

// Computes a hash value of Data using a given hash function.
// In order to utilize multiple cores, we first split data into 1MB
// chunks, compute a hash for each chunk, and then compute a hash value
// of the hash values.
void computeHash(
    llvm::MutableArrayRef<uint8_t> hashBuf, llvm::ArrayRef<uint8_t> data,
    std::function<void(uint8_t *dest, llvm::ArrayRef<uint8_t> arr)> hashFn) {
  std::vector<llvm::ArrayRef<uint8_t>> chunks = split(data, 1024 * 1024);
  const size_t hashesSize = chunks.size() * hashBuf.size();
  std::unique_ptr<uint8_t[]> hashes(new uint8_t[hashesSize]);

  // Compute hash values.
  llvm::parallelFor(0, chunks.size(), [&](size_t i) {
    hashFn(hashes.get() + i * hashBuf.size(), chunks[i]);
  });

  // Write to the final output buffer.
  hashFn(hashBuf.data(), llvm::ArrayRef(hashes.get(), hashesSize));
}
} // namespace

eld::Expected<void> BuildIDFragment::finalizeBuildID(uint8_t *BufferStart,
                                                     size_t BufSize) {

  // Nothing to compute for NONE/HEXSTRING
  if (m_BuildIDKind == BuildIDKind::NONE ||
      m_BuildIDKind == BuildIDKind::HEXSTRING)
    return eld::Expected<void>();

  // Fedora introduced build ID as "approximation of true uniqueness across all
  // binaries that might be used by overlapping sets of people". It does not
  // need some security goals that some hash algorithms strive to provide, e.g.
  // (second-)preimage and collision resistance. In practice people use 'md5'
  // and 'sha1' just for different lengths. Implement them with the more
  // efficient BLAKE3.
  size_t hashSize = getHashSize();
  llvm::ArrayRef<uint8_t> input{BufferStart, BufSize};
  std::unique_ptr<uint8_t[]> buildId(new uint8_t[hashSize]);
  llvm::MutableArrayRef<uint8_t> output(buildId.get(), hashSize);

  switch (m_BuildIDKind) {
  case BuildIDKind::NONE:
  case BuildIDKind::HEXSTRING:
    break;
  case BuildIDKind::FAST:
    computeHash(output, input, [](uint8_t *dest, llvm::ArrayRef<uint8_t> arr) {
      llvm::support::endian::write64le(dest, llvm::xxh3_64bits(arr));
    });
    break;
  case BuildIDKind::MD5:
    computeHash(output, input, [&](uint8_t *dest, llvm::ArrayRef<uint8_t> arr) {
      std::memcpy(dest, llvm::BLAKE3::hash<16>(arr).data(), hashSize);
    });
    break;
  case BuildIDKind::SHA1:
    computeHash(output, input, [&](uint8_t *dest, llvm::ArrayRef<uint8_t> arr) {
      std::memcpy(dest, llvm::BLAKE3::hash<20>(arr).data(), hashSize);
    });
    break;
  case BuildIDKind::UUID:
    if (auto ec = llvm::getRandomBytes(buildId.get(), hashSize))
      return std::make_unique<plugin::DiagnosticEntry>(
          diag::error_entropy_source_failure,
          std::vector<std::string>{ec.message()});
    break;
  }
  m_BuildID = std::string((const char *)(output.data()), hashSize);
  return eld::Expected<void>();
}
