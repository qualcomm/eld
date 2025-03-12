//===- DiagWriters.inc-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

DIAG(creating_tarball, DiagnosticEngine::Note, "Creating tarball %0")
DIAG(fail_to_create_tarball, DiagnosticEngine::Error,
     "Error creating tarball %0 due to %1")
DIAG(zlib_not_available, DiagnosticEngine::Error,
     "Compression support not available")
DIAG(ignore_cache_file_option, DiagnosticEngine::Warning,
     "Ignoring --cache-file option: Link is partial or linker script contains "
     "no rules")
DIAG(unable_to_write_symdef, DiagnosticEngine::Error,
     "Unable to write symdef file %0 due to %1")
DIAG(unsupported_section_kind, DiagnosticEngine::Warning,
     "unsupported section kind %0 of section %1")
DIAG(unsupported_output_file_type, DiagnosticEngine::Error,
     "Unsupported output file type: %0")
DIAG(err_output_size_too_large, DiagnosticEngine::Error,
     "Output file too large: %0 bytes")
