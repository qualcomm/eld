# Relaxation via Deferred Edits

## Background

RISC-V linker relaxation shrinks code: `AUIPC+JALR` can collapse to `JAL` or
`C.J`, `HI20+LO12` can become a GP-relative instruction, and `R_RISCV_ALIGN`
padding can shrink once final alignment is known. Each change can delete bytes
from a `RegionFragmentEx`, changing section size and moving every symbol after
the edit.

Those moves can invalidate a relaxation decision that was made earlier in the
relaxation process. Applying the decision eagerly would require restoring
bytes, relocation metadata, and cached instruction data whenever a later
decision changed the layout.

## Deferred Design

Relaxations are recorded in a per-fragment `RelaxPlan` instead of being applied
immediately. The plan is committed once the relaxation and post-relaxation
verification phases are complete, through
`RegionFragmentEx::commitRelaxEdits()`.

While the plan is pending, offsets remain relative to the original,
pre-relaxation fragment contents. `RegionFragmentEx::size()` reports the
original size minus pending deletions, and `RegionFragmentEx::mapOffset()` maps
an original offset to its current output-relative offset. This lets layout,
symbol, relocation, and alignment calculations observe pending deletions
without modifying the fragment buffer.

If a decision no longer fits, its edit is removed from the plan. Since deferred
edits do not modify the buffer or relocation state when recorded, dropping an
edit does not require rollback of already-applied bytes.

### Example: mapping an offset

Assume a fragment starts with 32 bytes and has two pending deletions:

| Edit | Original range | Bytes removed |
| --- | --- | ---: |
| A | 4 through 7 | 4 |
| B | 16 through 17 | 2 |

The fragment still contains all 32 original bytes, but its reported pending
size is 26 bytes. An original offset of 3 maps to 3 because it is before both
deletions. Offset 10 maps to 6 because edit A removes four earlier bytes.
Offset 24 maps to 18 because both edits remove six earlier bytes.

The important detail is that all three calculations use original offsets. No
caller needs to guess whether an earlier relaxation has already changed the
buffer.

An in-place rewrite is different: if edit C rewrites bytes at offset 20 and
removes zero bytes, the fragment remains 26 bytes and offsets do not shift.
The instruction rewrite and relocation changes are still deferred until
commit.

## RelaxEdit

Each `RelaxEdit` describes one deferred change:

- `Reloc` identifies the relocation associated with the edit.
- `OrigOffset` identifies the bytes to rewrite before compaction.
- `DeleteOffset` and `DeleteBytes` describe the original-buffer range to
  remove.
- `NopBytes` records alignment padding information for diagnostics.
- `NewType`, `NewInstr`, `NewInstrSize`, and `NewAddend` describe relocation or
  instruction changes applied during commit.
- `Active` identifies an edit that is still eligible for application.

An edit with zero `DeleteBytes` is an in-place rewrite. It does not contribute
to the fragment's shift or compaction size, but it can still update the
instruction and relocation metadata at commit time.

## RelaxPlan Storage

`RelaxPlan` uses a `DenseMap` keyed by `Relocation *` as its authoritative
storage. This gives average constant-time insertion, removal, and lookup by
relocation. The map also enforces the plan's one-edit-per-relocation model.

The map is not ordered, so ordered operations use a lazily rebuilt derived
cache:

- `OrderedEdits` contains active edits sorted by `DeleteOffset`.
- `PrefixShift` stores the shift before each ordered edit.
- `DerivedValid` records whether those arrays match the map contents.
- `TotalShift` is maintained directly and is available in constant time.

Mutating the plan invalidates the derived cache. Calling `edits()` rebuilds it
when necessary and returns the sorted active edits. The method is intentionally
non-const because cache materialization is an explicit mutation; the data
structure does not rely on `mutable` state.

`find()` returns the map entry so callers can attach an instruction or
relocation decision after first recording a deletion. Because callers can
modify the returned edit, `find()` also invalidates the derived cache.

### Example: dirty and prepared states

Suppose the map contains edits at offsets 16, 4, and 28, in that insertion
order. The map does not promise an order, so it is not used directly for
compaction. When the plan is dirty, the sorted cache is rebuilt as offsets 4,
16, and 28, and the prefix shifts are computed in that order.

Adding an edit at offset 10 only updates the map and marks the cache dirty. It
does not move the existing cache entries or shift a vector of edits. The next
call to `edits()` rebuilds the ordered list as 4, 10, 16, and 28. Further
ordered queries reuse that result until the next plan mutation.

The commit path calls `edits()` before mapping relocation and symbol offsets.
That prepares the ordered view before the commit-time offset and compaction
passes. The same prepared view is reused by the remaining ordered operations.

## Recording Edits

`RegionFragmentEx` provides the operation-specific interface used by the RISC-V
backend:

- `recordDelete()` records a deletion, including optional alignment padding.
- `attachDecision()` adds the final relocation type and optional replacement
  instruction to an existing deletion edit.
- `recordRewrite()` records an in-place instruction rewrite with no deletion.
- `dropEdit()` removes a pending edit when verification rejects it.
- `pendingEdit()` provides access to an edit that needs a later decision.

The backend's relaxation records retain the relaxation-specific information,
such as whether a decision came from call or GP relaxation. `RelaxEdit` remains
generic and only describes the concrete buffer and relocation changes.

The deferred-edit framework is intended to support every relaxation that can
be represented as an instruction rewrite, relocation update, or deletion. In
the current implementation, call and GP relaxations are the only relaxation
decisions integrated with the post-relaxation verification and rollback flow.
Other relaxation types can use the same `RelaxPlan` representation when their
decision and re-verification paths are added.

## Commit Ordering

`commitRelaxEdits()` applies edits in the following order:

1. Encode replacement instructions while `OrigOffset` still refers to the
   untouched buffer.
2. Update each replacement relocation's target-data cache as well as the
   fragment bytes. `Relocation::target()` is separate from the fragment buffer
   and must not retain the old instruction encoding.
3. Map relocation and symbol offsets through the prepared plan. Symbol sizes
   are reduced for deletions that fall inside their original ranges.
4. Compact the fragment with one forward walk over sorted delete ranges.
5. Apply deferred relocation type and addend changes.
6. Clear the plan.

No edit should be applied directly to the fragment before this commit step.

### Example: compaction

Consider original bytes divided into these regions:

| Original region | Action |
| --- | --- |
| 0 through 3 | Copy to the output |
| 4 through 7 | Skip because edit A deletes it |
| 8 through 15 | Copy immediately after the first region |
| 16 through 17 | Skip because edit B deletes it |
| 18 through the end | Copy after the second deletion |

The forward compaction walk performs those copies in sorted edit order. It
does not perform one independent move for every deletion, and it never needs
to interpret already-compacted offsets as original offsets.

## Relaxation and Post-Relaxation Verification

The normal relaxation loop repeatedly converges layout and evaluates the
backend's relaxation phases. Other relaxation types may also change section
sizes after an earlier decision was recorded; the deferred-edit framework is
designed to keep those changes isolated until commit.

The post-relaxation verification phase recomputes the relevant range
conditions against the final layout. In the current implementation, a failed
call or GP decision is removed from its fragment's plan and from the backend's
corresponding tracking list. If removing an edit changes layout, dependent
decisions are re-evaluated against the new addresses before another
verification round.

### Example: rejecting a call relaxation

Assume the initial relaxation passes record two edits in one fragment:

| Decision | Original range | Effect |
| --- | --- | --- |
| Call relaxation | 40 through 43 | Delete four bytes and use a shorter call instruction |
| Alignment relaxation | 80 through 81 | Delete two padding bytes |

The final layout is then used to verify the shortened call. Suppose the call's
target is now too far away for the selected encoding. The verifier drops the
call edit and removes its call-relaxation record. The fragment immediately
becomes four bytes larger because the call deletion is no longer pending.

That size change moves everything after offset 40, including the alignment
site. The previously selected alignment result is therefore no longer trusted:
the alignment edit is undone, layout is converged again, and alignment is
re-evaluated using the new addresses. A later verification round checks the
remaining call or GP decisions against that updated layout.

If no further decision is rejected, the plan is committed. If another decision
fails, the same drop, reconvergence, and verification sequence repeats. A
rejected decision is sticky and is not reconsidered in later rounds.

The settle process must re-run layout convergence after dropping edits. A
plan-aware `size()` changes immediately, but section and program-header data
that was derived from the previous sizes does not update automatically.

The process uses sticky removal: once a verification pass rejects a relaxation,
that relaxation is not reconsidered. This guarantees progress when dropping
one edit causes another decision to move in and out of range. A bounded maximum
number of post-relaxation rounds remains as a safety backstop.

## Alignment and Overlap Rules

Delete ranges are expressed in original-buffer coordinates. Active delete
ranges must not overlap. In debug builds, `addEdit()` checks this invariant
against the edits currently stored in the map. `wouldOverlap()` is used by
alignment relaxation to reject duplicate or intersecting padding deletions.

For example, an existing deletion of original bytes 100 through 107 overlaps
an attempted deletion beginning at 104, but it does not overlap an attempted
deletion beginning at 108. Two zero-byte rewrites at the same offset do not
consume a range and therefore do not create a deletion overlap.

When the derived cache is prepared, overlap checks use the sorted neighboring
ranges. Before preparation, they conservatively scan the map. This preserves
the simple mutation path while retaining ordered checks for the commit phase.
