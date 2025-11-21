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
#include "llvm/Support/raw_ostream.h"
#include <atomic>
#include <cstring> // For strstr, etc.
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace eld {

extern llvm::BumpPtrAllocator BAlloc;
extern llvm::StringSaver Saver;

void freeArena();

struct SpecificAllocBase {
  SpecificAllocBase() { Instances.push_back(this); }
  virtual ~SpecificAllocBase() = default;

  virtual void reset() = 0;
  virtual const char *typeName() const = 0;
  virtual size_t sizeOfT() const = 0;
  virtual size_t count() const = 0;
  virtual size_t totalBytes() const = 0;
  virtual void report(llvm::raw_ostream &OS) const = 0;

  static std::vector<SpecificAllocBase *> Instances;
};

// Detect presence of static T::getTypeName()
template <typename T, typename = void>
struct HasGetTypeName : std::false_type {};

template <typename T>
struct HasGetTypeName<T, std::void_t<decltype(T::getTypeName())>>
    : std::true_type {};

// --- Internal detail for getting type names without RTTI ---
namespace detail {

// Fallback: Parse type name from compiler-specific function signature macro
template <typename T> std::string parseTypeNameFromSignature() {
#if defined(__clang__) || defined(__GNUC__)
  const char *signature = __PRETTY_FUNCTION__;
  const char *start = strstr(signature, "T = ");
  if (!start)
    return "parsing_failed";
  start += 4; // Move past "T = "
  const char *end = strchr(start, ']');
  if (!end)
    return "parsing_failed";
  return std::string(start, end - start);
#elif defined(_MSC_VER)
  const char *signature = __FUNCSIG__;
  const char *start = strstr(signature, "parseTypeNameFromSignature<");
  if (!start)
    return "parsing_failed";
  start += strlen("parseTypeNameFromSignature<");

  // MSVC might add "class ", "struct ", etc.
  if (strncmp(start, "class ", 6) == 0)
    start += 6;
  else if (strncmp(start, "struct ", 7) == 0)
    start += 7;

  // Find the matching '>' at the end
  const char *end = strrchr(start, '>');
  if (!end)
    return "parsing_failed";

  return std::string(start, end - start);
#else
  return "unsupported_compiler";
#endif
}

// Resolve type name: prefer user-defined, then fallback to macro parsing
template <typename T> std::string resolveTypeName() {
  if constexpr (HasGetTypeName<T>::value) {
    return T::getTypeName();
  } else {
    // Priority 2: Fallback to parsing compiler macros
    return parseTypeNameFromSignature<T>();
  }
}

} // namespace detail

template <class T> struct SpecificAlloc : public SpecificAllocBase {
  // The constructor now uses the robust resolveTypeName helper
  SpecificAlloc() : TypeName(detail::resolveTypeName<T>()) {}

  void reset() override {
    Alloc.DestroyAll();
    Cnt.store(0, std::memory_order_relaxed);
  }

  void onAllocate() { Cnt.fetch_add(1, std::memory_order_relaxed); }

  const char *typeName() const override { return TypeName.c_str(); }
  size_t sizeOfT() const override { return sizeof(T); }
  size_t count() const override { return Cnt.load(std::memory_order_relaxed); }
  size_t totalBytes() const override { return count() * sizeof(T); }

  void report(llvm::raw_ostream &OS) const override {
    OS << "  - " << typeName() << ": count=" << count()
       << ", sizeof(T)=" << sizeOfT() << "B"
       << ", total=" << totalBytes() << "B\n";
  }

  llvm::SpecificBumpPtrAllocator<T> Alloc;
  std::string TypeName;
  std::atomic<size_t> Cnt{0};
};

template <typename T> struct ArenaForType {
  static SpecificAlloc<T> &getAlloc() {
    static SpecificAlloc<T> Alloc;
    return Alloc;
  }

  template <typename... U> static T *create(U &&...Args) {
    auto &Alloc = getAlloc();
    T *P = new (Alloc.Alloc.Allocate()) T(std::forward<U>(Args)...);
    Alloc.onAllocate();
    return P;
  }
};

template <typename T, typename... U> T *make(U &&...Args) {
  return ArenaForType<T>::create(std::forward<U>(Args)...);
}

std::string formatBytes(size_t bytes);

void dumpArenaReport(llvm::raw_ostream &OS);

const char *getUninitBuffer(uint32_t Sz);

} // namespace eld

#endif
