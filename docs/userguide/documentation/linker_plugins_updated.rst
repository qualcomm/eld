Linker Plugins User Guide
===========================

Linker plugin functionality is an elegant and easy-to-use solution to add user-defined behavior to ELD.

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