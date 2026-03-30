#############################################
Adding Command-Line Options in ELD
#############################################

This guide describes the current workflow for adding a new linker option in ELD.
It is intended for community contributors working in the driver layer.

Overview
========

ELD command-line parsing is generated from TableGen option definitions.

1. Option syntax is declared in ``include/eld/Driver/*LinkerOptions.td``.
2. TableGen generates ``*LinkerOptions.inc``.
3. Each driver parses and consumes options in ``lib/LinkerWrapper/*LinkDriver.cpp``.
4. Shared behavior is typically implemented in ``lib/LinkerWrapper/GnuLdDriver.cpp``.

Where to add an option
======================

Choose the narrowest scope first:

1. Common across all GNU-style drivers:
   Add to ``include/eld/Driver/GnuLinkerOptions.td`` and handle in
   ``lib/LinkerWrapper/GnuLdDriver.cpp``.
2. Target specific:
   Add to one target file and handle in that target driver.
   Current target option files:

   * ``include/eld/Driver/ARMLinkerOptions.td``
   * ``include/eld/Driver/HexagonLinkerOptions.td``
   * ``include/eld/Driver/RISCVLinkerOptions.td``
   * ``include/eld/Driver/x86_64LinkerOptions.td``

Important: Target ``*.td`` files include ``GnuLinkerOptions.td``. A GNU option
change propagates to all target drivers after regeneration.

Step-by-step workflow
=====================

1. Define the option in a ``.td`` file
--------------------------------------

Add a new ``def``/``defm`` entry with:

* option kind (for example ``Flag``, ``Separate``, ``Joined``, ``JoinedOrSeparate``),
* prefixes (for example ``["--"]`` or ``["-", "--"]``),
* help text,
* group (for help output categorization),
* metavariable when the option takes a value.

Use existing helper multiclasses in ``GnuLinkerOptions.td`` (for example
``smDash`` or ``mDashEq``) when they match desired syntax and aliases.

2. Consume the parsed option in driver code
-------------------------------------------

Use generated option IDs from the target opt table enum (``T::your_option`` in
templated paths, or ``OPT_<Target>OptTable::your_option`` in target-local parse).

Common access patterns:

* Boolean presence: ``Args.hasArg(T::foo)``
* Last occurrence with value: ``Args.getLastArg(T::foo)``
* Multiple occurrences: ``Args.filtered(T::foo)``
* Positive/negative pair: ``Args.hasFlag(T::enable_foo, T::disable_foo, default)``

Then map parsed values into ``Config.options()`` and diagnostics.

3. Choose parse phase carefully
-------------------------------

Current target link flow is:

1. ``parseOptions(...)`` (target file) for early-return and target-local setup.
2. ``processLLVMOptions<T>(...)``.
3. ``processTargetOptions<T>(...)``.
4. ``processOptions<T>(...)`` (shared GNU options in ``GnuLdDriver``).
5. ``checkOptions<T>(...)``.
6. ``overrideOptions<T>(...)``.

Put behavior where ordering is correct. Example: a validation that depends on
other options belongs in ``checkOptions``.

4. Add tests
------------

Add or extend tests under ``test/``. For command-line behavior, first look at:

* ``test/Common/standalone/CommandLine/``
* target-specific directories such as ``test/ARM/`` or ``test/RISCV/``

Cover at least:

* accepted spelling(s),
* missing argument handling if applicable,
* invalid value diagnostics if applicable,
* interaction with related options.

5. Update docs/design index when adding new docs
------------------------------------------------

Register new design docs in ``docs/design/index.rst`` under the main toctree.

GeneralOptions and driver wiring
================================

Most command-line options ultimately flow through ``Config.options()``, which is
``eld::GeneralOptions`` stored in ``eld::LinkerConfig``.

Relevant files:

* ``include/eld/Config/GeneralOptions.h`` (option state + setters/getters)
* ``lib/Config/GeneralOptions.cpp`` (non-trivial parsing/helpers)
* ``include/eld/Config/LinkerConfig.h`` (``options()`` accessor)
* ``lib/LinkerWrapper/*LinkDriver.cpp`` and ``lib/LinkerWrapper/GnuLdDriver.cpp``
  (map parsed CLI args to ``GeneralOptions``)

When to change ``GeneralOptions``
---------------------------------

1. If the option only triggers immediate behavior inside parsing code, you may
   not need new ``GeneralOptions`` state.
2. If later link stages need the value, add storage + accessor APIs in
   ``GeneralOptions`` and set the value during option processing.
3. For options with value validation or reusable parsing logic, implement helper
   methods in ``GeneralOptions.cpp`` and call them from the driver.

Recommended pattern for new stateful options
--------------------------------------------

1. Add a private field in ``GeneralOptions`` with a safe default.
2. Add focused setter/getter APIs in ``GeneralOptions.h``.
3. If parsing is non-trivial (enums, list parsing, validation), add a
   ``setXxx(...)`` helper in ``GeneralOptions.cpp`` that returns success/failure.
4. In driver option handling, parse ``Args`` and call ``Config.options().setXxx``.
5. Emit diagnostics through existing ``Config.raise(...)`` / diagnostic IDs on
   invalid input.
6. Consume the option from downstream code via ``Config.options().getXxx()``.

Testing expectations with ``GeneralOptions`` changes
----------------------------------------------------

1. Add command-line tests showing parser behavior and diagnostics.
2. Add or update behavior tests that prove downstream consumers observe the new
   ``GeneralOptions`` state.
3. Include at least one negative case for invalid values when applicable.

How option IDs are produced
===========================

Each driver header creates an enum from generated ``*.inc`` files. For example,
``include/eld/Driver/GnuLdDriver.h`` defines ``OPT_GnuLdOptTable`` with IDs from
``GnuLinkerOptions.inc``. Target driver headers do the same for target tables.

Generated ``*.inc`` files are built via:

* ``include/eld/Driver/CMakeLists.txt``
* ``tablegen(... -gen-opt-parser-defs)``

Do not hand-edit generated ``*.inc`` files.

Community contribution guidelines for options
=============================================

1. Prefer compatible spellings when extending existing GNU-like behavior.
2. Add negative form when relevant (for example ``--foo`` and ``--no-foo``).
3. Keep help text imperative and specific.
4. Reuse existing option groups unless a new grouping is clearly needed.
5. Wire diagnostics through existing diagnostic paths instead of ad hoc prints.
6. Add at least one focused test with observable behavior.

Suggested review checklist
==========================

1. Is the option declared in the right ``.td`` scope (GNU vs target)?
2. Are aliases and spelling variants intentional?
3. Is parse ordering correct for this option's semantics?
4. Are invalid/missing values diagnosed?
5. Do tests cover both success and failure/edge behavior?
6. Does ``--help`` placement (group/help text/metavar) look correct?
