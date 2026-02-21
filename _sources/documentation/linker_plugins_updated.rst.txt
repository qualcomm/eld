Linker Plugins
===============

Linker plugin framework is a user-friendly solution to add custom link behavior.
This allows non-linker developers to tweak and hack linker for the specialized use-cases.
It is similar to how clang plugins let you tweak frontend compliation and LLVM passes
let you transform the LLVM IR. [[The core idea is to make the tool extendable and hackable
by providing the framework]].

But why would you ever need to customize link?
Short answer: Embedded development can get really fun at times. Long and more useful answer
will be discussed as we move on.

This guide contains all that you need to know about linker plugins, and how to effectively write one yourself.

**Overview**

* Implemented as a C++ class.
* Facilitates user-defined behavior to crucial parts of the link process.
* Provides finer control over the output image layout than a linker script.
* Allows inspecting symbols, chunks, relocation, section mapping, input and output sections.
* Allows adding and modifying section mapping rules, relocations, chunk attributes, and symbols.
* Plugins can communicate with the linker as well as with each other.
* Easy traceability of plugins using :code:`--trace=plugin` option.
* :code:`PluginADT` provides wrappers for many common linker data types.

.. toctree::
   :maxdepth: 2
   :numbered:

   linker_plugins/linker_plugins.rst
   linker_plugins/examples.rst
   linker_plugins/api_docs.rst
   linker_plugins/inbuilt_plugins.rst