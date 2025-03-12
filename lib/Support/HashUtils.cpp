//===- HashUtils.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/HashUtils.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MD5.h"
#include "llvm/Support/Parallel.h"
#include "llvm/Support/SHA1.h"
#include "llvm/Support/xxhash.h"

namespace eld {
namespace hash {

size_t getHashSize(HashKind K) {
  switch (K) {
  case XXHash:
    return 8;
  case Md5:
    return 16;
  case Sha1:
    return 20;
  }
  return 0;
}

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

void computeHash(
    llvm::MutableArrayRef<uint8_t> HashBuf, llvm::ArrayRef<uint8_t> Data,
    std::function<void(uint8_t *Dest, llvm::ArrayRef<uint8_t> Arr)> hashFn) {
  // Divide the chunk into 1M parts, and compute hash on that.
  std::vector<llvm::ArrayRef<uint8_t>> chunks = hash::split(Data, 1024 * 1024);
  std::vector<uint8_t> hashes(chunks.size() * HashBuf.size());
  // Compute hash values in parallel.
  llvm::parallelFor(0, chunks.size(), [&](size_t I) {
    hashFn(hashes.data() + I * HashBuf.size(), chunks[I]);
  });
  // Write to the final output buffer.
  hashFn(HashBuf.data(), hashes);
}

std::vector<uint8_t> getHash(hash::HashKind K, const uint8_t *Buffer,
                             size_t BufSize) {
  size_t HashSize = getHashSize(K);
  std::vector<uint8_t> Hash(HashSize);
  llvm::ArrayRef<uint8_t> Buf{Buffer, BufSize};

  switch (K) {
  case hash::HashKind::XXHash:
    hash::computeHash(
        Hash, Buf, [](uint8_t *Dest, llvm::ArrayRef<uint8_t> Arr) {
          llvm::support::endian::write64le(Dest, llvm::xxHash64(Arr));
        });
    break;
  case hash::HashKind::Md5:
    hash::computeHash(
        Hash, Buf, [HashSize](uint8_t *Dest, llvm::ArrayRef<uint8_t> Arr) {
          std::memcpy(Dest, llvm::MD5::hash(Arr).data(), HashSize);
        });
    break;
  case hash::HashKind::Sha1:
    hash::computeHash(
        Hash, Buf, [HashSize](uint8_t *Dest, llvm::ArrayRef<uint8_t> Arr) {
          std::memcpy(Dest, llvm::SHA1::hash(Arr).data(), HashSize);
        });
    break;
  }
  return Hash;
}

uint64_t getXXHash(const uint8_t *Buf, size_t BufSize) {
  std::vector<uint8_t> Hash = getHash(HashKind::XXHash, Buf, BufSize);
  uint64_t R = 0;
  std::memcpy(&R, Hash.data(), Hash.size());
  return R;
}

} // namespace hash
} // namespace eld
