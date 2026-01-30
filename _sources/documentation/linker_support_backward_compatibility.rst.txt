==============================
Shared Library Versioning
==============================

Overview
--------
Shared library versioning is a mechanism used in operating systems to manage different versions of dynamic libraries (.so files on Unix-like systems). It ensures that applications link to the correct version of a library while maintaining compatibility.

Key Concepts
------------

**SONAME**
    - The SONAME (Shared Object Name) is an identifier embedded in a shared library that represents its ABI (Application Binary Interface) version.
    - Applications link against the SONAME, not the actual filename, allowing upgrades without breaking compatibility.

**Semantic Versioning**
    - Libraries often follow semantic versioning: `MAJOR.MINOR.PATCH`.
    - **MAJOR**: Changes that break backward compatibility.
    - **MINOR**: Backward-compatible feature additions.
    - **PATCH**: Backward-compatible bug fixes.

ABI vs API
----------
- **API (Application Programming Interface)**: The set of functions and symbols exposed by the library.
- **ABI (Application Binary Interface)**: The compiled interface, including calling conventions, data types, and memory layout.
- ABI stability is crucial for shared libraries because applications depend on binary compatibility.

Backward Compatibility
-----------------------
- Libraries should maintain backward compatibility within the same major version.
- Breaking ABI changes require incrementing the major version and updating the SONAME.

Best Practices
--------------
- Use SONAME to manage ABI versions.
- Avoid breaking ABI unless necessary.
- Provide symbolic links:
    - `libexample.so` → `libexample.so.1` → `libexample.so.1.2.3`
- Document changes clearly.

Nuances
-------
- Different distributions may package libraries differently.
- Some systems use `ldconfig` to manage symbolic links.
- Applications may fail if the expected SONAME is missing, even if the actual library file exists.

Examples
========

SONAME Usage Example
--------------------
When building a shared library:

.. code-block:: bash

    clang -shared -Wl,-soname,libexample.so.1 -o libexample.so.1.2.3 example.o

Resulting files:

.. code-block:: text

    libexample.so.1.2.3   # Actual library file
    libexample.so.1       # SONAME symlink (ABI version)
    libexample.so         # Generic symlink for development

Applications link against `libexample.so.1`, not the full filename.

Semantic Versioning Example
---------------------------
- libmath.so.1.0.0 → Initial release
- libmath.so.1.1.0 → Adds new functions (backward-compatible)
- libmath.so.2.0.0 → Changes function signatures (breaks ABI)

When moving from 1.x.x to 2.x.x, update SONAME to `libmath.so.2`.

ABI vs API Example
------------------
- **API Change (safe)**: Adding a new function `add_matrix()` does not break existing binaries.
- **ABI Break (unsafe)**: Changing `struct Matrix { int rows; int cols; }` to include a new field changes memory layout, breaking existing binaries.

Symbolic Link Structure
-----------------------
After installation:

.. code-block:: text

    /usr/lib/
        libexample.so -> libexample.so.1
        libexample.so.1 -> libexample.so.1.2.3
        libexample.so.1.2.3

ASCII Diagram:

.. code-block:: text

    libexample.so → libexample.so.1 → libexample.so.1.2.3

Nuances
-------
- Different Linux distros may package libraries differently.
- If SONAME is missing, apps fail with:

.. code-block:: text

    error while loading shared libraries: libexample.so.1: cannot open shared object file


==============================
GNU ELF Symbol Versioning
==============================

.. Note::

  Support for symbol versioning is planned for a future release. Stay tuned for updates

This document continues to show a minimal end-to-end demonstration of GNU ELF symbol
versioning, then documents:

1. What the ``.symver`` / ``__attribute__((symver))`` mechanisms do.
2. What a GNU ld *version script* does.
3. Which ELF sections the linker/loader create/use for symbol versioning.

The example exports two versions of the same symbol and preserves
backward compatibility across library releases.

.. contents::
   :local:
   :depth: 2


A. Minimal Working Example (C + GNU ld)
=======================================

Goal
----

Export two versions of an API symbol (``foo``), keeping ABI compatibility
for old applications while making a new implementation the default for
new applications.


Files
-----

``demo_v1.c`` — first release of the library
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // v1 of the library: exports foo()
   #include <stdio.h>

   void foo(void) { puts("foo v1"); }

``demo_v1.map`` — version script for first release
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

   DEMO_1 {
     global: foo;
     local:  *;
   };

This assigns ``foo`` to version node ``DEMO_1`` and hides everything else.

``demo_v2.c`` — second release (keep old ``foo`` and add a new default ``foo``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #include <stdio.h>

   /* Approach A: classic assembler directive (works with GCC & Clang) */
   __asm__(".symver foo_v1,foo@DEMO_1");    // non-default (old) foo
   __asm__(".symver foo_v2,foo@@DEMO_2");   // default (new) foo

   void foo_v1(void) { puts("foo v1"); }
   void foo_v2(void) { puts("foo v2 (default)"); }

   /* New API symbol in v2 */
   void bar(void) { puts("bar v2"); }


``demo_v2.map`` — version script for second release
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

   DEMO_1 {
     global: foo;
     local:  *;
   };

   DEMO_2 {
     global: foo; bar;
   } DEMO_1;  /* DEMO_2 inherits from DEMO_1 */

``main_old.c`` — an “old” app built against v1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void foo(void);
   int main(void) { foo(); }

``main_new.c`` — a “new” app built against v2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void foo(void);
   void bar(void);
   int main(void) { foo(); bar(); }

``Makefile``
~~~~~~~~~~~

.. code-block:: make

   CC?=clang
   CFLAGS=-fPIC -O2 -Wall -Wextra

   all: stage1 stage2 inspect

   stage1: libdemo.so.1.0 p_old

   libdemo.so.1.0: demo_v1.c demo_v1.map
       $(CC) $(CFLAGS) -shared -Wl,-soname,libdemo.so.1 \
           -Wl,--version-script=demo_v1.map -o $@ demo_v1.c
       ln -sf $@ libdemo.so

   p_old: main_old.c libdemo.so
       $(CC) -Wl,-rpath,'$$ORIGIN' -L. -o $@ main_old.c -ldemo

   stage2: libdemo.so.2.0 p_new

   libdemo.so.2.0: demo_v2.c demo_v2.map
       $(CC) $(CFLAGS) -shared -Wl,-soname,libdemo.so.1 \
           -Wl,--version-script=demo_v2.map -o $@ demo_v2.c
       ln -sf $@ libdemo.so

   p_new: main_new.c libdemo.so
       $(CC) -Wl,-rpath,'$$ORIGIN' -L. -o $@ main_new.c -ldemo

   inspect:
       @echo "==> Exported (with versions) in libdemo.so"
       @nm -D --with-symbol-versions libdemo.so | egrep ' foo| bar' || true
       @echo; echo "==> Version sections in libdemo.so"
       @readelf -W --version-info libdemo.so | sed -n '1,120p'

   clean:
       rm -f p_old p_new libdemo.so libdemo.so.* *.o


Build & Run
-----------

.. code-block:: bash

   # Stage 1: build first library + "old" app
   make stage1
   LD_LIBRARY_PATH=. ./p_old
   # -> prints: foo v1

.. code-block:: bash

   # Stage 2: replace lib with v2 (keeps SONAME), rebuild "new" app
   make stage2
   LD_LIBRARY_PATH=. ./p_old
   # -> still prints: foo v1  (old app bound to DEMO_1)
   LD_LIBRARY_PATH=. ./p_new
   # -> prints: "foo v2 (default)" and "bar v2"


Inspecting the Result
---------------------

See exported symbols and their versions:

.. code-block:: bash

   nm -D --with-symbol-versions libdemo.so | egrep ' foo| bar'
   # ... foo@DEMO_1
   # ... foo@@DEMO_2
   # ... bar@@DEMO_2

``@@`` marks the *default* version; ``@`` marks a non-default (older) version.

See version metadata sections:

.. code-block:: bash

   readelf -W --version-info libdemo.so

You should see:

- Version definitions (from ``.gnu.version_d``).
- Per-symbol version indices in ``.gnu.version``.
- (In executables) Version needs in ``.gnu.version_r``.

Optional runtime trace from the dynamic linker:

.. code-block:: bash

   LD_DEBUG=versions LD_LIBRARY_PATH=. ./p_new 2>&1 | grep -E 'checking for version|needed'


B. What ``.symver`` / ``__attribute__((symver))`` Do
====================================================

- ``.symver`` (assembler directive)
  - Binds a specific implementation symbol to a public name *and* a version node.
  - Syntax: ``.symver <impl>, <public>@<NODE>`` or ``.symver <impl>, <public>@@<NODE>``.
  - ``@@`` marks the default implementation selected by new linkers.
  - The version node must exist in the version script when you build the DSO.

- ``__attribute__((symver("...")))``
  - Front-end attribute that makes compiler emit the corresponding ``.symver``.
  - Handy with LTO because it attaches at the language level.

Exactly one default definition should exist for a symbol (the one with ``@@``).


C. What a Version Script Does
=============================

A GNU ld *version script* (use with ``-Wl,--version-script=...``):

1. Declares **version nodes** and binds **symbols** to those nodes.
2. Controls visibility (``global:`` exported; ``local:`` hidden).
3. Supports **inheritance** between nodes to evolve the ABI without bumping the SONAME.
4. For C++, you can match demangled names with an ``extern "C++" { ... }`` block.

If you want to tag all currently **exported** symbols with one version without changing visibility,
this works:

.. code-block:: none

   V1 { *; };

This does not force hidden symbols to become exported; it only tags whatever is already exported.


D. ELF Sections Used by Symbol Versioning
=========================================

When you link with versioned symbols, the following GNU extension sections (referenced by dynamic
tags) are produced/used:

- ``.gnu.version`` (*SHT_GNU_versym*, referenced by ``DT_VERSYM``)
  - A table parallel to ``.dynsym`` that stores a 16-bit version index per symbol.

- ``.gnu.version_d`` (*SHT_GNU_verdef*, referenced by ``DT_VERDEF``/``DT_VERDEFNUM``)
  - Version **definitions** provided by this DSO (node names, indices, hashes).

- ``.gnu.version_r`` (*SHT_GNU_verneed*, referenced by ``DT_VERNEED``/``DT_VERNEEDNUM``)
  - Version **requirements** (what this module needs from its dependencies).

``readelf -W --version-info`` summarizes these sections so you can verify bindings and defaults.


E. Practical Notes & Common Pitfalls
====================================

- C++ name mangling:
  - If you version C++ functions with ``.symver``, use the **mangled** name.
  - In version scripts you can use demangled names inside ``extern "C++" { ... }``.

- Default vs. non-default:
  - Default shows as ``@@``; older variants show as ``@``.
  - Newly linked apps bind to the default; previously linked apps keep using their recorded version.

- Diagnostics:
  - ``nm -D --with-symbol-versions``: quick view of ``foo@@V2`` / ``foo@V1``.
  - ``readelf -W --version-info``: detailed view of ``.gnu.version*`` sections.
  - ``LD_DEBUG=versions``: watch the dynamic linker’s version checks at runtime.


F. One-Page Cheat Sheet
=======================

- ``.symver`` / ``__attribute__((symver))``:
  - Attach a version to a symbol definition; exactly one default (``@@``) per symbol.
  - Ensure the version node exists in the link-time version script.

- Version script:
  - Define version nodes, bind symbols, control visibility, and express inheritance.
  - Can version all currently exported symbols via ``V1 { *; };``.

- ELF sections:
  - ``.gnu.version`` — per-dynamic-symbol version indices (``DT_VERSYM``).
  - ``.gnu.version_d`` — version definitions (``DT_VERDEF*``).
  - ``.gnu.version_r`` — version requirements (``DT_VERNEED*``).


Appendix: Quick Commands
========================

.. code-block:: bash

   # Build first version and old app
   make stage1
   LD_LIBRARY_PATH=. ./p_old

   # Upgrade library, build new app
   make stage2
   LD_LIBRARY_PATH=. ./p_old
   LD_LIBRARY_PATH=. ./p_new

   # Inspect exports and versions
   nm -D --with-symbol-versions libdemo.so | egrep ' foo| bar'
   readelf -W --version-info libdemo.so

   # Trace loader checks
