//===- SFrameFragment.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Fragment/SFrameFragment.h"
#include "eld/Core/Module.h"
#include "eld/Readers/SFrameSection.h"
#include "eld/Support/Memory.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstring>

using namespace eld;
using namespace llvm::support;
using namespace llvm::support::endian;

SFrameFragment::SFrameFragment(llvm::StringRef name, SFrameSection *owner,
                               int8_t cfaFixedFPOffset, int8_t cfaFixedRAOffset)
    : Fragment(Fragment::SFrame, owner, 1), m_Name(name.str()),
      m_OwnerSection(owner), m_CFAFixedFPOffset(cfaFixedFPOffset),
      m_CFAFixedRAOffset(cfaFixedRAOffset) {
  (void)m_OwnerSection; // Suppress unused field warning
}

SFrameFragment::~SFrameFragment() = default;

const std::string SFrameFragment::name() const { return m_Name; }

size_t SFrameFragment::size() const {
  return calculateHeaderSize() + calculateFDESize() + calculateFRESize();
}

void SFrameFragment::addFunctionDescriptor(
    uint32_t start_addr, uint32_t func_size,
    const std::vector<uint8_t> &fre_data) {
  FunctionDescriptor func;
  func.start_address = start_addr;
  func.func_size = func_size;
  func.fre_data = fre_data;
  m_Functions.push_back(std::move(func));
}

size_t SFrameFragment::calculateHeaderSize() const {
  return sizeof(SFrameHeader); // No auxiliary header for now
}

size_t SFrameFragment::calculateFDESize() const {
  return m_Functions.size() * sizeof(SFrameFuncDescEntry);
}

size_t SFrameFragment::calculateFRESize() const {
  size_t total_size = 0;
  for (const auto &func : m_Functions) {
    total_size += func.fre_data.size();
  }
  return total_size;
}

eld::Expected<void> SFrameFragment::emit(MemoryRegion &region, Module &module) {
  uint8_t *Out = region.begin() + getOffset(module.getConfig().getDiagEngine());

  size_t header_size = calculateHeaderSize();
  size_t fde_size = calculateFDESize();

  // Emit header
  emitHeader(Out);

  // Emit FDEs
  if (!m_Functions.empty()) {
    emitFDEs(Out + header_size);
  }

  // Emit FREs
  size_t fre_size = calculateFRESize();
  if (fre_size > 0) {
    emitFREs(Out + header_size + fde_size);
  }

  return {};
}

void SFrameFragment::emitHeader(uint8_t *buffer) const {
  SFrameHeader header;
  std::memset(&header, 0, sizeof(header));

  // Fill preamble
  header.sfh_preamble.sfp_magic =
      byte_swap<uint16_t>(SFRAME_MAGIC, llvm::endianness::little);
  header.sfh_preamble.sfp_version = SFRAME_VERSION_2;
  header.sfh_preamble.sfp_flags = m_Flags;

  // Fill header fields
  header.sfh_abi_arch = static_cast<uint8_t>(m_ABI);
  header.sfh_cfa_fixed_fp_offset = m_CFAFixedFPOffset;
  header.sfh_cfa_fixed_ra_offset = m_CFAFixedRAOffset;
  header.sfh_auxhdr_len = 0; // No auxiliary header

  // Convert counts to little endian
  header.sfh_num_fdes =
      byte_swap<uint32_t>(m_Functions.size(), llvm::endianness::little);

  // Calculate total FREs
  uint32_t total_fres = 0;
  for (const auto &func : m_Functions) {
    (void)func; // Suppress unused variable warning
    // Assume each function has at least one FRE for now
    total_fres += 1; // Simplified - actual implementation would parse FRE data
  }
  header.sfh_num_fres =
      byte_swap<uint32_t>(total_fres, llvm::endianness::little);

  header.sfh_fre_len =
      byte_swap<uint32_t>(calculateFRESize(), llvm::endianness::little);
  header.sfh_fdeoff =
      byte_swap<uint32_t>(calculateHeaderSize(), llvm::endianness::little);
  header.sfh_freoff = byte_swap<uint32_t>(
      calculateHeaderSize() + calculateFDESize(), llvm::endianness::little);

  // Copy header to buffer
  std::memcpy(buffer, &header, sizeof(header));
}

void SFrameFragment::emitFDEs(uint8_t *buffer) const {
  size_t current_offset = 0;
  size_t current_fre_offset = 0;

  for (const auto &func : m_Functions) {
    SFrameFuncDescEntry fde;
    std::memset(&fde, 0, sizeof(fde));

    fde.sfde_func_start_address =
        byte_swap<int32_t>(func.start_address, llvm::endianness::little);
    fde.sfde_func_size =
        byte_swap<uint32_t>(func.func_size, llvm::endianness::little);
    fde.sfde_func_start_fre_off =
        byte_swap<uint32_t>(current_fre_offset, llvm::endianness::little);
    fde.sfde_func_num_fres =
        byte_swap<uint32_t>(1, llvm::endianness::little); // Simplified

    // Set function info (FDE type and FRE type)
    fde.sfde_func_info = (SFRAME_FDE_TYPE_PCINC << 4) | SFRAME_FRE_TYPE_ADDR4;
    fde.sfde_func_rep_size = 0; // Not used for PCINC type
    fde.sfde_func_padding2 = 0;

    std::memcpy(buffer + current_offset, &fde, sizeof(fde));
    current_offset += sizeof(fde);
    current_fre_offset += func.fre_data.size();
  }
}

void SFrameFragment::emitFREs(uint8_t *buffer) const {
  size_t current_offset = 0;

  for (const auto &func : m_Functions) {
    if (!func.fre_data.empty()) {
      std::memcpy(buffer + current_offset, func.fre_data.data(),
                  func.fre_data.size());
      current_offset += func.fre_data.size();
    }
  }
}

void SFrameFragment::dump(llvm::raw_ostream &os) {
  os << "SFrameFragment: " << m_Name << "\n";
  os << "  Size: " << size() << " bytes\n";
  os << "  ABI: " << static_cast<int>(m_ABI) << "\n";
  os << "  Flags: 0x" << llvm::format("%02x", m_Flags) << "\n";
  os << "  Functions: " << m_Functions.size() << "\n";

  for (size_t i = 0; i < m_Functions.size(); ++i) {
    const auto &func = m_Functions[i];
    os << "    Function " << i << ": start=0x"
       << llvm::format("%08x", func.start_address) << " size=" << func.func_size
       << " fre_data=" << func.fre_data.size() << " bytes\n";
  }
}
