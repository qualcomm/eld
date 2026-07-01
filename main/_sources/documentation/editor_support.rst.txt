Editor Support
==============

.. contents::
   :local:

Overview
--------

ELD provides editor integration files that add syntax highlighting for ELD
linker scripts and map files.  The integration is currently available for
**Vim** (version 9.1 or later).

Vim Syntax Highlighting
-----------------------

Supported File Types
~~~~~~~~~~~~~~~~~~~~

The filetype-detection file recognises the following files automatically:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Pattern
     - Description
   * - ``*.ld``, ``*.lds``, ``*.lcs``, ``*.lcs.template``, ``*.t``, ``*.x``
     - ELD linker scripts
   * - ``*.map`` (first line matches ``# Linker``)
     - ELD map files

To set the filetype manually inside an open Vim buffer:

.. code-block:: vim

   :set filetype=eld

Installation
~~~~~~~~~~~~

Copy the two Vim files into your Vim runtime directories:

.. code-block:: bash

   cp etc/vim/eld.vim          ~/.vim/syntax/eld.vim
   cp etc/vim/ftdetect_eld.vim ~/.vim/ftdetect/eld.vim

Vim resolves syntax files by filetype name (``filetype=eld`` →
``syntax/eld.vim``), so the filenames must be kept exactly as shown.

After copying the files, open any linker script or map file and Vim will
apply syntax highlighting automatically.
