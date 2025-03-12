//===- OutputSectDataFragment.cpp------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Fragment/OutputSectDataFragment.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Script/Expression.h"

using namespace eld;

OutputSectDataFragment::OutputSectDataFragment(OutputSectData &outSectData)
    : Fragment(Fragment::Type::OutputSectDataFragType,
               outSectData.getELFSection(),
               /*align=*/1),
      m_OutSectData(outSectData) {}

size_t OutputSectDataFragment::size() const {
  return m_OutSectData.getDataSize();
}

eld::Expected<void> OutputSectDataFragment::emit(MemoryRegion &mr, Module &M) {
  ASSERT(paddingSize() == 0,
         "OutputSectDataFragment must not have any padding!");
  auto expVal = m_OutSectData.getExpr().evaluateAndReturnError();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expVal);
  uint64_t value = expVal.value();
  if (M.getConfig().showLinkerScriptWarnings()) {
    std::size_t dataSizeBits = 8 * size();
    // FIXME: 'size() < sizeof(value)' is redundant.
    if (size() < sizeof(value) && !llvm::isIntN(dataSizeBits, value) &&
        !llvm::isUIntN(dataSizeBits, value)) {
      M.getConfig().raise(diag::warn_output_data_truncated)
          << llvm::utohexstr(value, true) << dataSizeBits
          << m_OutSectData.getOSDKindAsStr();
    }
  }
  uint8_t *out = mr.begin() + getOffset(M.getConfig().getDiagEngine());
  // FIXME: This will probaby give incorrect result if the host system uses
  // big-endian encoding and value size (uint64_t) is greater than dataSize.
  memcpy(out, &value, size());
  return {};
}
