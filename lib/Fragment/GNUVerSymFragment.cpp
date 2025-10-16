#include "eld/Fragment/GNUVerSymFragment.h"
#include "eld/Core/Module.h"

using namespace eld;

GNUVerSymFragment::GNUVerSymFragment(ELFSection *S,
                                     const std::vector<ResolveInfo *> &DynSyms)
    : Fragment(Fragment::Type::GNUVerSym, S), DynamicSymbols(DynSyms) {}

size_t GNUVerSymFragment::size() const {
  return DynamicSymbols.size() * 2;
}

eld::Expected<void> GNUVerSymFragment::emit(MemoryRegion &MR, Module &M) {
  uint8_t *Buf = MR.begin() + getOffset(M.getConfig().getDiagEngine());
  for (const auto *DynSym : DynamicSymbols) {
    uint16_t VerNum = DynSym->getSymbolVersionID();
    // Note: default versions are not hidden; other versions may set VERSYM_HIDDEN.
    *reinterpret_cast<uint16_t *>(Buf) = VerNum;
    Buf += 2;
  }
  return {};
}
