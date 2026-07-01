Linker Plugin API Usage and Reference
=======================================

Linker Wrapper
----------------

.. graphviz:: ../images/LinkerWrapper.dot
   :alt: LinkerWrapper flow.
----------------------------------------------

   Plugins interacts with the linker using LinkerWrapper.

.. doxygenclass:: eld::plugin::LinkerWrapper
   :members:

Plugin Abstract Data Types
-----------------------------

Chunk
^^^^^^^^

.. doxygenstruct:: eld::plugin::Chunk
   :members:

LinkerScriptRule
^^^^^^^^^^^^^^^^^^

.. doxygenstruct:: eld::plugin::LinkerScriptRule
   :members:

OutputSection
^^^^^^^^^^^^^^^

.. doxygenstruct:: eld::plugin::OutputSection
   :members:

Section
^^^^^^^^

.. doxygenstruct:: eld::plugin::Section
   :members:

Use
^^^^^^^

.. doxygenstruct:: eld::plugin::Use
   :members:

Symbol
^^^^^^^

.. doxygenstruct:: eld::plugin::Symbol
   :members:

INIFile
^^^^^^^^^^^

.. doxygenstruct:: eld::plugin::INIFile
   :members:

InputFile
^^^^^^^^^^^

.. doxygenstruct:: eld::plugin::InputFile
   :members:

PluginData
^^^^^^^^^^^^^

.. doxygenstruct:: eld::plugin::PluginData
   :members:

AutoTimer
^^^^^^^^^^^^

.. doxygenstruct:: eld::plugin::AutoTimer
   :members:

Timer
^^^^^^^

.. doxygenstruct:: eld::plugin::Timer
   :members:

RelocationHandler
^^^^^^^^^^^^^^^^^^^

.. doxygenstruct:: eld::plugin::RelocationHandler
   :members:

LinkerPluginConfig
-------------------

.. doxygenclass:: eld::plugin::LinkerPluginConfig
   :members:
   :protected-members:

Utility Data Structures
--------------------------

JSON Objects
^^^^^^^^^^^^^^^

.. doxygenstruct:: eld::plugin::SmallJSONObject
   :members:

.. doxygenstruct:: eld::plugin::SmallJSONArray
   :members:

.. doxygenstruct:: eld::plugin::SmallJSONValue
   :members:

TarWriter
^^^^^^^^^^^^

.. doxygenclass:: eld::plugin::TarWriter
   :members:

ThreadPool
^^^^^^^^^^^^^

.. doxygenclass:: eld::plugin::ThreadPool
   :members:

Plugin Diagnostic Framework
-----------------------------

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
options that affect diagnostics. For example, if the :code:`--fatal-warnings`
option is enabled, then warnings will be converted to fatal errors.  If
the :code:`-Werror` option is used, warnings are promoted to regular errors
instead of fatals.

The framework is thread-safe. Thus, diagnostics can be reported safely from
multiple threads simultaneously. This is crucial in situations where
thread-safety is neessary. In such cases, C++ :code:`std::cout` and
:code:`std::cerr` output streams cannot be used, as they are not thread-safe.

That's enough about the theories. Now, let's take a look at how to use the
Plugin Diagnostic Framework.

The linker assigns a unique ID to each diagnostic template.  A diagnostic
template has a diagnostic severity and a diagnostic format string.
Diagnostic template unique IDs are necessary for creating and reporting
diagnostics. The code below demonstrates how to obtain a diagnostic ID
for a diagnostic template.

.. code-block:: cpp

   // There are similar functions for other diagnostic severities.
   // Linker is a LinkerWrapper object.
   DiagnosticEntry::DiagIDType errorID = Linker->getErrorDiagID("Error diagnostic with two args: %0 and %1");
   DiagnosticEntry::DiagIDType warnID = Linker->getWarningDiagID("Warning diagnostic with one arg: %0");

:code:`%0`, :code:`%1` and so on in diagnostic format string are replaced by
diagnostic arguments. Diagnostic arguments must be provided when reporting a
diagnostic.

The below code demonstrates how to report a diagnostic using a diagnostic ID.

.. code-block:: cpp

   // arguments can be of any type. 'LinkerWrapper::reportDiag' is a
   // variadic template function.
   Linker->reportDiag(errorID, arg1, arg2);
   Linker->reportDiag(warnID, arg1);

DiagnosticEntry
^^^^^^^^^^^^^^^^

:code:`DiagnosticEntry` packs complete diagnostic information, and can
be used to pass diagnostics from one location to another. Many plugin
framework APIs use :code:`DiagnosticEntry` to return errors to the caller,
where they can be properly handled.

.. code-block:: cpp

   // diag represents a complete diagnostic.
   // diagID encodes diagnostic severity and diagnostic format string.
   DiagnosticEntry diag(diagID, {arg1, arg2, ...});

eld::Expected<ReturnType>
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Many plugin framework APIs return values of type :code:`eld::Expected<ReturnType>`.
At any given time, :code:`eld::Expected<ReturnType>` holds either an
expected value of type :code:`ReturnType` or an unexpected value of type
:code:`std::unique_ptr<eld::plugin::DiagnosticEntry>`. Returning the error to the
caller allows plugin authors to decide how to best handle a particular error
for their use case. A typical usage pattern for this is.

.. code-block:: cpp

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


Overriding Diagnostic Severity
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Diagnostic severity can be overriden by using :code:`DiagnosticEntry`
subclasses. :code:`DiagnosticEntry` has subclasses for each diagnostic
severity level. Let's explore how to use them to override diagnostic severity.

.. code-block::

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

Diagnostic Framework types API reference
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenclass:: eld::plugin::DiagnosticBuilder
   :members:

.. doxygenclass:: eld::plugin::DiagnosticEntry
   :members:

.. doxygenclass:: eld::plugin::FatalDiagnosticEntry
   :members:

.. doxygenclass:: eld::plugin::ErrorDiagnosticEntry
   :members:

.. doxygenclass:: eld::plugin::WarningDiagnosticEntry
   :members:

.. doxygenclass:: eld::plugin::NoteDiagnosticEntry
   :members:

.. doxygenclass:: eld::plugin::VerboseDiagnosticEntry
   :members:
