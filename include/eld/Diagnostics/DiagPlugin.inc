//===- DiagPlugin.inc------------------------------------------------------===//
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

DIAG(unable_to_load_library, DiagnosticEngine::Error,
     "Unable to load library %0 due to %1")
DIAG(unable_to_find_func, DiagnosticEngine::Error,
     "Unable to find func %0 in library %1 due to %2")
DIAG(unable_to_find_plugin_func, DiagnosticEngine::Error,
     "Unable to find plugin func %0 in library %1 due to %2")
DIAG(unable_to_find_plugin_for_type, DiagnosticEngine::Error,
     "Unable to find plugin type %0 in library %1")
DIAG(unable_to_reserve_memory, DiagnosticEngine::Error,
     "Unable to find reserve memory type for %0 bytes")
DIAG(loaded_library, DiagnosticEngine::Note, "Loaded Library %0 requested")
DIAG(unloaded_library, DiagnosticEngine::Note, "Unloaded Library %0")
DIAG(found_register_function, DiagnosticEngine::Note,
     "Registration function found %0 in Library %1")
DIAG(found_cleanup_function, DiagnosticEngine::Note,
     "Cleanup function found %0 in Library %1")
DIAG(found_function_for_plugintype, DiagnosticEngine::Note,
     "Plugin handler %0 found in Library %1")
DIAG(registering_all_functions, DiagnosticEngine::Note,
     "Registering all plugin handlers for plugin types")
DIAG(found_plugin_handler, DiagnosticEngine::Note,
     "Found plugin handler for plugin type %0 in Library %1")
DIAG(allocating_memory, DiagnosticEngine::Note,
     "Creating MemoryBlock sized %0 for output section %1")
DIAG(applying_relocations, DiagnosticEngine::Note,
     "Applying relocations for section %0")
DIAG(syncing_relocations, DiagnosticEngine::Note,
     "Syncing relocations for section %0")
DIAG(adding_memory_blocks, DiagnosticEngine::Note,
     "Adding memory blocks for section %0")
DIAG(calling_handler, DiagnosticEngine::Note, "Running handler for section %0")
DIAG(plugin_returned_blocks, DiagnosticEngine::Note,
     "Plugin returned %0 blocks when processing section %1")
DIAG(plugin_returned_blocks_more_than_input_size, DiagnosticEngine::Fatal,
     "Plugin %0 returned size of memory block %1 more than input memory block "
     "size %2")
DIAG(plugin_request_memory, DiagnosticEngine::Note,
     "Plugin %0 requests memory %1 bytes and returned %2 bytes")
DIAG(plugin_destroy, DiagnosticEngine::Note, "Plugin %0 Destroyed")
DIAG(cannot_find_output_section_for_input, DiagnosticEngine::Error,
     "Cannot find output section for input section %0")
DIAG(output_section_contains_assignments, DiagnosticEngine::Note,
     "Output section %0 contains assignments, Disabling running the Order "
     "Plugin")
DIAG(plugin_mismatch, DiagnosticEngine::Error,
     "Plugin Type Mismatch in library %0 for plugin type %1")
DIAG(plugin_has_error, DiagnosticEngine::Error,
     "Plugin %0 defined in library %1 returned error %2 in state %3")
DIAG(plugin_config_error, DiagnosticEngine::Error,
     "Plugin Config file %0 cannot be parsed")
DIAG(plugin_config_parse_error, DiagnosticEngine::Error,
     "Syntax Error in Config file %0")
DIAG(no_output_section_for_plugin, DiagnosticEngine::Warning,
     "No output section %0 found for running plugin %1 specified")
DIAG(using_plugin, DiagnosticEngine::Note,
     "Using the absolute path %0 for library %1")
DIAG(output_section_override_not_present, DiagnosticEngine::Note,
     "Output section override set for section %0 from input file %1, but %2 "
     "not present, using default match")
DIAG(initializing_plugin, DiagnosticEngine::Note,
     "Initializing Plugin %1, requested by Plugin %0 having Name %2")
DIAG(running_plugin, DiagnosticEngine::Note,
     "Running Plugin %1, requested by Plugin %0 having Name %2")
DIAG(note_plugin_version, DiagnosticEngine::Note,
     "Plugin version %0.%1 in library %2 for plugin type %3")
DIAG(error_plugin_no_api_version, DiagnosticEngine::Error,
     "Plugin %0 in library %1 does not export getPluginAPIVersion function and "
     "will not be loaded. ")
DIAG(wrong_version, DiagnosticEngine::Error,
     "Plugin version mismatch in library %0 for plugin type %1 : Found version "
     "%2.%3, expected major version %4, minor version %5 or below")
DIAG(wrong_rule_selected, DiagnosticEngine::Warning,
     "Plugin %0 moved section %1 from input file %2 that violates rule %3")
DIAG(found_config_function, DiagnosticEngine::Note,
  "Plugin Config function %0 found in library %1")
DIAG(fatal_pending_section_override, DiagnosticEngine::Fatal,
     "Plugin '%0' has pending section overrides. Please call "
     "'LinkerWrapper::finishAssignOutputSections'")
DIAG(verbose_found_config_file, DiagnosticEngine::Verbose,
     "Found plugin config file %0")
DIAG(note_using_default_config_path, DiagnosticEngine::Note,
     "Using default config path for linker plugins")
DIAG(error_config_file_not_found, DiagnosticEngine::Error,
     "Plugin config file %0 not found")
DIAG(warn_pending_rule_insertion, DiagnosticEngine::Warning,
     "The rule '%0' created by '%1' has not been inserted to "
     "the output section '%2'")
DIAG(error_chunk_not_found, DiagnosticEngine::Error,
     "Chunk '%0' not found in rule '%1' for output section '%2'")
DIAG(err_chunk_removed_but_not_inserted, DiagnosticEngine::Error,
     "The chunk '%0' was removed from the rule '%1' by the plugin '%2' but "
     "has not been inserted back")
DIAG(err_multiple_chunk_add, DiagnosticEngine::Error, "The chunk '%0' cannot "
     "be added to the rule '%1' because it was previously added to the rule "
     "'%2' and has not been removed from there")
DIAG(err_chunk_inserted_but_not_removed, DiagnosticEngine::Error,
     "The chunk '%0' is inserted to the rule '%1' by the plugin '%2' but "
     "has not been removed from any rule")
DIAG(trace_plugin_init, DiagnosticEngine::Trace, "Calling %0::Init plugin hook")
DIAG(trace_plugin_destroy, DiagnosticEngine::Trace,
     "Calling %0::Destroy plugin hook")
DIAG(trace_plugin_register_opt_without_val, DiagnosticEngine::Trace,
     "Plugin '%0' registered command-line option '%1' without value")
DIAG(trace_plugin_register_opt_with_val, DiagnosticEngine::Trace,
     "Plugin '%0' registered command-line option '%1' with value")
DIAG(verbose_calling_plugin_opt_handler_with_val, DiagnosticEngine::Verbose,
     "Calling '%0::CommandLineOptionHandler' for option '%1' with value '%2'")
DIAG(verbose_calling_plugin_opt_handler_without_val, DiagnosticEngine::Verbose,
     "Calling '%0::CommandLineOptionHandler' for option '%1'")
DIAG(trace_plugin_visit_sections, DiagnosticEngine::Trace,
     "Calling plugin hook handler %0::VisitSections for the input file '%1'")
DIAG(trace_plugin_visit_symbol, DiagnosticEngine::Trace,
     "Calling plugin hook handler %0::VisitSymbol for the symbol '%1'")
DIAG(trace_plugin_enable_visit_symbol, DiagnosticEngine::Trace,
     "Enabled VisitSymbol hook for the plugin '%0'")
DIAG(trace_plugin_hook, DiagnosticEngine::Trace,
     "Calling plugin hook handler %0::%1")
DIAG(trace_show_RM_sect_name_in_diag, DiagnosticEngine::Trace,
     "Plugin '%0' has enabled the display of rule-matching section name "
     "in diagnostics")
DIAG(trace_set_aux_sym_name_map, DiagnosticEngine::Trace,
     "Plugin '%0' has set auxiliary symbol name map for the "
     "input file '%1'")
DIAG(error_invalid_input_section, DiagnosticEngine::Error,
     "Invalid input section '%0'")
DIAG(error_invalid_argument, DiagnosticEngine::Error,
     "Invalid argument for API call")
DIAG(error_plugin_name_not_unique, DiagnosticEngine::Error,
     "Plugin name %0 needs to be unique")
DIAG(verbose_find_default_plugins, DiagnosticEngine::Verbose,
     "Finding default plugin configuration in '%0'")
DIAG(error_finding_default_plugins, DiagnosticEngine::Error,
     "Error finding default plugin configuration: '%0'")
DIAG(error_finding_plugin_config, DiagnosticEngine::Error,
     "plugin configuration file not found: '%0'")
