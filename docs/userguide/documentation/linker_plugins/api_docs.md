# Linker Plugin API Usage and Reference

## Linker Wrapper

```{eval-rst}
.. d2:: ../images/LinkerWrapper.d2
   :alt: LinkerWrapper flow.
```

Plugins interacts with the linker using LinkerWrapper.

```{eval-rst}
.. doxygenclass:: eld::plugin::LinkerWrapper
   :members:
```

### Relocation replacement data

`LinkerWrapper::setTargetDataForUse(Use &U, uint64_t Data)` supplies complete
instruction or data bytes for a relocation's storage field, not an unencoded
operand. Later successful calls by the same plugin update the replacement;
the last accepted value is used. A null use returns an error.

Plugins may submit replacement data from worker threads, including when a
layout map is enabled. Replacement insertion, allocation of the corresponding
map operation, and updates to map bookkeeping are serialized together.
All worker tasks launched by an overridden plugin callback must finish before
that callback returns, for example `OutputSectionIteratorPlugin::Run()`
during `AfterLayout`. This does not require all tasks to finish before an
individual `setTargetDataForUse()` call returns. Other plugin APIs and
concurrent layout-map reads are not covered by this guarantee.
Replacing storage bytes does not suppress normal relocation scanning or
allocation of GOT/PLT entries and dynamic relocations.

### Plugin-owned relocations

`LinkerWrapper::claimUse(Use U)` lets a plugin take responsibility for the
final bytes at one relocation site. This is useful when a plugin replaces an
address calculation or stored pointer with an absolute value: normal scanning
could otherwise allocate a GOT or PLT slot or a dynamic relocation that would
change the replacement at runtime.

Call `claimUse` from an early plugin hook after relocations are available,
for example `LinkerPlugin::ActBeforeRuleMatching`, or from `RelocCallBack`.
An early hook can enumerate uses with `getUses`; callback registration is not
required. Alternatively, register relevant types with `registerReloc` in
`LinkerPluginConfig::Init` to receive uses during the serial callback prepass.
Claims can target any eligible use, not just the current callback's use, and
joined plugin worker tasks may claim uses. ELD closes the claim window after
the prepass, before starting target scanning, even when no callbacks ran.
Output-section `BeforeLayout` and subsequent hooks are too late to claim.

Claims are supported during final links for executables, shared libraries,
and PIEs. The source must have a complete relocation storage field with a
nonzero width of at most eight bytes in a PROGBITS chunk. Partial links,
late claims, duplicate claims (including a second claim by the same plugin),
and claims for uses with an existing replacement from `setTargetDataForUse`
are rejected.
Check each API result and report errors through the plugin diagnostic framework.

For each claimed use that survives into the output, the owning plugin must
supply final bytes with `setTargetDataForUse` before relocation application.
For an output-section iterator this can be done in its `AfterLayout` run,
when final addresses are available. The first accepted replacement belongs to
its writing plugin, even without a claim. Later successful calls by that plugin
update the replacement data, and the last accepted value is used. Writes by
another plugin are rejected, even when the proposed bytes are identical.
Conflicting claims and writes cause a link error even if the plugin ignores
the returned error: ELD diagnoses unreported conflicts before relocation
application. Check and report each API result so errors can be diagnosed
earlier. Replacement data and layout-map bookkeeping are serialized together.
Finish all worker tasks before returning from the plugin hook. ELD closes
replacement writes when validation/application begins; later setter calls
fail rather than accepting data that cannot affect the output. Missing data
and NOLOAD/NOBITS output storage are diagnosed for surviving claims before
relocation application; discarded claims do not require replacement data.

ELD skips normal target scanning and application for claimed uses. Those uses
do not reserve GOT/PLT entries or dynamic relocations and are omitted from
`--emit-relocs`. Other references to the same symbol retain normal processing
and may still allocate those entries. Existing garbage-collection dependencies
and the use's symbol, type, and addend are retained. String and constant pools
may still be merged, but claimed uses are not retargeted by that merging.
Hexagon also skips trampoline creation for claimed branches. The plugin must
validate the instruction sequence, encode complete instruction or data bytes
within the original storage width, and handle any other target relaxation that
affects its rewrite. Claiming does not validate the semantic correctness of
those bytes.

`setTargetDataForUse` alone replaces final storage bytes but does not suppress
normal scanning or runtime relocation allocation. `setSymbolAddress` changes
the symbol to an absolute value but does not claim its references. A plugin must
identify the references it intends to rewrite, including references expressed
as a section symbol plus an addend, and claim each relevant use explicitly.

For example, a string-ID plugin can resolve a section-symbol-plus-addend use to
its original containing object symbol, check local binding, and claim the use in
the callback. After layout, it assigns the object's absolute ID using
`setSymbolAddress` (introduced in [PR #1751](https://github.com/qualcomm/eld/pull/1751))
and submits fully encoded instruction or pointer bytes with `setTargetDataForUse`.
Section-relative attribution, instruction rewriting, and preservation of genuine
interior offsets remain plugin policy, not target-specific behavior in ELD.

`LinkerWrapper::isSymbolPreemptible(Symbol S)` queries the target backend's
interposition rules for the current symbol and link configuration. It accounts
for binding, visibility, output type, symbol versioning, and `-Bsymbolic` /
`-Bsymbolic-functions`. The query does not change symbol binding or visibility;
a null symbol conservatively returns `true`. Before replacing a reference
with a fixed value, a plugin must decide whether doing so preserves required
runtime interposition.

## Plugin Abstract Data Types

### Chunk

```{eval-rst}
.. doxygenstruct:: eld::plugin::Chunk
   :members:
```

### LinkerScriptRule

```{eval-rst}
.. doxygenstruct:: eld::plugin::LinkerScriptRule
   :members:
```

### OutputSection

```{eval-rst}
.. doxygenstruct:: eld::plugin::OutputSection
   :members:
```

### Section

```{eval-rst}
.. doxygenstruct:: eld::plugin::Section
   :members:
```

### Use

```{eval-rst}
.. doxygenstruct:: eld::plugin::Use
   :members:
```

### Symbol

```{eval-rst}
.. doxygenstruct:: eld::plugin::Symbol
   :members:
```

### INIFile

```{eval-rst}
.. doxygenstruct:: eld::plugin::INIFile
   :members:
```

### InputFile

```{eval-rst}
.. doxygenstruct:: eld::plugin::InputFile
   :members:
```

### PluginData

```{eval-rst}
.. doxygenstruct:: eld::plugin::PluginData
   :members:
```

### AutoTimer

```{eval-rst}
.. doxygenstruct:: eld::plugin::AutoTimer
   :members:
```

### Timer

```{eval-rst}
.. doxygenstruct:: eld::plugin::Timer
   :members:
```

### RelocationHandler

```{eval-rst}
.. doxygenstruct:: eld::plugin::RelocationHandler
   :members:
```

## LinkerPluginConfig

```{eval-rst}
.. doxygenclass:: eld::plugin::LinkerPluginConfig
   :members:
   :protected-members:
```

## Utility Data Structures

### JSON Objects

```{eval-rst}
.. doxygenstruct:: eld::plugin::SmallJSONObject
   :members:
```

```{eval-rst}
.. doxygenstruct:: eld::plugin::SmallJSONArray
   :members:
```

```{eval-rst}
.. doxygenstruct:: eld::plugin::SmallJSONValue
   :members:
```

### TarWriter

```{eval-rst}
.. doxygenclass:: eld::plugin::TarWriter
   :members:
```

### ThreadPool

```{eval-rst}
.. doxygenclass:: eld::plugin::ThreadPool
   :members:
```

## Plugin Diagnostic Framework

Plugin diagnostic framework provides a uniform and consistent solution for
reporting diagnostics from plugins. It is recommended to avoid custom
solutions for printing diagnostics from a plugin.

First, let's go over some of the key benefits of using the Plugin Diagnostic
Framework. After that, we'll see how to effectively use the framework.

The framework allows for creating and reporting diagnositcs on the fly.
Diagnostics can be reported with different severities such as *Note*,
*Warning*, *Error* and more. Each severity has a unique color, and diagnostics
are prefixed with "PluginName:DiagnosticSeverity".

The linker keeps track of diagnostics emitted from the diagnostic framework.
This is required to maintain extensive logs for diagnostic purposes.
The framework also adjusts diagnostic behavior based on linker command-line
options that affect diagnostics. For example, if the {code}`--fatal-warnings`
option is enabled, then warnings will be converted to fatal errors. If
the {code}`-Werror` option is used, warnings are promoted to regular errors
instead of fatals.

The framework is thread-safe. Thus, diagnostics can be reported safely from
multiple threads simultaneously. This is crucial in situations where
thread-safety is neessary. In such cases, C++ {code}`std::cout` and
{code}`std::cerr` output streams cannot be used, as they are not thread-safe.

That's enough about the theories. Now, let's take a look at how to use the
Plugin Diagnostic Framework.

The linker assigns a unique ID to each diagnostic template. A diagnostic
template has a diagnostic severity and a diagnostic format string.
Diagnostic template unique IDs are necessary for creating and reporting
diagnostics. The code below demonstrates how to obtain a diagnostic ID
for a diagnostic template.

```c++
// There are similar functions for other diagnostic severities.
// Linker is a LinkerWrapper object.
DiagnosticEntry::DiagIDType errorID = Linker->getErrorDiagID("Error diagnostic with two args: %0 and %1");
DiagnosticEntry::DiagIDType warnID = Linker->getWarningDiagID("Warning diagnostic with one arg: %0");
```

{code}`%0`, {code}`%1` and so on in diagnostic format string are replaced by
diagnostic arguments. Diagnostic arguments must be provided when reporting a
diagnostic.

The below code demonstrates how to report a diagnostic using a diagnostic ID.

```c++
// arguments can be of any type. 'LinkerWrapper::reportDiag' is a
// variadic template function.
Linker->reportDiag(errorID, arg1, arg2);
Linker->reportDiag(warnID, arg1);
```

### DiagnosticEntry

{code}`DiagnosticEntry` packs complete diagnostic information, and can
be used to pass diagnostics from one location to another. Many plugin
framework APIs use {code}`DiagnosticEntry` to return errors to the caller,
where they can be properly handled.

```c++
// diag represents a complete diagnostic.
// diagID encodes diagnostic severity and diagnostic format string.
DiagnosticEntry diag(diagID, {arg1, arg2, ...});
```

### eld::Expected\<ReturnType>

Many plugin framework APIs return values of type {code}`eld::Expected<ReturnType>`.
At any given time, {code}`eld::Expected<ReturnType>` holds either an
expected value of type {code}`ReturnType` or an unexpected value of type
{code}`std::unique_ptr<eld::plugin::DiagnosticEntry>`. Returning the error to the
caller allows plugin authors to decide how to best handle a particular error
for their use case. A typical usage pattern for this is.

```c++
eld::Expected<eld::plugin::INIFile> readFile = Linker->readINIFile(configPath);
if (!readFile) {
  if (readFile.error()->diagID() == eld::plugin::Diagnostic::errr_file_does_not_exit) {
    // handle this particular error
    // ...
  } else {
    // Or, simply report the error and return.
    Linker->reportDiagEntry(std::move(readFile.error()));
    return;
  }
}

eld::plugin::INIFile file = std::move(readFile.value());
```

### Overriding Diagnostic Severity

Diagnostic severity can be overriden by using {code}`DiagnosticEntry`
subclasses. {code}`DiagnosticEntry` has subclasses for each diagnostic
severity level. Let's explore how to use them to override diagnostic severity.

```
eld::Expected<eld::plugin::INIFile> readFile = Linker->readINIFile(configPath);
eld::plugin::INIFile file;
if (readFile)
  file = std::move(readFile.value());
else {
  // Let's report the error as a Note, and move on by using a default INI file.
  eld::plugin::NoteDiagnositcEntry noteDiag(readFile.error()->diagID(), readFile.error()->args());
  Linker->reportDiagEntry(noteDiag);
  file = DefaultINIFile();
}
```

### Diagnostic Framework types API reference

```{eval-rst}
.. doxygenclass:: eld::plugin::DiagnosticBuilder
   :members:
```

```{eval-rst}
.. doxygenclass:: eld::plugin::DiagnosticEntry
   :members:
```

```{eval-rst}
.. doxygenclass:: eld::plugin::FatalDiagnosticEntry
   :members:
```

```{eval-rst}
.. doxygenclass:: eld::plugin::ErrorDiagnosticEntry
   :members:
```

```{eval-rst}
.. doxygenclass:: eld::plugin::WarningDiagnosticEntry
   :members:
```

```{eval-rst}
.. doxygenclass:: eld::plugin::NoteDiagnosticEntry
   :members:
```

```{eval-rst}
.. doxygenclass:: eld::plugin::VerboseDiagnosticEntry
   :members:
```
