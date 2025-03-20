//===- BuildIDFragment.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_BUILDIDFRAGMENT_H
#define ELD_FRAGMENT_BUILDIDFRAGMENT_H

#include "eld/Fragment/Fragment.h"

namespace eld {

class LinkerConfig;

/** \class BuildIDFragment
 *  \brief BuildIDFragment is a kind of Fragment contains the BuildID
 *  of the program, that can be used to uniquely identify this program
 */
class BuildIDFragment : public Fragment {
public:
  enum BuildIDKind : uint8_t { NONE, FAST, MD5, UUID, SHA1, HEXSTRING };

  BuildIDFragment(ELFSection *O = nullptr);

  ~BuildIDFragment();

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::BuildID;
  }

  size_t size() const override;

  eld::Expected<void> setBuildIDStyle(const LinkerConfig &Config);

  void dump(llvm::raw_ostream &OS) override;

  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  eld::Expected<void> finalizeBuildID(uint8_t *BufferStart, size_t BufSize);

private:
  size_t getHashSize() const;

private:
  BuildIDKind BuildIdKind = NONE;
  std::string BuildIdString;
};

} // namespace eld

#endif
