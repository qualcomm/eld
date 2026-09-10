# Adding a New Backend to ELD

## Overview

ELD is a modular linker framework supporting multiple target architectures. The **Template backend** (`eld/lib/Target/Template`) is a minimal reference implementation you can copy and adapt for new architectures.

A complete ELD backend consists of three main components:

1. **Backend** - Core linking logic (relocations, GOT/PLT, symbol resolution)
2. **Driver** - Command-line interface and option parsing (optional)
3. **Wrapper** - Plugin API for extending the linker (optional)

This guide focuses on the backend implementation. The driver and wrapper are optional for basic functionality.

## Quick Start

1. Copy the Template backend directory to your new target name
2. Rename all `Template*` files and classes to your target name
3. Implement the key methods in each component
4. Register your backend in the target registry
5. (Optional) Create a driver for command-line support
6. (Optional) Implement wrapper hooks for plugin support

## Template Backend Structure

The Template backend contains the minimum necessary hooks:

```
Template/
├── TemplateInfo.h/cpp                 # Target machine type and flags
├── TemplateLDBackend.h/cpp            # Core linker backend
├── TemplateRelocator.h/cpp            # Relocation processing
├── TemplateGOT.h/cpp                  # Global Offset Table
├── TemplatePLT.h/cpp                  # Procedure Linkage Table
├── TemplateELFDynamic.h/cpp           # Dynamic section
├── TemplateRelocationInfo.h           # Relocation type definitions
├── TemplateRelocationFunctions.h      # Relocation helpers
├── TemplateRelocationCompute.cpp      # Relocation implementations
└── TargetInfo/TemplateTargetInfo.cpp  # Target registration
```

## Available Hooks

The Template backend implements the **minimum necessary hooks**. Additional hooks are available in base classes:

- `eld/Target/GNULDBackend.h` - Backend base class
- `eld/Target/Relocator.h` - Relocator base class
- `eld/Target/TargetInfo.h` - Target info base class
- `eld/Fragment/GOT.h` - GOT base class
- `eld/Fragment/PLT.h` - PLT base class
- `eld/Target/ELFDynamic.h` - Dynamic section base class

Refer to these headers for advanced functionality.

## References

- [ELF Specification](https://refspecs.linuxfoundation.org/elf/)
- Your target's ABI specification
- Template backend: `eld/lib/Target/Template/`
- Driver: `eld/include/eld/Driver/`
- Wrapper: `eld/lib/LinkerWrapper/`
