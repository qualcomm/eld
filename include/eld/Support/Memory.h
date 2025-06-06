//===- Memory.h------------------------------------------------------------===//
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

//===----------------------------------------------------------------------===//
//
// This file defines arena allocators.
//
// Almost all large objects, such as files, sections or symbols, are
// used for the entire lifetime of the linker once they are created.
// This usage characteristic makes arena allocator an attractive choice
// where the entire linker is one arena. With an arena, newly created
// objects belong to the arena and freed all at once when everything is done.
// Arena allocators are efficient and easy to understand.
// Most objects are allocated using the arena allocators defined by this file.
//
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_MEMORY_H
#define ELD_SUPPORT_MEMORY_H

#include "llvm/Support/Allocator.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/StringSaver.h"
#include <vector>

namespace eld {

// Use this arena if your object doesn't have a destructor.
extern llvm::BumpPtrAllocator BAlloc;
extern llvm::StringSaver Saver;
void freeArena();

// These two classes are hack to keep track of all
// SpecificBumpPtrAllocator instances.
struct SpecificAllocBase {
  SpecificAllocBase() { Instances.push_back(this); }
  virtual ~SpecificAllocBase() = default;
  virtual void reset() = 0;
  static std::vector<SpecificAllocBase *> Instances;
};

template <class T> struct SpecificAlloc : public SpecificAllocBase {
  void reset() override { Alloc.DestroyAll(); }
  llvm::SpecificBumpPtrAllocator<T> Alloc;
};

// Use this arena if your object has a destructor.
// Your destructor will be invoked from freeArena().
template <typename T, typename... U> T *make(U &&...Args) {
  static SpecificAlloc<T> Alloc;
  return new (Alloc.Alloc.Allocate()) T(std::forward<U>(Args)...);
}

const char *getUninitBuffer(uint32_t Sz);

} // namespace eld

#endif
