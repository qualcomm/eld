//===- TarWriter.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/PluginAPI/TarWriter.h"
#include "eld/Support/MemoryArea.h"
#include "llvm/Support/TarWriter.h"

using namespace eld;
using namespace eld::plugin;

plugin::TarWriter::TarWriter(std::unique_ptr<llvm::TarWriter> TW) {
  m_TW = std::move(TW);
}

TarWriter::TarWriter(TarWriter &&other) noexcept : m_TW(nullptr) {
  *this = std::move(other);
}

TarWriter &TarWriter::operator=(TarWriter &&other) noexcept {
  if (this == &other)
    return *this;
  std::swap(m_TW, other.m_TW);
  return *this;
}

eld::Expected<void>
plugin::TarWriter::addBufferToTar(plugin::MemoryBuffer &buffer) const {
  auto buf = buffer.getBuffer();
  m_TW->append(buf->getName(), buf->getContents());
  return {};
}

TarWriter::~TarWriter() {}