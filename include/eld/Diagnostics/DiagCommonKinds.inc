//===- DiagCommonKinds.inc-------------------------------------------------===//
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

DIAG(cannot_open_search_dir, DiagnosticEngine::Note,
     "can not open search directory `%0'")
DIAG(err_no_inputs, DiagnosticEngine::Error, "no inputs")
DIAG(err_unrecognized_input_file, DiagnosticEngine::Fatal,
     "cannot recognize the format of file `%0'. object format or given target "
     "machine (%1) is incompatible.")
DIAG(err_cannot_find_namespec, DiagnosticEngine::Fatal,
     "cannot recognize namespec -l%0")
DIAG(verbose_trying_library, DiagnosticEngine::Verbose,
     "Trying to open `%0' for %1 `%2' (%3): %4")
DIAG(verbose_trying_input_file, DiagnosticEngine::Verbose,
     "Trying to open input `%0' of type `%1' for namespec `%2': %3")
DIAG(verbose_trying_script_include_file, DiagnosticEngine::Verbose,
     "Trying to open script include file `%0' for `%1': %2")
DIAG(err_mixed_shared_static_objects, DiagnosticEngine::Error,
     "cannot link shared objects with -static option. Found Shared object %0")
DIAG(err_unrecognized_member, DiagnosticEngine::Error,
     "`%0': Could not read archive member")
DIAG(err_no_ar_symtab, DiagnosticEngine::Error,
     "`%0': Could not read symbol table. Run ranlib to generate one.")
DIAG(err_no_extended_strtab, DiagnosticEngine::Unreachable,
     "`%0': No extended string table found.")
DIAG(err_cannot_read_symbol, DiagnosticEngine::Fatal,
     "can not read symbol[%0] in file %1")
DIAG(err_cannot_read_section, DiagnosticEngine::Fatal,
     "can not read section `%0'.")
DIAG(err_cannot_read_target_section, DiagnosticEngine::Fatal,
     "can not read target-dependent section `%0'.")
DIAG(err_unsupported_section, DiagnosticEngine::Fatal,
     "unsupported section `%0' (type %1)")
DIAG(warn_illegal_input_section, DiagnosticEngine::Warning,
     "section `%0' should not appear in input file `%1'")
DIAG(err_cannot_init_backend, DiagnosticEngine::Error,
     "Cannot initialize target backend for given triple '%0'. Available "
     "targets:\n %1")
DIAG(fatal_unwritable_output, DiagnosticEngine::Fatal,
     "unable to write output file %0 because of %1")
DIAG(warn_unsupported_option, DiagnosticEngine::Warning,
     "Unrecognized option `%0'")
DIAG(warn_incompatible_option, DiagnosticEngine::Warning,
     "Option `-%0' is disabled as it is incompatible with `-%1`")
DIAG(symdef_incompatible_option, DiagnosticEngine::Fatal,
     "SymDef files are valid only when building executables")
DIAG(warn_shared_textrel, DiagnosticEngine::Warning,
     "Add DT_TEXTREL in a shared object!")
DIAG(err_invalid_emulation, DiagnosticEngine::Error,
     "Invalid target emulation: `%0'.")
DIAG(err_cannot_find_scriptfile, DiagnosticEngine::Fatal,
     "cannot open %0 file %1")
DIAG(fatal_unsupported_ISA, DiagnosticEngine::Fatal,
     "Unsupported ISA flag for input %0: %1")
DIAG(error_call_gc_without_request, DiagnosticEngine::Error,
     "Attempted to run garbage collection without a prior request")
DIAG(no_entry_sym_for_GC, DiagnosticEngine::Error,
     "Garbage collection cannot continue due to missing or undefined entry "
     "symbol. Use --entry=<symbol>")
DIAG(fatal_unsupported_emulation, DiagnosticEngine::Fatal,
     "Invalid Emulation %0")
DIAG(warn_entry_symbol_not_found, DiagnosticEngine::Warning,
     "%0 Entry symbol not found, defaulting to %1")
DIAG(warn_entry_symbol_bad_value, DiagnosticEngine::Warning,
     "%0 Entry symbol has bad value, defaulting to %1")
DIAG(warn_unknown_verify_type, DiagnosticEngine::Warning,
     "Unknown verify type %0, ignoring.")
DIAG(warn_group_is_empty, DiagnosticEngine::Warning, "Empty --start-group --end-group.")
DIAG(nested_group_not_allowed, DiagnosticEngine::Fatal,
     "Nested --start-group/--end-group not allowed")
DIAG(mismatched_group, DiagnosticEngine::Fatal,
     "Mismatched --start-group/--end-group")
DIAG(unsupported_version_node, DiagnosticEngine::Warning,
     "Only anonymous version nodes are suppored. Ignoring version script `%0'")
DIAG(unsupported_dependent_node, DiagnosticEngine::Warning,
     "Dependent nodes are not suppored. Ignoring dependency for node '%0' "
     "in version script '%1'")
DIAG(unable_to_write_output_file, DiagnosticEngine::Error,
     "Unable to write output file %0 because of error %1")
DIAG(unable_to_write_temporary_file, DiagnosticEngine::Fatal,
     "Unable to write output file %0 because of error %1")
DIAG(warn_strip_symbols, DiagnosticEngine::Warning,
     "Strip symbols is disabled, as its incompatible with %0")
DIAG(unable_to_add_ini_hash_entry, DiagnosticEngine::Fatal,
     "Unable to add hash entry to mapping.ini")
DIAG(unable_to_write_reproduce_tarball, DiagnosticEngine::Fatal,
     "Unable to write reproduce tarball")
DIAG(unable_to_add_version_file, DiagnosticEngine::Fatal,
     "Unable to add version file %0 to tarball  %1")
DIAG(unable_to_find_mapping_file, DiagnosticEngine::Fatal,
     "Could not read mapping file %0")
DIAG(unable_to_compress_tar, DiagnosticEngine::Fatal,
     "Could not compress data for file %0")
DIAG(unable_to_write_compressed_tar, DiagnosticEngine::Fatal,
     "Error in writing file %0")
DIAG(unable_to_find_library, DiagnosticEngine::Fatal,
     "Unable to find library %0")
DIAG(unexpected_linker_behavior, DiagnosticEngine::Fatal,
     "!!!UNEXPECTED LINKER BEHAVIOR!!!")
DIAG(linker_crash_use_reproduce, DiagnosticEngine::Fatal,
     "Please rerun link with %0 and contact "
     "support")
DIAG(plugin_crash, DiagnosticEngine::Fatal,
     "!!!PLUGIN CRASHED!!!\nUser specified plugin %0 caused segmentation fault")
DIAG(unable_to_create_temporary_file, DiagnosticEngine::Fatal,
     "Unable to create temporary file %0")
DIAG(unable_to_write_json_file, DiagnosticEngine::Error,
     "Unable to write JSON file %0 due to error %1")
DIAG(unsupported_linker_flavor, DiagnosticEngine::Error,
     "Unsupported linker flavor %0")
DIAG(unable_to_compress, DiagnosticEngine::Fatal, "Unable to compress %0")
DIAG(thread_count_with_no_threads, DiagnosticEngine::Error,
     "Cannot disable threads and specify thread count")
DIAG(invalid_value_for_option, DiagnosticEngine::Error,
     "Invalid value for %0: %1")
DIAG(invalid_option, DiagnosticEngine::Error,
     "Invalid option %0 specified for %1")
DIAG(invalid_option_mapstyle, DiagnosticEngine::Error,
     "Invalid options specified for -MapStyle. Valid values are "
     "txt/gnu/yaml/llvm/all")
DIAG(invalid_option_error_style, DiagnosticEngine::Error,
     "error-style can only take options llvm/gnu")
DIAG(invalid_option_match_error_style, DiagnosticEngine::Error,
     "error-style can only take options match-llvm/match-gnu")
DIAG(enable_disable_newdtags, DiagnosticEngine::Error,
     "Cannot enable and disable DTAGS at the same time")
DIAG(cannot_find_target, DiagnosticEngine::Error, "Unable to find target: %0")
DIAG(unique_output_sections_unsupported, DiagnosticEngine::Note,
     "--unique-output-sections is supported only with -r/partial link")
DIAG(fatal_mapped_file_not_found, DiagnosticEngine::Fatal,
     "Expected mapped file '%0' (mapping of '%1') not found")
DIAG(unable_to_find_mergestr_fragment, DiagnosticEngine::Error,
     "Unable to find merge string fragment at offset %0 in section %1 from "
     "file %2")
DIAG(error_invalid_option_symdef_style, DiagnosticEngine::Error,
     "Invalid options specified for -symdef-style=%0 valid values are "
     "default/provide")
DIAG(error_unknown_symdef_style, DiagnosticEngine::Error,
     "Unknown symdef style detected")
DIAG(note_eld_flags_without_output_name, DiagnosticEngine::Note, "ELDFlags: '%0'")
DIAG(note_eld_flags, DiagnosticEngine::Note, "ELDFlags(%0): '%1'")
DIAG(warn_gc_sections_relocatable, DiagnosticEngine::Warning,
     "--gc-sections has no effect when building a relocatable file")
DIAG(error_missing_arg_value, DiagnosticEngine::Error,
     "Missing arg value for '%0' expected %1 argument(s)")
DIAG(err_invalid_image_base, DiagnosticEngine::Error,
     "--image-base: number expected, but got %0")
DIAG(warn_image_base_not_multiple_page_size, DiagnosticEngine::Warning,
     "--image-base: address isn't multiple of page size: %0")
DIAG(error_map_detail_absrel_path, DiagnosticEngine::Error,
     "The absolute-path and relative-path MapDetail options can "
     "not be used together")
DIAG(warn_unable_to_compute_relpath, DiagnosticEngine::Warning,
     "Unable to compute relative path of '%0' for '%1'")
DIAG(verbose_using_basepath_for_mapfiles, DiagnosticEngine::Verbose,
     "Using '%0' for computing relative paths in map files")
DIAG(fatal_invalid_diag_id, DiagnosticEngine::Fatal,
     "Invalid diagnostic ID: %0")
DIAG(fatal_missing_diag_args, DiagnosticEngine::Fatal,
     "Missing argument %0 when reporting diagnostic '%1'")
DIAG(error_invalid_input_format, DiagnosticEngine::Error,
     "Invalid input format '%0'. Supported values: 'binary', 'default'")
DIAG(warn_unknown_trace_option, DiagnosticEngine::Warning,
     "Unknown trace option '%0'")
DIAG(err_shared_objects_in_partial_link, DiagnosticEngine::Error,
    "Shared libraries are not allowed with partial links, found %0")
DIAG(err_patch_base_not_executable, DiagnosticEngine::Error,
     "The file %0, specified by --patch-base, must be an executable ELF file")
DIAG(err_patch_not_static, DiagnosticEngine::Error,
     "Patching is only supported for static linking")
DIAG(error_invalid_target2, DiagnosticEngine::Error,
     "unknown --target2 option: %0")
