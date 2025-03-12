//===- HashUtils.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_HASHUTILS_H
#define ELD_SUPPORT_HASHUTILS_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include <vector>

namespace eld {
namespace hash {

enum HashKind : uint8_t { XXHash, Md5, Sha1 };

size_t getHashSize(HashKind K);

// Split one uint8 array into small pieces of uint8 arrays.
std::vector<llvm::ArrayRef<uint8_t>> split(llvm::ArrayRef<uint8_t> arr,
                                           size_t chunkSize);

// Compute Hash of a buffer.
void computeHash(
    llvm::MutableArrayRef<uint8_t> hashBuf, llvm::ArrayRef<uint8_t> data,
    std::function<void(uint8_t *dest, llvm::ArrayRef<uint8_t> arr)> hashFn);

std::vector<uint8_t> getHash(hash::HashKind K, const uint8_t *Buf,
                             size_t BufSize);

uint64_t getXXHash(const uint8_t *Buf, size_t BufSize);
} // namespace hash
} // namespace eld

#endif
