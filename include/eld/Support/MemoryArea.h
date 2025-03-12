//===- MemoryArea.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef ELD_SUPPORT_MEMORYAREA_H
#define ELD_SUPPORT_MEMORYAREA_H

#include "eld/Support/Memory.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/MemoryBufferRef.h"

namespace llvm {
class StringRef;
}

namespace eld {
class DiagnosticEngine;
/** \class MemoryArea
 *  \brief MemoryArea is used to manage input read-only memory space.
 */
class MemoryArea {
public:
  // Read a Input File.
  explicit MemoryArea(llvm::StringRef pFilename);

  // Initialize Memory area.
  bool Init(DiagnosticEngine *);

  // Initialize a MemoryArea with a MemoryBuffer.
  explicit MemoryArea(std::unique_ptr<llvm::MemoryBuffer> Buf);

  explicit MemoryArea(llvm::MemoryBufferRef bufRef);

  // Form a MemoryArea from an existing Buffer.
  static MemoryArea *CreateCopy(llvm::StringRef Buf);

  // Form a MemoryArea from an existing buffer without copying it.
  static MemoryArea *CreateRef(llvm::StringRef Buf, std::string BufferName,
                               bool isNullTerminated);

  // Form a MemoryArea from an existing buffer without copying it and return
  // unique MemoryArea pointer.
  static std::unique_ptr<MemoryArea>
  CreateUniqueRef(const std::string &FileName, const uint8_t *Data,
                  size_t Length, bool isNullTerminated);

  // Request a Piece of an Input File.
  llvm::StringRef request(size_t pOffset, size_t pLength);

  llvm::StringRef getContents() const { return MB->getBuffer(); }

  // Return the size of the Input File.
  size_t size() const;

  llvm::MemoryBufferRef getMemoryBufferRef() const { return *MB; }

  // Return buffer name.
  llvm::StringRef getName() const;

private:
  std::unique_ptr<llvm::MemoryBuffer> MB;
  std::string m_FileName;
};

} // namespace eld

#endif
