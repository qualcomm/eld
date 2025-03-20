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

OutputSectDataFragment::OutputSectDataFragment(OutputSectData &OutSectData)
    : Fragment(Fragment::Type::OutputSectDataFragType,
               OutSectData.getELFSection(),
               /*align=*/1),
      OutSectData(OutSectData) {}

size_t OutputSectDataFragment::size() const {
  return OutSectData.getDataSize();
}

eld::Expected<void> OutputSectDataFragment::emit(MemoryRegion &Mr, Module &M) {
  ASSERT(paddingSize() == 0,
         "OutputSectDataFragment must not have any padding!");
  auto ExpVal = OutSectData.getExpr().evaluateAndReturnError();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(ExpVal);
  uint64_t Value = ExpVal.value();
  if (M.getConfig().showLinkerScriptWarnings()) {
    std::size_t DataSizeBits = 8 * size();
    // FIXME: 'size() < sizeof(value)' is redundant.
    if (size() < sizeof(Value) && !llvm::isIntN(DataSizeBits, Value) &&
        !llvm::isUIntN(DataSizeBits, Value)) {
      M.getConfig().raise(Diag::warn_output_data_truncated)
          << llvm::utohexstr(Value, true) << DataSizeBits
          << OutSectData.getOSDKindAsStr();
    }
  }
  uint8_t *Out = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  // FIXME: This will probaby give incorrect result if the host system uses
  // big-endian encoding and value size (uint64_t) is greater than dataSize.
  memcpy(Out, &Value, size());
  return {};
}
