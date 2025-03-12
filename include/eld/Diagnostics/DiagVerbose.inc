//===- DiagVerbose.inc-----------------------------------------------------===//
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

DIAG(mapping_input_section_to_output_section, DiagnosticEngine::Verbose,
     "Mapping input section %0 to output section %1")
DIAG(loading_all_members, DiagnosticEngine::Verbose, "Initializing Archive %0")
DIAG(loading_member, DiagnosticEngine::Verbose, "Loading Member %0")
DIAG(release_memory_bitcode, DiagnosticEngine::Verbose,
     "Release LTO Module for file %0")
DIAG(release_file, DiagnosticEngine::Verbose, "Release memory for file %0")
DIAG(use_embedded_bitcode, DiagnosticEngine::Verbose,
     "Using embedded bitcode section from file %0")
DIAG(verbose_ehframe, DiagnosticEngine::Verbose,
     "Splitting EhFrame records by %0 from Offset %1 Size %2")
DIAG(applying_endof_image_address, DiagnosticEngine::Verbose,
     "Applying out of bound address for symbol %0 referred from %1 defined in "
     "%2")
DIAG(deleting_instructions, DiagnosticEngine::Verbose,
     "%0 : Deleting %1 bytes for symbol '%2' in section %3+0x%4 file %5")
DIAG(add_nops, DiagnosticEngine::Verbose,
     "%0 : Need only %1 bytes in section %2+0x%3 file %4")
DIAG(not_relaxed, DiagnosticEngine::Verbose,
     "%0 : Cannot relax %1 bytes for symbol '%2' in section %3+0x%4 file %5")
DIAG(relax_to_compress, DiagnosticEngine::Verbose,
     "%0 : relaxing instruction 0x%1 to compressed instruction 0x%2 for symbol "
     "%3 in section %4+0x%5 file %6")
DIAG(verbose_ehframe_remove_fde, DiagnosticEngine::Verbose, "EhFrame %0")
DIAG(verbose_ehframe_read_fde, DiagnosticEngine::Verbose, "EhFrame %0")
DIAG(verbose_ehframe_read_cie, DiagnosticEngine::Verbose, "EhFrame %0")
DIAG(verbose_ehframe_log, DiagnosticEngine::Verbose, "EhFrame Handling :  %0")
DIAG(set_symbol, DiagnosticEngine::Verbose, "Setting %0 to %1")
DIAG(adding_ini_hash_entry, DiagnosticEngine::Verbose,
     "Adding path %0 with hash %1 to mapping.ini")
DIAG(adding_mappingfile_to_tarball, DiagnosticEngine::Verbose,
     "Adding file %0 to tarball")
DIAG(creating_version_file, DiagnosticEngine::Verbose,
     "Creating version file %0")
DIAG(created_response_file, DiagnosticEngine::Verbose,
     "Created response file %0 with contents:\n%1")
DIAG(created_mapping_file, DiagnosticEngine::Verbose,
     "Created mapping file %0 with contents:\n%1")
DIAG(function_has_error, DiagnosticEngine::Verbose, "Function %0 has error")
DIAG(reproduce_signal_handler, DiagnosticEngine::Verbose,
     "Registered reproduce signal handler")
DIAG(keeping_section, DiagnosticEngine::Verbose,
     "Keeping section %0 from file %1")
DIAG(retaining_section, DiagnosticEngine::Trace,
     "Retaining section %0 from file %1")
DIAG(default_signal_handler, DiagnosticEngine::Verbose,
     "Registered default signal handler")
DIAG(reading_timing_data, DiagnosticEngine::Verbose,
     "Reading timing data for %0 with start time %1 and duration %2 from input "
     "file %3")
DIAG(executing_assert_after_layout_is_complete, DiagnosticEngine::Verbose,
     "Deferring executing assert \"%0\" until Layout")
DIAG(skipping_executed_asserts, DiagnosticEngine::Verbose,
     "Skipping executed assert \"%0\" after complete layout")
DIAG(executing_assert_after_layout, DiagnosticEngine::Verbose,
     "Now executing assert \"%0\"")
DIAG(reproduce_in_env, DiagnosticEngine::Verbose,
     "Setting reproduce option based on environment variable "
     "ELD_REPRODUCE_CREATE_TAR")
DIAG(disabling_cache_by_option, DiagnosticEngine::Verbose,
     "Disabling cache file generation due to --include-cache option")
DIAG(reproduce_using_cache_output_file, DiagnosticEngine::Verbose,
     "Using cache output file %0 for --reproduce")
DIAG(using_cache_input_file, DiagnosticEngine::Verbose,
     "Using cache input file %0")
DIAG(using_cache_output_file, DiagnosticEngine::Verbose,
     "Using cache output file %0")
// --- Linker.cpp
DIAG(initializing_linker, DiagnosticEngine::Verbose, "Initializing Linker")
DIAG(initializing_diagnostic_printer, DiagnosticEngine::Verbose,
     "Intializing Diagnostic Printer")
DIAG(reading_input_files, DiagnosticEngine::Verbose, "Readiing input files")
DIAG(verbose_loading_non_universal_plugins, DiagnosticEngine::Verbose,
     "Loading non universal plugins")
DIAG(verbose_loading_universal_plugins, DiagnosticEngine::Verbose,
     "Loading universal plugins")
DIAG(parsed_version_script, DiagnosticEngine::Verbose, "Parsing version script")
DIAG(parsing_dynlist, DiagnosticEngine::Verbose, "Parsing dyn list")
DIAG(adding_script_symbols, DiagnosticEngine::Verbose,
     "Adding symbol assigments from linker script")
DIAG(beginning_post_LTO_phase, DiagnosticEngine::Verbose,
     "Beginning post-LTO phase")
DIAG(beginning_link, DiagnosticEngine::Verbose, "Beginning link")
DIAG(initializing_standard_sections, DiagnosticEngine::Verbose,
     "Creating standard sections in output file")
DIAG(merging_input_sections, DiagnosticEngine::Verbose,
     "Merging input sections before layout")
DIAG(begin_layout, DiagnosticEngine::Verbose, "Preparing layout of output file")
DIAG(emit_output_file, DiagnosticEngine::Verbose, "Writing output file %0")
DIAG(printing_layout, DiagnosticEngine::Verbose, "Printing layout")
DIAG(initializing_backend, DiagnosticEngine::Verbose, "Initializing backend")
DIAG(initializing_object_linker, DiagnosticEngine::Verbose,
     "Initializing object linker")
DIAG(initializing_emulator, DiagnosticEngine::Verbose, "Initializing emulator")
DIAG(unloading_plugins, DiagnosticEngine::Verbose, "Unloading plugins")
DIAG(creating_internal_inputs, DiagnosticEngine::Verbose,
     "Created linker internal input %0")
// --- LinkerScript.cpp
DIAG(unloaded_plugin, DiagnosticEngine::Verbose, "Unloaded Plugin %0")
DIAG(added_plugin, DiagnosticEngine::Verbose, "Added plugin %0 to script")
DIAG(loaded_plugin, DiagnosticEngine::Verbose, "Loaded plugin %0 from script")
DIAG(added_section_override, DiagnosticEngine::Verbose,
     "Plugin '%0' added output section override ('%1') for section '%2' "
     "with annotation '%3'")
DIAG(added_chunk_op, DiagnosticEngine::Verbose,
     "Added chuck op for section %0 with annotation %1")
DIAG(removed_chunk_op, DiagnosticEngine::Verbose,
     "Removed chuck op for section %0 with annotation %1")
// --- Module.cpp
DIAG(creating_output_section, DiagnosticEngine::Verbose,
     "Creating output section %0 with type %1 flag %2 alignment %3")
DIAG(created_internal_section, DiagnosticEngine::Verbose,
     "Created linker internal section %0 with type %1 flag %2 alignment %3 "
     "entry size %4")
DIAG(created_eh_frame_hdr, DiagnosticEngine::Verbose,
     "Created eh frame header section %0 with type %1 flag %2 alignment %3")
DIAG(intializing_plugin, DiagnosticEngine::Verbose, "Initalized plugin %0")
DIAG(adding_output_section_for_plugin, DiagnosticEngine::Verbose,
     "Adding outputsection %0 for Plugin %1")
DIAG(adding_symbol_to_fragment, DiagnosticEngine::Verbose,
     "Adding symbol %0 to fragment")
DIAG(added_symbol, DiagnosticEngine::Verbose, "Adding symbol %0 %1")
DIAG(initialized_timer, DiagnosticEngine::Verbose, "Initialized timer %0")
DIAG(reading_compressed_section, DiagnosticEngine::Verbose,
     "Reading compressed section %0 from file %1")
DIAG(entry_symbol, DiagnosticEngine::Verbose, "Entry symbol %0")
DIAG(calling_function_from_dynamic_lib, DiagnosticEngine::Verbose,
     "Calling function '%0' from the dynamic library '%1'.")
DIAG(merging_strings, DiagnosticEngine::Verbose, "Merging strings")
DIAG(handling_merge_strings_for_section, DiagnosticEngine::Verbose,
     "Handling merge string relocations for section %0 in file %1")
DIAG(merging_fragments, DiagnosticEngine::Trace,
     "Merging fragment from file %0 in section %1\n\twith fragment "
     "from file %2 in section %3\n\tand content %4 to output section "
     "%5")
DIAG(modifying_mergestr_reloc, DiagnosticEngine::Trace,
     "Modified relocation %0 + %1 from relocation section %2 in file %3:\n\t "
     "Old fragment: offset %4 in section %5 "
     "from file %6\n\t New fragment: offset %7 in section %8 from file %9")
DIAG(splitting_merge_string_section, DiagnosticEngine::Verbose,
     "%0:(%1+0x%2) created mergeable string fragment with contents %3 and align "
     "%4")
DIAG(mapstyles_used, DiagnosticEngine::Verbose,
     "Using MapStyles %0")
DIAG(verbose_using_just_symbols, DiagnosticEngine::Verbose,
     "Using just symbols for input %0")
DIAG(verbose_inserting_section_at_fixed_addr, DiagnosticEngine::Trace,
     "Section %0 is placed at 0x%1 in output section %2")
DIAG(verbose_linker_script_output_file, DiagnosticEngine::Verbose,
     "Using output file name '%0' specified in the linker script '%1'")
DIAG(verbose_rule_matching_cache_func_hash, DiagnosticEngine::Verbose,
     "Rule-matching cache-functionailty hash: %0")
