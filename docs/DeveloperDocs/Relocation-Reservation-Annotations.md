# Relocation Allocation and Symbol Preemption

The architecture relocation references describe **ELD's implementation**, not
just the ABI's relocation formulas. Their reservation columns describe what
scanning a live relocation in an allocatable input section requests during a
final link. A relocatable (`-r`) link does not perform these final-link
reservations. Debug/non-allocatable references do not imply a GOT or PLT entry.

## Allocation Annotations

Each cell has one compact bullet per link mode: `S`, `D`, `P`, and `H`.
All annotations are single letters; their case and position distinguish modes,
resources, and conditions. They describe this relocation's reservation, not whether another
reference already allocated an entry.

| Position | Annotation | Meaning |
|----------|------------|---------|
| Mode prefix | `S`, `D`, `P`, `H` | Static executable, dynamic executable, PIE, shared library |
| Mode override | `I` | Static PIE; replaces the preceding PIE rule, not an additional mode |
| GOT resource | `G` | Ordinary symbol-address GOT slot |
| GOT resource | `J` | PLT destination-address GOT slot; **no independent ordinary GOT slot** |
| GOT resource | `B` | Shared GOT-base fragment, no per-symbol slot |
| TLS resource | `O` | One thread-pointer-offset GOT slot |
| TLS resource | `N` | Shared module-ID GOT entry |
| TLS resource | `Q` | Two GOT slots: module ID and module-relative offset |
| TLS resource | `C` | TLS descriptor |
| PLT resource | `L` | Ordinary PLT entry |
| PLT resource | `R` | IFUNC resolver PLT entry; static resolution uses `IRELATIVE` |
| Outcome | `-` | No entry requested |
| Outcome | `U` | Uses an existing runtime/internal/TLS-sequence allocation; no new entry |
| Outcome | `X` | Unsupported input relocation or output mode; no usable allocation |
| Outcome | `E` | Link fails: static local-dynamic TLS relaxation is unimplemented |
| Condition | `p` | Preemptible=true, from the shared flowchart below |
| Symbol type | `f`, `i`, `t` | `STT_FUNC`, `STT_GNU_IFUNC`, `STT_TLS` respectively |
| Binding | `g` | Global-bound (not local-bound) target |
| Visibility/binding | `h` | Local-bound **and** hidden target; `!h` excludes only that combination |
| Relaxation | `r` | GOT bypass: `--relax`, Preemptible=false, non-IFUNC, addend `-4`, supported load/call/jump opcode |
| Modifier | `{a}` | Mixed direct/GOT IFUNC references add a GOT slot holding the canonical PLT address |
| Modifier | `{c}` | TLS-model conversion stub, not a range trampoline |
| Modifier | `{z}` | Entry belongs to the TLS resolver, not the referenced TLS symbol |
| Modifier | `{w}` | Linker warns for this IFUNC relocation despite reservation |
| Modifier | `{e}` | Static GD-to-local-exec relaxation is unimplemented |
| Modifier | `{n}` | Current handler is a no-op |

`Resource[condition]` allocates only when the condition holds; otherwise it
requests nothing. `&` means AND, `!` means NOT, `+` combines reservations,
and parentheses group conditions. With no condition, visibility and symbol
type do not restrict reservation. A `/` in the mode prefix groups modes.
An `I:` clause overrides only static PIE; other PIE links keep the main rule.

Examples:

- `S/D/P/H: G`: all four modes reserve an ordinary GOT symbol slot.
- `D/P/H: J[p&f]`: these modes reserve a PLT destination GOT slot only
  for a preemptible function; **not** an ordinary symbol GOT slot.
- `D/P/H: L[p]`: Preemptible=true creates a PLT under these link modes.
- `P: L[p]; I: L[p] + R[g&i]`: PIE uses the preemption rule; static PIE
  also handles global-bound IFUNC targets through a resolver PLT.
- `S: J[i]{a} + G[!i]`: IFUNC uses the PLT destination slot (with the
  mixed-reference exception); other targets use an ordinary GOT slot.

## Visibility and Preemption

The following shared flowchart determines `Preemptible=true` or
`Preemptible=false`. Follow the branches in order: visibility and
linker-provided-symbol exclusions precede weak-undefined handling, PIE
precedes the static-link test for imported/undefined symbols, and IFUNC
handling precedes the final version-script-scope test.

```{eval-rst}
.. d2:: ../documentation/images/SymbolPreemption.d2
   :alt: Symbol preemption decisions for visibility, binding, resolution state, link mode, and symbolic options.
```

{download}`D2 flowchart source <../documentation/images/SymbolPreemption.d2>`.
The diagram uses ELD's shared D2 styles and documentation renderer.

The chart's shared-object output branch is reached only after excluding PIE;
the weak-undefined mode test also accepts a shared-object output kind when
the link is static. Thus a static PIE and a static shared-object output can
still yield `Preemptible=true` along their indicated paths.

`Preemptible=true` alone does **not** allocate a PLT for every relocation.
The cell's mode and condition annotations specify when an entry is reserved.
`L[p&f]` requires a preemptible `STT_FUNC`; `L[p]` has no function-type test.
`R[i]` is a separate IFUNC resolver path, not an ordinary preemption condition.

An absolute data reference may create a dynamic or COPY relocation
**without** allocating a GOT or PLT entry. Conversely, an ordinary GOT
reference can allocate a slot for local/hidden symbols. Visibility and mode
then determine whether that slot contains a link-time value or needs a
runtime relocation. The TLS resource letters distinguish thread-pointer offsets,
module IDs, and descriptors from ordinary symbol-address slots.

On ARM, AArch64, and RISC-V, static IFUNC finalization may add a regular GOT
slot when the same symbol has **both direct and GOT references**. That slot
contains the canonical PLT address; GOT-only IFUNC references can use the
GOTPLT slot. The per-reference columns describe the initial reservation
paths, with the local/global binding exceptions in each architecture's notes.

Reservations are normally shared across references to a symbol. A relaxable
reference that needs no slot does not remove a slot needed by another
reference. For ARM, `T` **inside an expression** remains the Thumb-state bit;
the allocation condition `t` instead means `STT_TLS`.
Unsupported symbol/relocation combinations may warn or fail even when the
scanner allocates a slot; a reservation does not establish that the target
type is legal. ELF `STT_NOTYPE` is not equivalent to `STT_FUNC`: a condition
requiring a function target does not reserve a PLT for a notype symbol just because
it labels code.

## Trampolines

The trampoline column describes branch-island range extension or ARM ISA
interworking, not PLT allocation. A branch may need a trampoline to a local
function or to an already-created PLT entry. Symbol type alone does not
determine branch reach: displacement, addend, alignment, instruction encoding,
and (on ARM) target ISA are relevant. A relocation with no trampoline retains
its documented overflow policy; do not assume an overflowing field will be
repaired. Hexagon TLS access stubs are noted separately because they are
model-conversion/resolver stubs, not distance-triggered trampolines.

The range-stub entries assume trampoline insertion is enabled;
`--no-trampolines` suppresses the ARM, AArch64, and Hexagon insertion passes.
Unresolved targets can also be skipped by those passes (see architecture
notes). An AArch64 erratum-repair stub is separate from relocation-driven
branch range extension and is not implied by an ADRP relocation alone.

## Architecture References

- [AArch64](AArch64-Relocations.md)
- [ARM](ARM-Relocations.md)
- [Hexagon](Hexagon-Relocations.md)
- [RISC-V](RISCV-Relocations.md)
- [i386](X86_32-Relocations.md)
- [x86-64](X86_64-Relocations.md)

## Implementation Sources

- [Shared preemption and dynamic-relocation policy](../../lib/Target/GNULDBackend.cpp)
- [Reservation flags and generic scanning](../../lib/Target/Relocator.cpp)
- [Branch-island selection, including PLT targets](../../lib/BranchIsland/StubFactory.cpp)

Each architecture reference links its scanner and range-extension implementation.
