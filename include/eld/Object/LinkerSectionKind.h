//===- LinkerSectionKind.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_OBJECT_LINKERSECTIONKIND_H
#define ELD_OBJECT_LINKERSECTIONKIND_H

#include <cstdint>

namespace eld {

enum class LinkerSectionKind : uint8_t {
  Common,
  Debug,
  Discard,
  DynamicRelocation,
  EhFrame,
  EhFrameHdr,
  Error,
  Exclude,
  GCCExceptTable,
  GNUProperty,
  Group,
  Ignore,
  Internal,
  LinkOnce,
  MergeStr,
  MetaData,
  NamePool,
  Note,
  Null,
  OutputSectData,
  Regular,
  Relocation,
  SFrame,
  StackNote,
#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  SymbolVersion,
#endif
  Target,
  Timing,
  Version,
};

} // namespace eld

#endif
