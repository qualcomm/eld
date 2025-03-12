//===- MemoryArea.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/MemoryArea.h"
#include "eld/Diagnostics/Diagnostic.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/MemoryBufferRef.h"

using namespace eld;

//===--------------------------------------------------------------------===//
// MemoryArea
//===--------------------------------------------------------------------===//
MemoryArea::MemoryArea(llvm::StringRef Filename) : m_FileName(Filename) {}

bool MemoryArea::Init(DiagnosticEngine *DiagEngine) {
  auto MBOrErr = llvm::MemoryBuffer::getFile(m_FileName);
  if (auto EC = MBOrErr.getError()) {
    DiagEngine->raise(diag::fatal_cannot_read_input_err)
        << m_FileName << EC.message();
    return false;
  }
  MB = std::move(*MBOrErr);
  return true;
}

MemoryArea::MemoryArea(std::unique_ptr<llvm::MemoryBuffer> Buf) {
  MB = std::move(Buf);
}

MemoryArea::MemoryArea(llvm::MemoryBufferRef BufRef) {
  MB = llvm::MemoryBuffer::getMemBuffer(BufRef,
                                        /*RequiresNullTerminator=*/false);
}

MemoryArea *MemoryArea::CreateCopy(llvm::StringRef Buf) {
  std::unique_ptr<llvm::MemoryBuffer> MB =
      llvm::MemoryBuffer::getMemBufferCopy(Buf);
  return make<MemoryArea>(std::move(MB));
}

MemoryArea *MemoryArea::CreateRef(llvm::StringRef Buf, std::string BufferName,
                                  bool IsNullTerminated) {
  std::unique_ptr<llvm::MemoryBuffer> MB =
      llvm::MemoryBuffer::getMemBuffer(Buf, BufferName, IsNullTerminated);
  return make<MemoryArea>(std::move(MB));
}

std::unique_ptr<MemoryArea>
MemoryArea::CreateUniqueRef(const std::string &FileName, const uint8_t *Data,
                            size_t Length, bool IsNullTerminated) {
  std::unique_ptr<llvm::MemoryBuffer> MB = llvm::MemoryBuffer::getMemBuffer(
      llvm::StringRef(reinterpret_cast<const char *>(Data), Length), FileName,
      IsNullTerminated);
  return std::make_unique<MemoryArea>(std::move(MB));
}

llvm::StringRef MemoryArea::request(size_t Offset, size_t Length) {
  return llvm::StringRef(MB->getBuffer().data() + Offset, Length);
}

size_t MemoryArea::size() const { return (MB ? MB->getBufferSize() : 0); }

llvm::StringRef MemoryArea::getName() const {
  return MB->getBufferIdentifier();
}
