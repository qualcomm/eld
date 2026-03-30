Fragment, Section, Symbol, and Relocation Model
================================================

.. contents::
   :local:

Overview
--------
ELD is a fragment linker. The core data path is:

1. Input ``ELFSection`` objects own lists of ``Fragment`` and ``Relocation``.
2. A symbol name resolves to ``ResolveInfo``.
3. ``ResolveInfo`` points to an output symbol via ``outSymbol()``.
4. ``LDSymbol`` points to a location through ``FragmentRef``.
5. ``FragmentRef`` points to a ``Fragment`` plus an in-fragment offset.
6. ``Relocation`` uses:
   - ``targetRef()`` for place ``P`` (where relocation is applied),
   - ``symInfo()->outSymbol()->fragRef()`` for symbol ``S`` location.

Fragment as Linker IR (also called chunks)
------------------------------------------
In ELD, ``Fragment`` acts as the linker intermediate representation, similar to
how LLVM IR is the compiler middle layer between frontends and backends.

In many code reviews and discussions, fragments are also called *chunks*. In
this document, those terms are equivalent:

1. Fragment == Chunk (smallest link-time placement/data unit).
2. Section is a container of many fragments/chunks.
3. Most link-time transforms operate at fragment/chunk granularity.

Why this IR exists
------------------
A linker must combine many heterogeneous entities:

1. Raw input bytes from object files.
2. Linker-created content (stubs, GOT/PLT entries, synthetic tables).
3. Transformed content (merged strings, discarded ranges, relaxed code).

Using only section-level representation is too coarse. Using raw bytes only is
too low-level and loses semantic placement anchors. ``Fragment`` provides the
middle layer needed for precise, composable transformations.

.. graphviz::

   digraph ir_analogy {
     rankdir=LR;

     FE [label="Object/Script Inputs"];
     IR [label="Fragment/Chunk IR"];
     BE [label="Target Relocation + Writers"];

     FE -> IR [label="decode/construct"];
     IR -> BE [label="layout + relocate + emit"];
   }

Advantages of Fragments/Chunks
------------------------------
1. Fine-grained layout control:
   offsets/alignment are controlled per fragment instead of only per section.
2. Uniform handling of input and synthetic data:
   region/string fragments and linker-generated fragments share one model.
3. Better relocation precision:
   ``FragmentRef`` gives stable (fragment, offset) anchors for place and symbol.
4. Safer transformations:
   merge/split/discard operations can update fragment links without rewriting
   section-wide raw blobs.
5. Target extensibility:
   target-specific fragments (stub/got/plt/etc) fit the same pipeline.
6. Better diagnostics:
   fragment path + owning section enables precise source/target location output.

.. graphviz::

   digraph chunk_advantages {
     rankdir=TB;

     Fragment [label="{Fragment/Chunk IR|{granularity|uniformity|reloc anchors|transform safety|target hooks|diagnostics}}"];
     Layout [label="Layout/Placement"];
     Relax [label="Relaxation/Merge/Split"];
     Reloc [label="Relocation"];
     Emit [label="Writer/Emission"];

     Fragment -> Layout;
     Fragment -> Relax;
     Fragment -> Reloc;
     Fragment -> Emit;
   }

What a Fragment is
------------------
``Fragment`` is the minimum linking unit. It stores:

1. Fragment kind (region/string/stub/got/plt/etc).
2. Owning input section (`getOwningSection()`).
3. Offset/alignment inside output placement.

Fragments are stored in each ``ELFSection`` fragment list. Output placement is
derived through the owning section's output mapping.

.. graphviz::

   digraph fragment_basic {
     rankdir=TB;

     InputSection [label="{ELFSection (input)|Fragments[]|Relocations[]|OutputSectionEntry*}"];
     Fragment [label="{Fragment|Kind|OwningSection*|Offset+Alignment}"];
     OutputEntry [label="{OutputSectionEntry|getSection() to output ELFSection}"];
     OutputSection [label="{ELFSection (output)|addr|offset}"];

     InputSection -> Fragment [label="owns (list)"];
     Fragment -> InputSection [label="getOwningSection()"];
     InputSection -> OutputEntry [label="getOutputSection()"];
     OutputEntry -> OutputSection [label="getSection()"];
     Fragment -> OutputSection [label="getOutputELFSection()"];
   }

Chunking inside a section
-------------------------
A section is commonly decomposed into multiple fragments/chunks, some from
input and some linker-generated.

.. graphviz::

   digraph section_chunking {
     rankdir=LR;

     S [label="{ELFSection .text|Fragments = [F0,F1,F2,StubF3,F4]}"];
     F0 [label="{F0|Region}"];
     F1 [label="{F1|Region}"];
     F2 [label="{F2|Region}"];
     F3 [label="{F3|Stub (generated)}"];
     F4 [label="{F4|Fill/String/etc}"];

     S -> F0;
     S -> F1;
     S -> F2;
     S -> F3;
     S -> F4;
   }

What a Section is
-----------------
``Section`` is the generic abstraction. ``ELFSection`` extends it with ELF
header attributes and fragment/relocation containers.

Conceptually:

1. Input sections carry content, fragments, relocations, and script matching.
2. Output sections carry final address/offset and segment linkage.
3. Input-to-output mapping is represented by ``OutputSectionEntry``.

.. graphviz::

   digraph section_model {
     rankdir=TB;

     Section [label="{Section|name|size|InputFile*|OutputSectionEntry*}"];
     ELFSection [label="{ELFSection : Section|Type/Flags/Align|Fragments[]|Relocations[]|addr/offset}"];
     OutputSectionEntry [label="{OutputSectionEntry|name|rules|OutputELFSection*|LoadSegment*}"];
     OutputELFSection [label="{ELFSection (output)|addr|offset|kind}"];

     Section -> OutputSectionEntry [label="getOutputSection()"];
     OutputSectionEntry -> OutputELFSection [label="getSection()"];
     ELFSection -> Section [arrowhead=empty, label="inherits"];
   }

How symbols connect with fragments using FragRef and outSymbol
---------------------------------------------------------------
``ResolveInfo`` is the symbol-resolution record for a name. The key pointer is
``ResolveInfo::OutputSymbol`` exposed as ``outSymbol()``.

That output symbol is ``LDSymbol``. ``LDSymbol`` carries ``FragmentRef*`` which
anchors the symbol to a fragment and offset.

.. graphviz::

   digraph symbol_fragment_model {
     rankdir=LR;

     ResolveInfo [label="{ResolveInfo|name/type/binding/desc|OutputSymbol*|value/size}"];
     LDSymbol [label="{LDSymbol|ResolveInfo*|FragmentRef*|value/size/shndx}"];
     FragmentRef [label="{FragmentRef|Fragment*|offset}"];
     Fragment [label="{Fragment|Kind|OwningSection*|offset}"];
     InputSection [label="{ELFSection (input)}"];
     OutputSection [label="{ELFSection (output)}"];

     ResolveInfo -> LDSymbol [label="outSymbol()"];
     LDSymbol -> FragmentRef [label="fragRef()"];
     FragmentRef -> Fragment [label="frag()"];
     Fragment -> InputSection [label="getOwningSection()"];
     FragmentRef -> OutputSection [label="getOutputELFSection()"];
   }

Relocation connection graph
---------------------------
Relocation computation combines both the place and symbol chains:

1. ``targetRef()`` -> place fragment/offset (``P``).
2. ``symInfo()`` -> ``ResolveInfo`` -> ``outSymbol()`` -> ``fragRef()`` (``S``).

.. graphviz::

   digraph reloc_graph {
     rankdir=TB;

     Reloc [label="Relocation\ntype/addend/targetData\ntargetRef()\nsymInfo()"];
     PlaceRef [label="FragmentRef (place)\nfragment + offset"];
     SymInfo [label="ResolveInfo\noutSymbol()"];
     OutSym [label="LDSymbol\nfragRef() + value"];
     SymRef [label="FragmentRef (symbol)\nfragment + offset"];
     PlaceFrag [label="Fragment\n(place)"];
     SymFrag [label="Fragment\n(symbol)"];

     Reloc -> PlaceRef [label="targetRef()\nplace P"];
     PlaceRef -> PlaceFrag [label="frag()"];
     Reloc -> SymInfo [label="symInfo()"];
     SymInfo -> OutSym [label="outSymbol()"];
     OutSym -> SymRef [label="fragRef()\nsymbol S"];
     SymRef -> SymFrag [label="frag()"];
   }

Lifecycle view
--------------
High-level lifecycle from inputs to final addresses:

.. graphviz::

   digraph lifecycle {
     Parse [label="Parse inputs"];
     Build [label="Build sections + fragments/chunks"];
     Resolve [label="ResolveInfo <-> outSymbol"];
     Attach [label="Attach LDSymbol.fragRef"];
     Layout [label="Assign fragment offsets"];
     Reloc [label="Apply relocations via FragmentRef"];
     Emit [label="Emit output sections"];

     Parse -> Build -> Resolve -> Attach -> Layout -> Reloc -> Emit;
   }

Building the ELD IR
-------------------
This section describes how ``IRBuilder`` (``lib/SymbolResolver/IRBuilder.cpp``)
constructs ELD's internal linker IR from input files.

At a high level, IR construction means:

1. Create/resolve ``ResolveInfo`` entries for input names.
2. Create ``LDSymbol`` objects and connect them to ``ResolveInfo``.
3. Attach location anchors via ``FragmentRef`` when a symbol has concrete
   section-backed storage.
4. Build relocations with fragment-based place references and symbol references.

The result is a graph where every later link stage (layout, GC, relaxation,
relocation, emission) works on stable IR entities instead of raw input bytes.

Core symbol ingestion paths
^^^^^^^^^^^^^^^^^^^^^^^^^^^
``IRBuilder::addSymbol(...)`` dispatches based on input kind:

1. Object-like inputs:
   ``addSymbolFromObject(...)``.
2. Dynamic objects:
   ``addSymbolFromDynObj(...)``.

Both paths create input symbols, resolve through ``NamePool``/resolver logic,
and then establish output-symbol links (`outSymbol`) that the rest of the
linker consumes.

How ``FragmentRef`` is chosen during IR build
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
IRBuilder assigns symbol location anchors conservatively.

1. ``FragmentRef::null()`` for symbols without concrete fragment placement,
   including undefined/common/absolute cases.
2. ``FragmentRef::discard()`` for symbols in ignored/discarded sections.
3. Concrete ``FragmentRef(fragment, offset)`` for regular section-backed
   definitions.
4. Merge-string special case:
   symbols are anchored to merge-string fragments with fragment-relative offset.

This explains why downstream code frequently checks ``hasFragRef()`` before
following symbol-to-fragment links.

Local symbols
^^^^^^^^^^^^^
Local symbols are handled as non-exported, file-scoped IR entities:

1. They are inserted via ``NamePool::insertLocalSymbol``.
2. IRBuilder sets ``ResolveInfo::outSymbol`` directly to the local input symbol.
3. If section-backed, they still carry ``FragmentRef`` and can participate in
   relocation computations in that object's context.
4. They do not participate in global symbol preemption/override semantics.

Object-file (regular) symbols
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
For regular object symbols (non-local path):

1. IRBuilder builds an input ``ResolveInfo`` candidate and input ``LDSymbol``.
2. Resolver decides whether the new definition/reference overrides existing
   resolved state.
3. ``ResolvedResult.Info->outSymbol()`` is updated when required, including
   override by the new object symbol and cases where a corresponding
   shared-library symbol remains the output symbol for unresolved dynamic
   references.
4. Value/finality is updated according to override state and symbol class.

This is where most object-vs-object and object-vs-dynamic symbol arbitration is
materialized into concrete IR links.

Dynamic symbols
^^^^^^^^^^^^^^^
Dynamic object symbol ingestion has important filtering and resolution behavior:

1. Section symbols are ignored in this path.
2. Local/hidden/internal dynamic symbols are skipped.
3. Non-local dynamic symbols are resolved through the same central name pool.
4. ``outSymbol`` may be redirected when a dynamic symbol wins resolution.
5. Needed/used dynamic library state is updated based on resolution outcome.
6. Export-to-dyn decisions are derived from output form and symbol properties.

In short, dynamic symbol handling is not separate metadata; it is integrated
into the same IR graph with specific policy checks.

``outSymbol`` invariants after IR construction
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
After symbol ingestion/resolution, practical invariants are:

1. Resolved global names have a canonical ``ResolveInfo``.
2. That canonical record points to one output symbol via ``outSymbol``.
3. That output symbol may or may not have ``FragmentRef`` depending on symbol
   kind/state.
4. Relocation and backend code consume ``symInfo()->outSymbol()`` as the
   canonical symbol entry point.

Relocation IR construction in IRBuilder
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
IRBuilder relocation creation uses fragment-oriented place anchors:

1. Place ``P`` is captured as a ``FragmentRef``:
   - section-front fragment + offset, or
   - explicit fragment + offset overload.
2. ``Relocation::setSymInfo(...)`` is always set to resolved symbol info.
3. Section-undefined corner cases are filtered before relocation creation.

This guarantees relocation math later can derive both the place and symbol
paths through fragment-based references.

.. graphviz::

   digraph irbuilder_symbol_flow {
     rankdir=TB;

     ReadSym [label="Read symbol from input"];
     ChooseRef [label="Choose FragmentRef\n(null/discard/concrete)"];
     CreateIn [label="Create input LDSymbol"];
     Resolve [label="Insert/resolve in NamePool"];
     SetOut [label="Update ResolveInfo.outSymbol()"];
     Attach [label="Input/Output symbols carry fragRef"];

     ReadSym -> ChooseRef;
     ChooseRef -> CreateIn;
     CreateIn -> Resolve;
     Resolve -> SetOut;
     SetOut -> Attach;
   }

Practical notes
---------------
1. ``outSymbol`` is not a separate class; it is the ``ResolveInfo::OutputSymbol``
   pointer to ``LDSymbol``.
2. ``LDSymbol::hasFragRef()`` is the guard before dereferencing symbol location
   through fragments.
3. ``FragmentRef::null()`` and ``FragmentRef::discard()`` represent special
   non-location states.
4. For final addresses, relocation code uses output section address plus
   fragment output offset.
