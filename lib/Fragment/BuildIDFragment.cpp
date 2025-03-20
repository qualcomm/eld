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
  BuildIdKind = BuildIDKind::NONE;
}

eld::Expected<void>
BuildIDFragment::setBuildIDStyle(const LinkerConfig &Config) {
  std::string BuildId;
  if (!Config.options().hasBuildIDValue())
    BuildId = "fast";
  else
    BuildId = Config.options().getBuildID().lower();
  if (BuildId == "fast")
    BuildIdKind = BuildIDKind::FAST;
  else if (BuildId == "md5")
    BuildIdKind = BuildIDKind::MD5;
  else if (BuildId == "sha1" || BuildId == "tree")
    BuildIdKind = BuildIDKind::SHA1;
  else if (BuildId == "uuid") {
    BuildIdKind = BuildIDKind::UUID;
  } else if (llvm::StringRef(BuildId).starts_with("0x")) {
    BuildIdKind = BuildIDFragment::BuildIDKind::HEXSTRING;
    if (!llvm::all_of(llvm::StringRef(BuildId.substr(2)), llvm::isHexDigit)) {
      return std::make_unique<plugin::DiagnosticEntry>(
          Diag::error_build_id_not_hex, std::vector<std::string>{BuildId});
    }
    BuildIdString = llvm::fromHex(BuildId.substr(2));
  } else if (BuildId == "none") {
    BuildIdKind = BuildIDKind::NONE;
  } else {
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::error_unknown_build_id, std::vector<std::string>{BuildId});
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
  switch (BuildIdKind) {
  case BuildIDKind::FAST:
    return 8;
  case BuildIDKind::MD5:
  case BuildIDKind::UUID:
    return 16;
  case BuildIDKind::SHA1:
    return 20;
  case BuildIDKind::HEXSTRING:
    return BuildIdString.size();
  default:
    break;
  }
  return 0;
}

void BuildIDFragment::dump(llvm::raw_ostream &OS) {}

eld::Expected<void> BuildIDFragment::emit(MemoryRegion &Mr, Module &M) {
  if (BuildIdKind == BuildIDKind::NONE)
    return eld::Expected<void>();
  uint8_t *Out = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  llvm::support::endian::write32le(Out, 4);                 // Name size
  llvm::support::endian::write32le(Out + 4, getHashSize()); // Content size
  llvm::support::endian::write32le(Out + 8, llvm::ELF::NT_GNU_BUILD_ID); // Type
  std::memcpy(Out + 12, "GNU", 4); // Name string
  std::memcpy(Out + 16, BuildIdString.c_str(), BuildIdString.size());
  return eld::Expected<void>();
}

namespace {

// Split one uint8 array into small pieces of uint8 arrays.
std::vector<llvm::ArrayRef<uint8_t>> split(llvm::ArrayRef<uint8_t> Arr,
                                           size_t ChunkSize) {
  std::vector<llvm::ArrayRef<uint8_t>> Ret;
  while (Arr.size() > ChunkSize) {
    Ret.push_back(Arr.take_front(ChunkSize));
    Arr = Arr.drop_front(ChunkSize);
  }
  if (!Arr.empty())
    Ret.push_back(Arr);
  return Ret;
}

// Computes a hash value of Data using a given hash function.
// In order to utilize multiple cores, we first split data into 1MB
// chunks, compute a hash for each chunk, and then compute a hash value
// of the hash values.
void computeHash(
    llvm::MutableArrayRef<uint8_t> HashBuf, llvm::ArrayRef<uint8_t> Data,
    std::function<void(uint8_t *Dest, llvm::ArrayRef<uint8_t> Arr)> hashFn) {
  std::vector<llvm::ArrayRef<uint8_t>> chunks = split(Data, 1024 * 1024);
  const size_t HashesSize = chunks.size() * HashBuf.size();
  std::unique_ptr<uint8_t[]> hashes(new uint8_t[HashesSize]);

  // Compute hash values.
  llvm::parallelFor(0, chunks.size(), [&](size_t I) {
    hashFn(hashes.get() + I * HashBuf.size(), chunks[I]);
  });

  // Write to the final output buffer.
  hashFn(HashBuf.data(), llvm::ArrayRef(hashes.get(), HashesSize));
}
} // namespace

eld::Expected<void> BuildIDFragment::finalizeBuildID(uint8_t *BufferStart,
                                                     size_t BufSize) {

  // Nothing to compute for NONE/HEXSTRING
  if (BuildIdKind == BuildIDKind::NONE || BuildIdKind == BuildIDKind::HEXSTRING)
    return eld::Expected<void>();

  // Fedora introduced build ID as "approximation of true uniqueness across all
  // binaries that might be used by overlapping sets of people". It does not
  // need some security goals that some hash algorithms strive to provide, e.g.
  // (second-)preimage and collision resistance. In practice people use 'md5'
  // and 'sha1' just for different lengths. Implement them with the more
  // efficient BLAKE3.
  size_t HashSize = getHashSize();
  llvm::ArrayRef<uint8_t> Input{BufferStart, BufSize};
  std::unique_ptr<uint8_t[]> BuildId(new uint8_t[HashSize]);
  llvm::MutableArrayRef<uint8_t> Output(BuildId.get(), HashSize);

  switch (BuildIdKind) {
  case BuildIDKind::NONE:
  case BuildIDKind::HEXSTRING:
    break;
  case BuildIDKind::FAST:
    computeHash(Output, Input, [](uint8_t *Dest, llvm::ArrayRef<uint8_t> Arr) {
      llvm::support::endian::write64le(Dest, llvm::xxh3_64bits(Arr));
    });
    break;
  case BuildIDKind::MD5:
    computeHash(
        Output, Input, [HashSize](uint8_t *Dest, llvm::ArrayRef<uint8_t> Arr) {
          std::memcpy(Dest, llvm::BLAKE3::hash<16>(Arr).data(), HashSize);
        });
    break;
  case BuildIDKind::SHA1:
    computeHash(
        Output, Input, [HashSize](uint8_t *Dest, llvm::ArrayRef<uint8_t> Arr) {
          std::memcpy(Dest, llvm::BLAKE3::hash<20>(Arr).data(), HashSize);
        });
    break;
  case BuildIDKind::UUID:
    if (auto Ec = llvm::getRandomBytes(BuildId.get(), HashSize))
      return std::make_unique<plugin::DiagnosticEntry>(
          Diag::error_entropy_source_failure,
          std::vector<std::string>{Ec.message()});
    break;
  }
  BuildIdString = std::string((const char *)(Output.data()), HashSize);
  return eld::Expected<void>();
}
