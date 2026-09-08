Symbol Resolution Inspector
###########################

``SymbolResolutionInspector.py`` reads the JSON file produced by
``ld.eld --emit-symbol-resolution-report=<file>`` and prints a compact view of
how ELD resolved symbols.

Use it when the raw JSON is too noisy and you want to inspect one symbol, a
small group of symbols, or all records that match a property in the rendered
symbol description.

Usage
=====

.. code-block:: sh

   utils/SymbolResolutionInspector/SymbolResolutionInspector.py report.json
   utils/SymbolResolutionInspector/SymbolResolutionInspector.py report.json --symbol '^foo$'
   utils/SymbolResolutionInspector/SymbolResolutionInspector.py report.json --filter 'bitcode|SharedLib'
   utils/SymbolResolutionInspector/SymbolResolutionInspector.py report.json --symbol '^foo$' --json

``--symbol`` is a Python regular expression matched against the JSON ``Name``
field. ``--filter`` is a Python regular expression matched against the human
readable symbol line printed by the tool.

The text output groups records by symbol name, prints the selected definition
when one exists, and marks selected records with ``[Selected]``. If a bitcode
symbol has a matching post-LTO object symbol, that record is printed as an
indented ``LTO:`` line.

``--json`` writes the matching records back as a valid symbol resolution report.
This is useful when another script expects the JSON schema but only needs a
filtered subset.

Symbol Line Format
==================

Each record is rendered as::

   name(input-file[plugin]:section) [Size=N, bitcode, SectionIndexKind, Binding, Type, Visibility]

The ``[plugin]``, ``:section``, and ``bitcode`` parts appear only when they are
present in the report.
