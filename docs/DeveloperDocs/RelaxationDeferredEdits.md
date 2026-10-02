# Relaxation via Deferred Edits

## Background

RISC-V linker relaxation shrinks code: `AUIPC+JALR` collapses to `JAL` or
`C.J`, `HI20+LO12` collapses to a single GP-relative instruction, and
`R_RISCV_ALIGN` padding shrinks once the final alignment is known. Each of
these deletes bytes from a `RegionFragmentEx`, which changes section size and
moves every symbol after it — which can invalidate a relaxation decision
that was already made.

`GNULDBackend::relax()` loops until a fixed point:

```
while (!finished) {
  converge layout (createProgramHdrs, up to 4 iterations)
  preRelaxation()
  mayBeRelax(iteration, finished)
}
```

`RISCVLDBackend::mayBeRelax()` interprets `iteration` as a pass index:
`RELAXATION_CALL`, `RELAXATION_PC`, `RELAXATION_LUI`, `RELAXATION_TLSDESC`,
`RELAXATION_ALIGN` (last). Because ALIGN runs last and can still shrink
sections, a call or GP relaxation decided in an earlier pass can be
invalidated by the time the loop ends. If that happens with eager mutation,
the linker emits an instruction whose relaxed encoding can't reach its
target — a silent miscompile, not a diagnostic.

## Design

Relaxations are not applied as they are decided. Each is **recorded** in a
per-fragment `RelaxPlan` and the whole set is applied once, in
`RegionFragmentEx::commitRelaxEdits()`, after the fixed point is reached and
re-verified. A decision that no longer fits is simply dropped from the plan
— since nothing was ever written to the buffer or to the `Relocation`, there
is nothing to restore.

This follows an existing convention: `FragmentRef` already distinguishes an
input-relative offset from an output-relative offset (see `EhFramePiece`,
`MergeStringFragment`). `RegionFragmentEx` becomes a third instance of the
same pattern — offsets stay relative to the original, pre-relaxation input
until commit, and `FragmentRef::getOutputOffset()` maps through the pending
plan in the meantime.

## Data Model

`include/eld/Fragment/RelaxPlan.h`:

```cpp
enum class RelaxKind { CallToJal, CallToCJ, LuiGp, PcGp };

struct RelaxEdit {
  Relocation *Reloc = nullptr;
  uint32_t OrigOffset = 0;

  uint32_t DeleteOffset = 0; // 0 DeleteBytes == pure rewrite, no deletion
  uint32_t DeleteBytes = 0;
  uint32_t NopBytes = 0; // ALIGN only

  std::optional<Relocation::Type> NewType;
  std::optional<Relocation::DWord> NewInstr;
  uint8_t NewInstrSize = 0;
  std::optional<Relocation::Address> NewAddend;
};

class RelaxPlan {
public:
  void addEdit(RelaxEdit E);
  void removeEdit(const Relocation *R);
  void clear();

  uint32_t shiftAt(uint32_t OrigOff) const; // sum of DeleteBytes before Off
  uint32_t totalShift() const;

  RelaxEdit *find(const Relocation *R);
  llvm::ArrayRef<RelaxEdit> edits() const;
  bool empty() const;

private:
  std::vector<RelaxEdit> Edits; // sorted by DeleteOffset
  std::vector<uint32_t> PrefixShift;
  llvm::DenseMap<const Relocation *, unsigned> ByReloc; // O(1) find/remove
  uint32_t TotalShift = 0;
};
```

`RelaxKind` covers the two relaxation families that get re-verified after
the fact (call relaxation, GP-relative relaxation); it lives on the
backend's own tracking records (`CallRelaxRecord`, `GPRelaxRecord`), not on
`RelaxEdit` itself, since nothing generic needs to ask an edit what kind it
is. `RelaxPlan` is an ordinarily mutable value type — no `mutable` keyword,
no immutable rebuild-from-scratch; `addEdit`/`removeEdit` update `Edits` and
incrementally rebuild `PrefixShift`/`ByReloc`.

`RegionFragmentEx` owns one `RelaxPlan Plan;` by value:

```cpp
class RegionFragmentEx : public Fragment {
public:
  void recordDelete(Relocation *Reloc, uint32_t DeleteOffset,
                    uint32_t DeleteBytes, uint32_t NopBytes = 0);
  void attachDecision(Relocation *Reloc, Relocation::Type NewType,
                      std::optional<Relocation::DWord> NewInstr = std::nullopt,
                      uint32_t InstrOffset = 0, uint8_t NewInstrSize = 0);
  void recordRewrite(Relocation *Reloc, uint32_t InstrOffset,
                     Relocation::Type NewType, Relocation::DWord NewInstr,
                     uint8_t NewInstrSize,
                     std::optional<Relocation::Address> NewAddend =
                         std::nullopt);
  void dropEdit(const Relocation *Reloc);
  RelaxEdit *pendingEdit(const Relocation *Reloc);
  uint32_t mapOffset(uint32_t OrigOff) const; // OrigOff - Plan.shiftAt(OrigOff)
  size_t size() const override; // Size - Plan.totalShift()
  void commitRelaxEdits();

private:
  RelaxPlan Plan;
};
```

`recordDelete`/`attachDecision` cover call relaxation (whole instruction
deleted, or deleted-and-replaced) — `recordDelete` records the deletion,
and `attachDecision` attaches the relocation-type change and, if the
instruction is being rewritten rather than fully deleted, its new encoding.
`recordRewrite` covers a GP-relative LO12 relocation, whose instruction is
rewritten in place (base register changed to `gp`) with no deletion
(`DeleteBytes = 0`, which the shift/compaction math already treats as a
no-op).

`size()` is already `virtual` and already consulted by layout, so the
`while (!finished)` loop in `relax()` converges on pending deletions with no
further change. `FragmentRef::getOutputOffset()` routes a `RegionFragmentEx`
offset through `mapOffset()` plus the fragment's own base offset, so every
address computation downstream (`Relocation::place()`, ALIGN's target
computation, the range checks below) is correct with no local change.

`RISCVLDBackend::relaxDeleteBytes()`/`reportMissedRelaxation()` are the
helpers that most relaxation call sites go through to reach `recordDelete()`
and raise the corresponding verbose diagnostic. Both take the symbol as a
`const ResolveInfo *` rather than a pre-extracted name, so the diagnostic
can print `getDecoratedName(shouldDemangle())` — a `nullptr` (the ALIGN
case, which has no symbol) prints an empty name.

### Two things worth knowing

**`Relocation::target()` is a separate cache from the fragment buffer.** It
reads `m_TargetData`, cached on the `Relocation` at parse time; the final
relocation-apply pass (`RISCVRelocator.cpp`) reads and rewrites
`pReloc.target()` directly, not the fragment's bytes. `commitRelaxEdits()`
must call `Reloc->setTargetData(*E.NewInstr)` alongside the `memcpy`, or the
final apply pass ORs its immediate into a stale opcode.

**Dropping an edit after `relax()`'s own loop exits needs an explicit
layout re-convergence.** `Fragment::size()` being plan-aware is what lets
`relax()`'s loop converge correctly *during* the loop — each iteration's
`createProgramHdrs()` runs inside it. The settle step below runs *after*
that loop exits; when it drops an edit, `size()` changes immediately, but
ELF section headers and program headers were already finalized against the
pre-drop size and don't re-derive automatically. `createProgramHdrs()` must
run again before ALIGN re-decides, and once more before the final commit.

## The Settle Loop

`GNULDBackend::postRelax(bool &pFinished)` is called once after `relax()`'s
`while (!finished)` loop exits, and repeatedly while `pFinished` is false —
mirroring `mayBeRelax`. `GNULDBackend::relax()` owns the loop and the
`createProgramHdrs()` calls between rounds; `RISCVLDBackend::postRelax()`
only decides whether another round is needed:

```cpp
void RISCVLDBackend::postRelax(bool &pFinished) {
  pFinished = true;
  if (m_PostRelaxNeedsAlignRedecision) {
    bool Dummy;
    mayBeRelax(RELAXATION_ALIGN, Dummy);
    m_PostRelaxNeedsAlignRedecision = false;
  }

  bool AnyDropped = false;
  if (m_PostRelaxRounds < MaxRounds) {
    ++m_PostRelaxRounds;
    AnyDropped = verifyAndRollbackCallRelaxations();
    AnyDropped |= verifyAndRollbackGPRelaxations();
  }
  if (AnyDropped) {
    undoAlignRelaxations();
    m_PostRelaxNeedsAlignRedecision = true;
    m_PostRelaxLayoutChanged = true;
    pFinished = false;
    return;
  }

  if (m_PostRelaxLayoutChanged) { // ALIGN's last decision still needs a sync
    m_PostRelaxLayoutChanged = false;
    pFinished = false;
    return;
  }

  for (RegionFragmentEx *Frag : m_FragmentsWithPendingRelaxEdits)
    Frag->commitRelaxEdits();
  m_FragmentsWithPendingRelaxEdits.clear();
}
```

`m_CallRelaxRecords`/`m_GPRelaxRecords` are the only "which decisions are
still live" state; a record is erased the moment it's dropped, and dropped
records are never reconsidered (sticky removal — see Termination).

`verifyAndRollbackCallRelaxations()` recomputes `S+A-P` from the relocation
and checks `isInt<12>` (`C.J`) or `isInt<21>` (`JAL`).
`verifyAndRollbackGPRelaxations()` recomputes `S+A` from the record's
`HIReloc` (the GP-relative pair's real symbol/addend source — for the
PC-relative form the LO relocation's own addend is 0, so `HIReloc` resolves
to the paired HI relocation via `getBaseReloc()`; for the plain form
`HIReloc` is the relocation itself) and checks `fitsInGP<12>`. Either
function drops via `Region->dropEdit(...)` and raises a verbose diagnostic
(`Diag::relax_call_rolled_back` / `Diag::relax_gp_rolled_back`).

`undoAlignRelaxations()` reverses every deferred ALIGN edit in
`m_AppliedAlignRelocs` (drop + reset type to `R_RISCV_ALIGN`) so
`mayBeRelax(RELAXATION_ALIGN, ...)` can decide it again against fresh
addresses, raising `Diag::relax_align_undone`.

## `commitRelaxEdits()`

```cpp
void RegionFragmentEx::commitRelaxEdits() {
  if (Plan.empty()) return;

  // Encode before compacting, while OrigOffset still addresses the
  // untouched buffer. Relocation::target() is a separate cache, so it
  // needs updating too.
  for (const RelaxEdit &E : Plan.edits())
    if (E.NewInstr) {
      memcpy(Data + E.OrigOffset, &*E.NewInstr, E.NewInstrSize);
      E.Reloc->setTargetData(*E.NewInstr);
    }

  size_t OrigSize = Size;
  // Fix up relocation and symbol offsets/sizes via Plan.shiftAt().

  // One forward walk instead of one memmove per edit.
  uint32_t Dst = 0, Src = 0;
  for (const RelaxEdit &E : Plan.edits()) {
    memmove(Data + Dst, Data + Src, E.DeleteOffset - Src);
    Dst += E.DeleteOffset - Src;
    Src = E.DeleteOffset + E.DeleteBytes;
  }
  memmove(Data + Dst, Data + Src, OrigSize - Src);
  Size = OrigSize - Plan.totalShift();

  for (const RelaxEdit &E : Plan.edits()) {
    if (E.NewType) E.Reloc->setType(*E.NewType);
    if (E.NewAddend) E.Reloc->setAddend(*E.NewAddend);
  }
  Plan.clear();
}
```

## Termination

Dropping an edit grows a section, which can move another edit's target
either closer to or further from it — re-evaluation is not monotone, and a
naive loop can oscillate. Two guards:

1. **Sticky removal.** A relaxation that fails its fitness check is erased
   from its tracking vector and never reconsidered, so the number of
   still-droppable records strictly decreases every round that drops
   anything.
2. **`MaxRounds = 8`** as a backstop.

The cost of sticky removal: a relaxation that would fit again after some
*other* relaxation was dropped stays dropped anyway, so output can be
marginally larger than a perfect fixed point.
