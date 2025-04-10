# About

At Qualcomm, embedded linkers are a critical component of our software stack. While several linkers function effectively on traditional platforms, they do not seamlessly support embedded use cases.

For instance, embedded projects :-

- Heavily rely on linker scripts, which are rarely a concern for host linking.
- Complex use cases involving embedded linking often necessitate custom extensions to linker functionality or linker scripts, tailored to the specific needs of customers.

We use a linker called eld derived from [mclinker](https://github.com/mclinker/mclinker). The linker operates on ELF files and is tailored to meet the requirements of the embedded community, which we refer to as the "Embedded linker (eld)".

The linker is distributed to our customers as part of the Qualcomm LLVM toolchains. It utilizes LLVM components and libraries wherever feasible, and our customers depend on the linker for constructing images that operate on Qualcomm hardware.

We are excited to open source the linker! Our goal is to develop new features with contributions from the linker and system software communities.

# Features

The linker is fully compliant with GNU standards, encompassing the reading of input files, symbol resolution, and the generation of GNU-compliant output.

The linker supports the following targets:
- ARM
- AArch64
- Hexagon
- RISC-V (including the Xqci extension)

The linker incorporates various features commonly available in GNU-compliant linkers, including:
- Partial linking
- Dynamic linking
- Static linking
- Link Time Optimization (LTO)
- Linker optimizations such as string merging

Additionally, the linker offers mature support for linker scripts with custom extensions.

# Extended Features

The linker also includes numerous features aimed at enhancing user experience, such as:

- Easy-to-read linker map files: Our customer support team extensively utilizes this information to diagnose and resolve issues. These map files are also machine-readable, enabling customers to generate custom reports from the images they build.

- Easy reproduction of link-time issues: A unique method for customers to share artifacts used at link time when encountering link-time issues.

- Comprehensive diagnostic options: Provides extensive debugging capabilities for complex issues such as symbol garbage collection, command-line issues, warnings, and errors.

# Linker Source Organization

The linker source code is categorized into three major sections:
- Core Linker (divided into functionally specific directories)
- Target-specific overrides
- Unit tests

The core linker encompasses the most critical features, while the target-specific overrides facilitate the customization of functionality provided by the core linker.

This encapsulation allows developers to modify and enhance target-specific functionality without impacting other targets.

The unit tests are divided into two categories:
- Unit testing using Google Unit Tests
- Lit tests using the LLVM lit test framework

# Linker Plugins

In response to increasing customer demand for inspecting and altering image layouts, extending linker functionality for unanticipated use cases, and generating custom reports, we have developed linker plugins.

These plugins allow customers to create custom passes that run during link time, offering complete user control and management by the linker, with a focus on diagnostics.

The plugin infrastructure also allows toolchain developers to treat the linker as a transparent component, facilitating the careful management of assumptions in the image, emitting errors or warnings if assumptions are violated, and encoding information in source code for linker consumption.

This infrastructure also assists compiler developers with layout decisions, enabling them to:
- Pass auxiliary information from the compiler to the linker
- Optimize image layout for specific use cases

# Major Qualcomm Products Utilizing the Linker

The linker is employed in various Qualcomm products, including:
- Qualcomm 5G modems (High Tier, Low Tier)
- Wireless LAN products and drivers
- Qualcomm AI software infrastructure
- Qualcomm firmware and device drivers
- Qualcomm Audio drivers
- ARM TrustZone
- Numerous ARM/AArch64/RISC-V images utilizing Zephyr RTOS
- Various microcontroller images based on RISC-V

# Testing

The linker comprises an extensive suite of unit test cases, which we aim to open source. These test cases are designed to be easily readable and modifiable. They include tests for the linker and linker plugin examples, enabling customers to comprehend the rich linker plugin infrastructure.

Our team also maintains an extensive testing farm of devices and applications that run on simulated platforms, which are tested daily.

# Documentation

The linker is accompanied by comprehensive user-facing documentation that elucidates linker behavior, as well as a thorough FAQ addressing common problems and solutions.

# Opening Issues

The team is dedicated to continually assessing and introducing features that focus on the embedded community.

We invite you to join our efforts in improving the linker and making the solution better. Please feel free to open any issues you would like to work on or share use cases that we do not currently support.

# Closing Remarks

We hope that the embedded linker solution addresses the use cases you are aiming to solve.

We would also like to acknowledge our team and the entire LLVM team at Qualcomm for their continuous contributions to enhancing the linker.

If you have any thoughts to share or would like to speak with us, please do not hesitate to contact us via email.
