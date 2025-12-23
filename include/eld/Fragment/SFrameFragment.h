//===- SFrameFragment.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_SFRAMEFRAGMENT_H
#define ELD_FRAGMENT_SFRAMEFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include "eld/Support/MemoryRegion.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <string>
#include <vector>

namespace eld {

class Module;
class DiagnosticEngine;
class SFrameSection;

// SFrame constants from the specification
constexpr uint16_t SFRAME_MAGIC = 0xdee2;
constexpr uint8_t SFRAME_VERSION_2 = 2;

// SFrame ABI/Arch identifiers
enum SFrameABI : uint8_t {
  SFRAME_ABI_AARCH64_ENDIAN_BIG = 1,
  SFRAME_ABI_AARCH64_ENDIAN_LITTLE = 2,
  SFRAME_ABI_AMD64_ENDIAN_LITTLE = 3,
  SFRAME_ABI_I386_ENDIAN_LITTLE = 4,
  SFRAME_ABI_S390X_ENDIAN_BIG = 5,
  SFRAME_ABI_RISCV64_ENDIAN_LITTLE = 6,
  SFRAME_ABI_RISCV32_ENDIAN_LITTLE = 7,
  SFRAME_ABI_HEXAGON_ENDIAN_LITTLE = 8,
};

// SFrame flags
enum SFrameFlags : uint8_t {
  SFRAME_F_FDE_SORTED = 0x1,
  SFRAME_F_FRAME_POINTER = 0x2,
  SFRAME_F_FDE_FUNC_START_PCREL = 0x4,
};

// SFrame FDE types
enum SFrameFDEType : uint8_t {
  SFRAME_FDE_TYPE_PCINC = 0,
  SFRAME_FDE_TYPE_PCMASK = 1,
};

// SFrame FRE types
enum SFrameFREType : uint8_t {
  SFRAME_FRE_TYPE_ADDR1 = 0,
  SFRAME_FRE_TYPE_ADDR2 = 1,
  SFRAME_FRE_TYPE_ADDR4 = 2,
};

// SFrame data structures from specification
struct SFramePreamble {
  uint16_t sfp_magic;
  uint8_t sfp_version;
  uint8_t sfp_flags;
} __attribute__((packed));

struct SFrameHeader {
  SFramePreamble sfh_preamble;
  uint8_t sfh_abi_arch;
  int8_t sfh_cfa_fixed_fp_offset;
  int8_t sfh_cfa_fixed_ra_offset;
  uint8_t sfh_auxhdr_len;
  uint32_t sfh_num_fdes;
  uint32_t sfh_num_fres;
  uint32_t sfh_fre_len;
  uint32_t sfh_fdeoff;
  uint32_t sfh_freoff;
} __attribute__((packed));

struct SFrameFuncDescEntry {
  int32_t sfde_func_start_address;
  uint32_t sfde_func_size;
  uint32_t sfde_func_start_fre_off;
  uint32_t sfde_func_num_fres;
  uint8_t sfde_func_info;
  uint8_t sfde_func_rep_size;
  uint16_t sfde_func_padding2;
} __attribute__((packed));

struct SFrameFrameRowEntryAddr1 {
  uint8_t sfre_start_addr;
  uint8_t sfre_info;
} __attribute__((packed));

struct SFrameFrameRowEntryAddr2 {
  uint16_t sfre_start_addr;
  uint8_t sfre_info;
} __attribute__((packed));

struct SFrameFrameRowEntryAddr4 {
  uint32_t sfre_start_addr;
  uint8_t sfre_info;
} __attribute__((packed));

/// \class SFrameFragment
/// Fragment representing SFrame stack unwinding information
class SFrameFragment : public Fragment {
public:
  SFrameFragment(llvm::StringRef name, SFrameSection *owner,
                 int8_t cfaFixedFPOffset = 0, int8_t cfaFixedRAOffset = 0);

  virtual ~SFrameFragment();

  const std::string name() const;

  size_t size() const override;

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::SFrame;
  }

  static bool classof(const SFrameFragment *) { return true; }

  eld::Expected<void> emit(MemoryRegion &region, Module &module) override;

  void dump(llvm::raw_ostream &os) override;

  // Add function descriptor entry
  void addFunctionDescriptor(uint32_t start_addr, uint32_t size,
                             const std::vector<uint8_t> &fre_data);

  // Set SFrame properties
  void setABI(SFrameABI abi) { m_ABI = abi; }
  void setFlags(uint8_t flags) { m_Flags = flags; }
  void setCFAFixedOffsets(int8_t fp_offset, int8_t ra_offset) {
    m_CFAFixedFPOffset = fp_offset;
    m_CFAFixedRAOffset = ra_offset;
  }

private:
  struct FunctionDescriptor {
    uint32_t start_address;
    uint32_t func_size;
    std::vector<uint8_t> fre_data;
  };

  std::string m_Name;
  SFrameSection *m_OwnerSection;
  std::vector<FunctionDescriptor> m_Functions;
  SFrameABI m_ABI = SFRAME_ABI_AMD64_ENDIAN_LITTLE;
  uint8_t m_Flags = SFRAME_F_FDE_SORTED;
  int8_t m_CFAFixedFPOffset = 0;
  int8_t m_CFAFixedRAOffset = 0;

  // Helper methods
  size_t calculateHeaderSize() const;
  size_t calculateFDESize() const;
  size_t calculateFRESize() const;
  void emitHeader(uint8_t *buffer) const;
  void emitFDEs(uint8_t *buffer) const;
  void emitFREs(uint8_t *buffer) const;
};

} // namespace eld

#endif // ELD_FRAGMENT_SFRAMEFRAGMENT_H
