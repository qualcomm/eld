" Vim syntax file
" Language:     ELD Linker Map File / Linker Script
" Maintainer:   ELD toolchain team
" Description:  Syntax highlighting for ELD (LLVM-based) linker map files
"               (*.map) and ELD linker scripts (*.ld, *.lds, *.lcs, *.t, *.x).
"               Both file types share this syntax because the map file embeds
"               the original linker-script source text verbatim in its
"               "# Output Section and Layout" section.
"
" File types:   Set via ftdetect/eld.vim (filetype=eld).
"               Manually:  :set filetype=eld
"
" Syntax groups overview
" ──────────────────────
"  Comments & metadata
"    eldComment          # line comments and // line comments
"    eldMetaKey          key label inside a # Key: value header line
"    eldBanner           top-level section headings in map files
"    eldLsComment        /* block comments */ (linker scripts)
"
"  Map-file structure
"    eldLoad / eldLoadPath        LOAD path[arch] lines
"    eldPullIn / eldSymbolInParens  archive pull-in pairs
"    eldOutputSection / eldSectionMeta  .name addr size # Offset: … lines
"    eldInputSection / eldSectionAttr   .name.sub addr size origin #SHT_…
"    eldGCSection / eldGCTag / eldGCSymbol  garbage-collected sections
"    eldSymbolLine / eldSymbol    indented address + symbol lines
"    eldPadding / eldPaddingKw    PADDING_ALIGNMENT lines
"    eldSHT / eldSHF              SHT_* and SHF_* type/flag tokens
"    eldSectionMeta / eldMetaKey2 # Offset: LMA: Alignment: … metadata
"
"  Rule lines (shared by map files and linker scripts)
"    eldRule             map-file rule:   *(pat) #Rule N, …
"    eldLsRule           script rule:     *(pat)  (no #Rule suffix)
"    eldRuleKw           KEEP / KEEP_DONTMOVE prefix keyword
"    eldRulePattern      the *(…) glob token
"    eldRuleGlob         the leading * wildcard
"    eldRuleSectionName  .section.name inside a pattern
"    eldRuleComment      #Rule N, source [n:n] tail
"    eldLsSortConstructors  SORT(CONSTRUCTORS) special form
"
"  Expression / assignment lines
"    eldExprLine         whole-line region for any assignment
"    eldExprLHS          symbol name on the left-hand side
"    eldExprEval         (0x…) evaluated value injected by ELD
"    eldExprOp           = += ? : ! < > & | ^ ~ operators
"    eldExprParen        structural ( )
"    eldExprDot          location counter . used as a value
"    eldExprComment      # original_expr; source trailing comment
"
"  Linker-script specific
"    eldLsString         "string literals"
"    eldLsBrace          { } section body delimiters
"    eldLsOutputSection  .name [addr] [type] : header line
"    eldLsSecAttr        (NOLOAD) / (DSECT) / … type annotation
"    eldLsFill           } =0x… fill value after section body
"    eldLsAssign         plain sym = expr; assignment (no eval tokens)
"    eldLsAssignLHS/Op/Dot/Comment  sub-items of eldLsAssign
"
"  Keywords (all use explicit colors; see "Keyword colors" section)
"    eldKwSectionList    KEEP EXCLUDE_FILE SORT* COMMON CONSTRUCTORS
"    eldKwAssign         PROVIDE PROVIDE_HIDDEN HIDDEN ASSERT
"    eldKwExpr           ALIGN DEFINED SIZEOF MAX MIN … (expression fns)
"    eldKwSecType        NOLOAD DSECT ONLY_IF_RO AT SUBALIGN FILL …
"    eldKwData           BYTE SHORT LONG QUAD SQUAD
"    eldKwTopLevel       SECTIONS MEMORY ENTRY OUTPUT_FORMAT …
"    eldKwInput          INCLUDE AS_NEEDED INPUT GROUP
"    eldKwGroup          START GROUP / END GROUP
"    eldKwDiscard        /DISCARD/
"    eldKwSegType        PT_LOAD PT_GNU_STACK … (PHDRS segment types)
"
"  Shared atoms
"    eldHex              0x… hex literals
"    eldSectionName      .section.name token
"    eldArchiveMember    lib.a(member.o)
"    eldArch             [arch] tag on LOAD lines
"    eldOriginFile       trailing origin path on input-section lines
"    eldScriptFile       bare linker-script filename in "scripts used" list
"
"  Folding (map files only)
"    Level 0 — only banner lines visible
"    Level 1 — banner open; output-section fold headers visible
"    Level 2 — output section open; all content visible
"    Special single-line folds: # CommandLine, # LinkStats Begin…End

if exists("b:current_syntax")
  finish
endif

" ════════════════════════════════════════════════════════════════════════════
" COMMENTS
" ════════════════════════════════════════════════════════════════════════════

" # line comments (map file header, linker-script # comments)
" // line comments (C++ style; treated identically by the ELD lexer)
" Both patterns share the same group so they get the same color.
" The contains= list allows sub-highlighting of metadata keys, GC sections,
" GC symbol names, and hex numbers that appear inside # comment lines.
syn match eldComment /^#.*$/
      \ contains=eldMetaKey,eldGCSection,eldGCSymbol,eldHex
syn match eldComment +//.*$+
      \ contains=eldMetaKey,eldGCSection,eldGCSymbol,eldHex

" Key label inside a map-file header comment:  # Key : value
" Pattern: # followed by a word that ends with ':'
syn match eldMetaKey /^#\s*[A-Za-z][A-Za-z0-9 _]*\s*:/ contained

" /* block comments */ — linker scripts only; may span multiple lines.
" 'fold' lets Vim fold long comment blocks independently.
syn region eldLsComment start="/\*" end="\*/" contains=@Spell fold

" ════════════════════════════════════════════════════════════════════════════
" MAP-FILE BANNERS
" ════════════════════════════════════════════════════════════════════════════

" Top-level section headings that appear in ELD map files.
" Each banner is the fold label for a level-1 fold (see Folding section).
" 'Linker scripts used' is matched with \> (end-of-word) because the actual
" line reads "Linker scripts used (including INCLUDE command)".
syn match eldBanner /^Linker Script and memory map$/
syn match eldBanner /^Archive member included because of file (symbol)$/
syn match eldBanner /^Build Statistics$/
syn match eldBanner /^Linker scripts used\>.*$/
syn match eldBanner /^Linker Plugin Information$/
syn match eldBanner /^Linker Plugin Run Information$/
syn match eldBanner /^# Output Section and Layout\s*$/
syn match eldBanner /^Discarded input sections$/
syn match eldBanner /^Memory Configuration$/
syn match eldBanner /^Cross Reference Table$/
syn match eldBanner /^Common symbol\s\+size\s\+file$/

" ════════════════════════════════════════════════════════════════════════════
" MAP-FILE LOAD / ARCHIVE LINES
" ════════════════════════════════════════════════════════════════════════════

" LOAD path/to/file.o[arch]
" The keyword 'LOAD' is matched first; the rest of the line is eldLoadPath
" which in turn highlights archive members and architecture tags.
syn match eldLoad     /^LOAD\s\+/ nextgroup=eldLoadPath
syn match eldLoadPath /.*$/ contained contains=eldArchiveMember,eldArch

" Archive pull-in pairs (two-line form in the "Archive member included" block):
"   lib.a(member.o)
"           lib.a(requirer.o) (symbol_that_pulled_it_in)
" The indented second line starts with two hard tabs.
syn match eldPullIn         /^\t\t\S.*$/
      \ contains=eldArchiveMember,eldSymbolInParens
syn match eldSymbolInParens /(\S\+)\s*$/ contained

" ════════════════════════════════════════════════════════════════════════════
" MAP-FILE OUTPUT / INPUT SECTION LINES
" ════════════════════════════════════════════════════════════════════════════

" Output section header line (map file):
"   .text   0xe7a0   0xc7e5f # Offset: 0xe7a0, LMA: 0xe7a0, Alignment: 0x8, …
" Distinguished from input-section lines by the '# Offset:' tail.
syn match eldOutputSection
      \ /^\.[a-zA-Z_][a-zA-Z0-9_.+-]*\s\+0x[0-9a-fA-F]\+\s\+0x[0-9a-fA-F]\+\s*#/
      \ contains=eldSectionName,eldHex,eldSectionMeta

" Input section / subsection line (map file):
"   .text.xmalloc   0x10e65   0x20   libbb/lib.a(xfuncs_printf.o)   #SHT_…
" Distinguished from output-section lines by having a non-# character after
" the two hex fields (the origin file path).
syn match eldInputSection
      \ /^\.[a-zA-Z_][a-zA-Z0-9_.+-]*\s\+0x[0-9a-fA-F]\+\s\+0x[0-9a-fA-F]\+\s\+\S/
      \ contains=eldSectionName,eldHex,eldOriginFile,eldSectionAttr

" Garbage-collected section lines appear inside # comments:
"   # .text.foo   <GC>   0x20   lib.a(foo.o)   #SHT_PROGBITS,SHF_ALLOC,4
" They are 'contained' so they only fire inside eldComment regions.
syn match eldGCSection /^#\s\+\.[a-zA-Z_][a-zA-Z0-9_.+-]*\s\+<GC>.*$/
      \ contained contains=eldGCTag,eldSectionName,eldHex,eldSectionAttr
syn match eldGCTag /<GC>/ contained

" GC'd symbol name line inside a # comment:  #   symbol_name
" Appears on the line immediately after a GC section line.
syn match eldGCSymbol /^#\s\+[a-zA-Z_$][a-zA-Z0-9_$.@]*\s*$/ contained

" Symbol address line (map file):
"     0x10e65   xmalloc
" Leading whitespace + hex address + symbol name, nothing else on the line.
syn match eldSymbolLine /^\s\+0x[0-9a-fA-F]\+\s\+[a-zA-Z_$][a-zA-Z0-9_$.@]*\s*$/
      \ contains=eldHex,eldSymbol
syn match eldSymbol /[a-zA-Z_$][a-zA-Z0-9_$.@]*\s*$/ contained

" PADDING_ALIGNMENT line (map file):
"   PADDING_ALIGNMENT   0xef74   0x4   0x0
syn match eldPadding   /^PADDING_ALIGNMENT\s.*$/
      \ contains=eldPaddingKw,eldHex
syn match eldPaddingKw /^PADDING_ALIGNMENT/ contained

" Section attribute annotation appended to input-section lines:
"   #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,4
" The three comma-separated fields are: type, flags, alignment.
syn match eldSectionAttr /#SHT_[A-Za-z0-9_]\+,[A-Za-z0-9_|]\+,[0-9]\+/ contained
      \ contains=eldSHT,eldSHF
" SHT_* section-type names read from /usr/include/elf.h
" Only names with plain literal values are included; names computed
" from other macros (e.g. SHT_LOPROC + 1) are omitted because they
" cannot appear literally in linker scripts or map files.
syn keyword eldSHT contained
      \ SHT_ALPHA_DEBUG SHT_ALPHA_REGINFO SHT_CHECKSUM SHT_DYNAMIC SHT_DYNSYM
      \ SHT_FINI_ARRAY SHT_GNU_ATTRIBUTES SHT_GNU_HASH SHT_GNU_LIBLIST
      \ SHT_GNU_verdef SHT_GNU_verneed SHT_GNU_versym SHT_GROUP SHT_HASH
      \ SHT_HIOS SHT_HIPROC SHT_HISUNW SHT_HIUSER SHT_INIT_ARRAY SHT_LOOS
      \ SHT_LOPROC SHT_LOSUNW SHT_LOUSER SHT_MIPS_ABIFLAGS SHT_MIPS_AUXSYM
      \ SHT_MIPS_CONFLICT SHT_MIPS_CONTENT SHT_MIPS_DEBUG SHT_MIPS_DELTACLASS
      \ SHT_MIPS_DELTADECL SHT_MIPS_DELTAINST SHT_MIPS_DELTASYM SHT_MIPS_DENSE
      \ SHT_MIPS_DWARF SHT_MIPS_EH_REGION SHT_MIPS_EVENTS SHT_MIPS_EXTSYM
      \ SHT_MIPS_FDESC SHT_MIPS_GPTAB SHT_MIPS_IFACE SHT_MIPS_LIBLIST
      \ SHT_MIPS_LINE SHT_MIPS_LOCSTR SHT_MIPS_LOCSYM SHT_MIPS_MSYM
      \ SHT_MIPS_OPTIONS SHT_MIPS_OPTSYM SHT_MIPS_PACKAGE SHT_MIPS_PACKSYM
      \ SHT_MIPS_PDESC SHT_MIPS_PDR_EXCEPTION SHT_MIPS_PIXIE SHT_MIPS_REGINFO
      \ SHT_MIPS_RELD SHT_MIPS_RFDESC SHT_MIPS_SHDR SHT_MIPS_SYMBOL_LIB
      \ SHT_MIPS_TRANSLATE SHT_MIPS_UCODE SHT_MIPS_WHIRL SHT_MIPS_XHASH
      \ SHT_MIPS_XLATE SHT_MIPS_XLATE_DEBUG SHT_MIPS_XLATE_OLD SHT_NOBITS
      \ SHT_NOTE SHT_NULL SHT_NUM SHT_PARISC_DOC SHT_PARISC_EXT
      \ SHT_PARISC_UNWIND SHT_PREINIT_ARRAY SHT_PROGBITS SHT_REL SHT_RELA
      \ SHT_RELR SHT_SHLIB SHT_STRTAB SHT_SUNW_COMDAT SHT_SUNW_move
      \ SHT_SUNW_syminfo SHT_SYMTAB SHT_SYMTAB_SHNDX SHT_X86_64_UNWIND
" SHF_* section-flag names read from /usr/include/elf.h
syn keyword eldSHF contained
      \ SHF_ALLOC SHF_ALPHA_GPREL SHF_ARM_COMDEF SHF_ARM_ENTRYSECT
      \ SHF_COMPRESSED SHF_EXCLUDE SHF_EXECINSTR SHF_GNU_RETAIN SHF_GROUP
      \ SHF_IA_64_NORECOV SHF_IA_64_SHORT SHF_INFO_LINK SHF_LINK_ORDER
      \ SHF_MASKOS SHF_MASKPROC SHF_MERGE SHF_MIPS_ADDR SHF_MIPS_GPREL
      \ SHF_MIPS_LOCAL SHF_MIPS_MERGE SHF_MIPS_NAMES SHF_MIPS_NODUPE
      \ SHF_MIPS_NOSTRIP SHF_MIPS_STRINGS SHF_ORDERED SHF_OS_NONCONFORMING
      \ SHF_PARISC_HUGE SHF_PARISC_SBP SHF_PARISC_SHORT SHF_STRINGS SHF_TLS
      \ SHF_WRITE

" Metadata comment on output-section header lines:
"   # Offset: 0xe7a0, LMA: 0xe7a0, Alignment: 0x8, Flags: SHF_ALLOC, Type: SHT_PROGBITS
syn match eldSectionMeta /#\s*Offset:.*$/ contained
      \ contains=eldMetaKey2,eldHex,eldSHT,eldSHF
syn match eldMetaKey2 /\(Offset\|LMA\|Alignment\|Flags\|Type\):/ contained

" ════════════════════════════════════════════════════════════════════════════
" RULE LINES  (shared by map files and linker scripts)
" ════════════════════════════════════════════════════════════════════════════
"
" A "rule" is an input-section specifier inside an output-section body.
" It takes one of these forms:
"
"   *(pattern)                          bare glob
"   KEEP (*(.eh_frame))                 keyword-wrapped, outer parens
"   KEEP (*( SORT_BY_INIT_PRIORITY(…))) nested sort keyword
"   *(COMMON)                           special COMMON pseudo-section
"   SORT(CONSTRUCTORS)                  special CONSTRUCTORS form
"
" Map files append a '#Rule N, source [n:n]' suffix; linker scripts do not.
" Two separate top-level matches handle the two cases so the #Rule comment
" is only highlighted in map files.

syn match eldLsRule /\*([^)]*)\(\s*#Rule\)\@!/
      \ contains=eldRuleGlob,eldRuleSectionName,eldKwSectionList

" Keyword prefix on a rule line (KEEP, KEEP_DONTMOVE, …).
" Two definitions: one 'contained' (used inside eldRule) and one top-level
" (used on linker-script lines where eldRule does not fire).
syn match eldRuleKw /^\s*[A-Z_]\+\ze\s\+[(*]/ contained
syn match eldRuleKw /^\s*[A-Z_]\+\ze\s\+[(*]/

" Map-file rule line: optional keyword prefix + *(pattern) + #Rule suffix.
" The (\? and )\? allow for the outer parens in KEEP (*(...)) form.
syn match eldRule
      \ /^\t*\([A-Z_]\+\s\+\)\?(\?\*([^#]*)\s*)\?\s*#Rule.*$/
      \ contains=eldRuleKw,eldRulePattern,eldRuleComment

" Linker-script rule line: *(pattern) anywhere on a line, no #Rule suffix.
" The negative lookahead \(\s*#Rule\)\@! prevents double-matching map lines.
" This also fires on inline rules: foo : { *(.text) }

" The *(section-list) glob token.
" Contains sub-items so that section names and sort keywords inside it are
" highlighted individually.
syn match eldRulePattern /\*([^)]*)/ contained
      \ contains=eldRuleGlob,eldRuleSectionName,eldKwSectionList

" The leading * wildcard character inside a rule pattern.
syn match eldRuleGlob /\*/ contained

" Section name inside a rule pattern: must start with '.' (e.g. .text.hot.*).
syn match eldRuleSectionName /\.[a-zA-Z_*?][a-zA-Z0-9_.+\-*?]*/ contained

" #Rule N, source_file [matched:total] — map-file only tail comment.
syn match eldRuleComment /#Rule.*$/ contained

" SORT(CONSTRUCTORS) — special form that sorts constructor functions.
" 'SORT' and 'CONSTRUCTORS' are both in eldKwSectionList so they get the
" section-list keyword color; the surrounding parens are unhighlighted.
syn match eldLsSortConstructors /\<SORT\s*(\s*CONSTRUCTORS\s*)/
      \ contains=eldKwSectionList

" ════════════════════════════════════════════════════════════════════════════
" KEYWORDS
" ════════════════════════════════════════════════════════════════════════════
"
" All keyword groups are defined here.  Most are 'contained' so they only
" fire inside the regions that explicitly list them; this prevents false
" matches on banner lines, file paths, and other non-keyword contexts.
"
" Priority note: 'syn keyword' has higher priority than 'syn match' at the
" same position.  Groups that need to be overridden by a surrounding match
" (e.g. eldKwAssign inside eldExprLine, eldKwSectionList inside eldRule)
" are marked 'contained' so the outer match wins at the line level.

" Section-list keywords: appear inside *(…) input-section patterns and as
" prefixes on rule lines.  Also includes COMMON and CONSTRUCTORS which are
" special pseudo-section names used without a leading dot.
" 'contained' so eldRule / eldLsRule win at line start on KEEP lines.
syn keyword eldKwSectionList contained
      \ KEEP KEEP_DONTMOVE
      \ EXCLUDE_FILE
      \ SORT SORT_BY_NAME SORT_BY_ALIGNMENT SORT_BY_INIT_PRIORITY SORT_NONE
      \ COMMON CONSTRUCTORS

" Symbol / assignment commands: wrap an assignment expression.
" 'contained' so eldExprLine wins at line start on PROVIDE_HIDDEN(…) lines.
syn keyword eldKwAssign PROVIDE PROVIDE_HIDDEN HIDDEN ASSERT contained

" Expression built-in functions: appear in the RHS of assignments and in
" section-address expressions.  Sourced from ScriptParser.cpp.
syn keyword eldKwExpr
      \ ALIGN ALIGNOF ADDR LOADADDR SIZEOF SIZEOF_HEADERS
      \ DEFINED ABSOLUTE NEXT MAX MIN LOG2CEIL
      \ SEGMENT_START ORIGIN LENGTH
      \ DATA_SEGMENT_ALIGN DATA_SEGMENT_END DATA_SEGMENT_RELRO_END
      \ CONSTANT COMMONPAGESIZE MAXPAGESIZE
      \ NEXT_SECTION LINKER_VERSION

" Output-section type and attribute keywords.
" These appear between the section name and the ':' in a linker script, or
" as standalone attributes inside a section body.
syn keyword eldKwSecType
      \ NOLOAD DSECT COPY INFO OVERLAY PROGBITS UNINIT
      \ ONLY_IF_RO ONLY_IF_RW ALIGN_WITH_INPUT
      \ AT SUBALIGN FILL FLAGS

" Data-output commands: emit raw bytes into the current section.
syn keyword eldKwData BYTE SHORT LONG QUAD SQUAD

" Top-level linker-script commands: appear at the outermost scope.
" Sourced from ScriptParser.cpp.
syn keyword eldKwTopLevel
      \ ENTRY EXTERN OUTPUT OUTPUT_FORMAT OUTPUT_ARCH
      \ SEARCH_DIR MEMORY SECTIONS PHDRS VERSION
      \ NOCROSSREFS REGION_ALIAS INSERT PRINT
      \ LINKER_PLUGIN PLUGIN_CONTROL_FILESZ PLUGIN_CONTROL_MEMSZ
      \ PLUGIN_ITER_SECTIONS PLUGIN_OUTPUT_SECTION_ITER PLUGIN_SECTION_MATCHER
      \ INCLUDE_OPTIONAL DONTMOVE

" Input / group control keywords.
" GROUP is also matched as part of 'START GROUP' / 'END GROUP' below.
syn keyword eldKwInput INCLUDE AS_NEEDED INPUT GROUP

" START GROUP … END GROUP — matched as a two-word phrase so the whole
" phrase gets the group color rather than just the individual words.
syn match eldKwGroup /\<\(START\|END\)\s\+GROUP\>/

" PT_* program-header type names read from /usr/include/elf.h
syn keyword eldKwSegType
      \ PT_DYNAMIC PT_GNU_EH_FRAME PT_GNU_PROPERTY PT_GNU_RELRO PT_GNU_SFRAME
      \ PT_GNU_STACK PT_HIOS PT_HIPROC PT_HISUNW PT_INTERP PT_LOAD PT_LOOS
      \ PT_LOPROC PT_LOSUNW PT_MIPS_ABIFLAGS PT_MIPS_OPTIONS PT_MIPS_REGINFO
      \ PT_MIPS_RTPROC PT_NOTE PT_NULL PT_NUM PT_PARISC_ARCHEXT
      \ PT_PARISC_UNWIND PT_PHDR PT_SHLIB PT_SUNWBSS PT_SUNWSTACK PT_TLS

" /DISCARD/ pseudo-section: discards all matching input sections.
" Matched as a pattern (not a keyword) because of the surrounding slashes.
syn match eldKwDiscard /\/DISCARD\//

" ════════════════════════════════════════════════════════════════════════════
" EXPRESSION / ASSIGNMENT LINES
" ════════════════════════════════════════════════════════════════════════════
"
" Expression lines appear in both map files and linker scripts.  In map
" files ELD injects the evaluated value after each symbol/call in parens:
"
"   .(0x1000) = ALIGN(.(0x1c), 0x1000); # . = ALIGN(., 0x1000); script.t
"   PROVIDE_HIDDEN(__start(0x0) = .(0x0)); # PROVIDE_HIDDEN(…); script.t
"
" In linker scripts the evaluated values are absent:
"
"   . = ALIGN(4K);
"   PROVIDE_HIDDEN ( __preinit_array_start = . );
"
" The map-file form is handled by eldExprLine (which expects ';' on the line).
" The linker-script form is handled by eldLsAssign (see below).
"
" Priority ordering of contained items matters:
"   eldExprEval  defined after eldExprParen  → (0x…) wins over bare (
"   eldExprLHS   defined after eldExprEval   → identifiers win over eval
"   eldKwAssign  is 'contained'              → eldExprLine wins at line start

" Whole-line region for map-file expression lines.
" Matches any line containing ';' that optionally starts with a keyword.
" All sub-items are listed in 'contains=' so they fire inside this region.
syn match eldExprLine
      \ /^\t*\(PROVIDE\|PROVIDE_HIDDEN\|HIDDEN\|ASSERT\)\?[^\n]*;.*$/
      \ contains=eldKwAssign,eldKwExpr,eldKwSectionList,eldExprEval,eldExprLHS,
      \           eldExprOp,eldExprParen,eldExprComment,eldExprDot,eldHex

" Operators: = += -= *= /= ? : ! != < > <= >= & | ^ ~
" Defined before eldExprParen so a bare '=' is not swallowed by paren matching.
syn match eldExprOp /[=!<>?:+\-*&|^~]\+/ contained

" Structural parentheses ( ).
" Defined before eldExprEval so that (0x…) can override bare ( below.
syn match eldExprParen /[()]/ contained

" Evaluated value injected by ELD: (0x1234) immediately after a symbol/call.
" Defined AFTER eldExprParen so it wins at the same start position — Vim
" gives priority to the later-defined contained item when both start at the
" same column.
syn match eldExprEval /(0x[0-9a-fA-F]\+)/ contained

" Any identifier in an expression (LHS symbol, function argument, etc.).
" Defined last among the contained items so keywords (eldKwExpr, eldKwAssign)
" defined earlier take priority over plain identifiers at the same position.
syn match eldExprLHS /[a-zA-Z_][a-zA-Z0-9_.]*/  contained

" Location counter '.' used as a value.
" The \ze lookahead ensures we only match '.' that is followed by a
" non-identifier character (so .section.names are not matched here).
syn match eldExprDot /\.\ze[^a-zA-Z0-9_\-]/ contained

" Trailing comment after the semicolon in a map-file expression line:
"   ; # original_expr; source_file
syn match eldExprComment /#.*$/ contained

" ════════════════════════════════════════════════════════════════════════════
" LINKER-SCRIPT SPECIFIC SYNTAX
" ════════════════════════════════════════════════════════════════════════════
"
" Items in this section only appear in linker scripts, not in map files.
" They are defined as top-level matches so they fire without needing a
" surrounding region — linker scripts have no equivalent of the map file's
" structured sections.

" String literals: used in OUTPUT_FORMAT("elf32-littlehexagon", …)
syn region eldLsString start='"' end='"' oneline

" Section body delimiters { }.
syn match eldLsBrace /[{}]/

" Output section header in a linker script:
"   .section_name [address] [(type)] :
" Matches a line that starts with a dot-name and ends with ':'.
" The [^{]* in the middle allows for address expressions and type annotations
" without accidentally matching lines that open the body on the same line.
syn match eldLsOutputSection /^\s*\.[a-zA-Z_][a-zA-Z0-9_.+-]*\s*[^{]*:/
      \ contains=eldSectionName,eldHex,eldLsSecAttr,eldKwSecType

" Section type annotation between the section name and ':':
"   .heap (NOLOAD) :
" The keyword inside the parens is highlighted via eldKwSecType.
syn match eldLsSecAttr /(\s*\(NOLOAD\|DSECT\|COPY\|INFO\|OVERLAY\)\s*)/ contained
      \ contains=eldKwSecType

" Fill value appended after the closing brace of a section body:
"   } =0x00c0007f
" The brace and hex value are highlighted as sub-items.
syn match eldLsFill /}\s*=\s*0x[0-9a-fA-F]\+/
      \ contains=eldLsBrace,eldHex

" Plain assignment in a linker script (no ELD-injected eval tokens):
"   sym = expr;      . = ALIGN(4K);      sym += value;
" Distinct from eldExprLine which requires the (0x…) eval tokens of map files.
" The pattern anchors to line start and requires a terminating ';'.
syn match eldLsAssign /^\s*[a-zA-Z_.][a-zA-Z0-9_.]*\s*[+\-*\/]\?=\s*[^;]\+;/
      \ contains=eldLsAssignLHS,eldKwExpr,eldKwSectionList,eldHex,
      \           eldLsAssignOp,eldLsAssignComment,eldLsAssignDot

" LHS of a linker-script assignment (symbol name or '.').
syn match eldLsAssignLHS /^\s*[a-zA-Z_.][a-zA-Z0-9_.]*/  contained

" Assignment operator: = += -= *= /=
syn match eldLsAssignOp  /[+\-*\/]\?=/                    contained

" Location counter '.' used as a value in a linker-script assignment.
syn match eldLsAssignDot /\b\.\b/                          contained

" Inline /* comment */ inside a linker-script assignment line.
syn match eldLsAssignComment /\/\*.*\*\//                  contained

" ════════════════════════════════════════════════════════════════════════════
" SHARED ATOMS
" ════════════════════════════════════════════════════════════════════════════

" Hex literals: 0x[0-9a-fA-F]+
" 'contained' — only fires inside regions that explicitly list it, preventing
" false matches on hex-like strings in file paths or symbol names.
syn match eldHex /\<0x[0-9a-fA-F]\+\>/ contained

" Section name token: starts with '.' followed by word/punctuation chars.
" Used both as a standalone item (linker-script output section headers) and
" as a contained item inside rule patterns and input-section lines.
syn match eldSectionName /\.[a-zA-Z_][a-zA-Z0-9_.+-]*/ contained

" Archive member reference: lib.a(member.o)
" Appears on LOAD lines, pull-in lines, and input-section origin fields.
syn match eldArchiveMember /[^ \t]\+\.a([^)]\+)/ contained

" Architecture tag appended to LOAD lines: [hexagonv68]
syn match eldArch /\[[a-zA-Z0-9_-]\+\]/ contained

" Origin file path on input-section lines (trailing field ending in .o/.a/…).
" Contains eldArchiveMember so archive paths are sub-highlighted.
syn match eldOriginFile /\s\+\S\+\.\(o\|a\|so\|elf\)\S*\s*$/ contained
      \ contains=eldArchiveMember

" Bare linker-script filename in the "Linker scripts used" section.
" Matches absolute paths (/…), relative paths (./…), and bare names.
syn match eldScriptFile /^\(\/\|\.\)[^ \t]*\.\(t\|ld\|lcs\|lds\|x\)\s*$/
syn match eldScriptFile /^[a-zA-Z][^ \t]*\.\(t\|ld\|lcs\|lds\|x\)\s*$/

" ════════════════════════════════════════════════════════════════════════════
" HIGHLIGHT LINKS
" ════════════════════════════════════════════════════════════════════════════
"
" 'hi def link' sets a default link that the user can override.
" Groups that have explicit colors in the "Keyword colors" section below do
" NOT use 'hi def link' — their color is set directly with 'hi'.

hi def link eldComment        Comment
hi def link eldMetaKey        Keyword
hi def link eldLsComment      Comment
hi def link eldBanner         Title
hi def link eldLoad           Statement
hi def link eldLoadPath       Normal
hi def link eldArchiveMember  String
hi def link eldArch           Special
hi def link eldPullIn         Normal
hi def link eldSymbolInParens Identifier
hi def link eldOutputSection  Type
hi def link eldSectionName    Type
hi def link eldInputSection   Normal
hi def link eldGCSection      Comment
hi def link eldGCTag          WarningMsg
hi def link eldGCSymbol       Comment
hi def link eldSymbolLine     Normal
hi def link eldSymbol         Identifier
hi def link eldPadding        Normal
hi def link eldPaddingKw      Keyword
hi def link eldSectionAttr    Comment
hi def link eldSHT            Constant
hi def link eldSHF            Constant
hi def link eldSectionMeta    Comment
hi def link eldMetaKey2       Keyword
hi def link eldHex            Number
hi def link eldOriginFile     Directory
hi def link eldScriptFile     String
hi def link eldRule           Normal
hi def link eldRuleKw         eldKwSectionList
hi def link eldRulePattern    Special
hi def link eldRuleGlob       Operator
hi def link eldRuleSectionName Type
hi def link eldRuleComment    Comment
hi def link eldLsRule         Normal
hi def link eldLsSortConstructors Normal
hi def link eldLsString       String
hi def link eldLsBrace        Delimiter
hi def link eldLsOutputSection Type
hi def link eldLsSecAttr      Normal
hi def link eldLsFill         Number
hi def link eldLsAssign       Normal
hi def link eldLsAssignLHS    Identifier
hi def link eldLsAssignOp     Operator
hi def link eldLsAssignDot    Special
hi def link eldLsAssignComment Comment
hi def link eldExprLine       Normal
hi def link eldExprLHS        Identifier
hi def link eldExprEval       Number
hi def link eldExprOp         Operator
hi def link eldExprParen      Delimiter
hi def link eldExprComment    Comment
hi def link eldExprDot        Special

" ════════════════════════════════════════════════════════════════════════════
" KEYWORD COLORS
" ════════════════════════════════════════════════════════════════════════════
"
" Explicit colors are used here (rather than 'hi def link') so that keywords
" stand out with distinct colors regardless of the active colorscheme.
"
" Color palette (One Dark inspired):
"   cyan    #56b6c2  — section-list / sort keywords, segment types
"   magenta #c678dd  — assignment commands (PROVIDE, HIDDEN, ASSERT)
"   yellow  #e5c07b  — expression functions (ALIGN, SIZEOF, …), data cmds
"   green   #98c379  — input/group control (INCLUDE, AS_NEEDED, …)
"   red     #e06c75  — destructive / control-flow (GROUP, /DISCARD/)
"   orange  #d19a66  — output-section type attributes (NOLOAD, AT, …)
"   blue    #61afef  — top-level script commands (SECTIONS, MEMORY, …)
"
" 'gui=' applies in gVim / Neovim with true-color; 'cterm=' applies in
" terminal Vim with 256-color support.

hi eldKwSectionList  guifg=#56b6c2 ctermfg=cyan     gui=bold cterm=bold
hi eldKwAssign       guifg=#c678dd ctermfg=magenta  gui=bold cterm=bold
hi eldKwExpr         guifg=#e5c07b ctermfg=yellow   gui=none cterm=none
hi eldKwInput        guifg=#98c379 ctermfg=green    gui=none cterm=none
hi eldKwGroup        guifg=#e06c75 ctermfg=red      gui=bold cterm=bold
hi eldKwDiscard      guifg=#e06c75 ctermfg=red      gui=bold cterm=bold
hi eldKwSecType      guifg=#d19a66 ctermfg=130      gui=none cterm=none
hi eldKwData         guifg=#e5c07b ctermfg=yellow   gui=none cterm=none
hi eldKwTopLevel     guifg=#61afef ctermfg=blue     gui=bold cterm=bold
hi eldKwSegType      guifg=#56b6c2 ctermfg=cyan     gui=none cterm=none

let b:current_syntax = "eld"

" ════════════════════════════════════════════════════════════════════════════
" FOLDING  (map files only)
" ════════════════════════════════════════════════════════════════════════════
"
" Two-level fold hierarchy driven by foldexpr:
"
"   foldlevel=0  →  only banner lines visible (one line per top-level section)
"   foldlevel=1  →  banner open; output-section fold headers visible
"   foldlevel=2  →  output section open; all content visible
"
" Fold-level assignments:
"   >1   banner line (s:IsBanner returns true)
"   >1   # CommandLine :  (single-line fold — next line opens a new fold)
"   >1   # LinkStats Begin
"   <1   # LinkStats End  (this line is the last line inside the fold)
"   >2   output-section header line (contains tab + '# Offset:')
"   <1   blank line immediately before the next banner (closes both levels)
"   <2   blank line immediately before the next output-section header
"   =    everything else (inherits the current fold level)
"
" s:IsBanner() is a script-local helper that returns true for any of the
" banner strings listed in the "MAP-FILE BANNERS" section above.
" 'Linker scripts used' is special-cased with \> because the actual line
" has trailing text "(including INCLUDE command)".

function! s:IsBanner(line)
  if a:line =~# '^Linker scripts used\>'
    return 1
  endif
  return a:line =~# '^\(Linker Script and memory map\|Archive member included because of file (symbol)\|Build Statistics\|Linker Plugin Information\|Linker Plugin Run Information\|# Output Section and Layout\s*\|Discarded input sections\|Memory Configuration\|Cross Reference Table\|Common symbol\s\+size\s\+file\)$'
endfunction

function! EldMapFoldExpr(lnum)
  let line = getline(a:lnum)

  " # CommandLine : … — single-line fold (the very long command-line string).
  " The next line (# LinkStats Begin) opens its own >1 fold, implicitly
  " closing this one.
  if line =~# '^# CommandLine\s*:'
    return '>1'
  endif

  " # LinkStats Begin … # LinkStats End — fold the build statistics block.
  if line =~# '^# LinkStats Begin$'
    return '>1'
  endif
  if line =~# '^# LinkStats End$'
    return '<1'
  endif

  " Banner lines open a level-1 fold that spans until the next banner.
  if s:IsBanner(line)
    return '>1'
  endif

  " Output-section header lines (tab + # Offset:) open a level-2 fold
  " nested inside the '# Output Section and Layout' banner fold.
  if line =~# '\t.*#\s*Offset:'
    return '>2'
  endif

  " Non-blank lines inherit the current fold level.
  if line =~# '\S'
    return '='
  endif

  " Blank line: look ahead to the next non-blank line to decide whether to
  " close a fold here.  Closing on the blank line (rather than on the next
  " header) keeps the blank line inside the current fold, which looks cleaner
  " when the fold is open.
  let next = a:lnum + 1
  while next <= line('$') && getline(next) =~# '^\s*$'
    let next += 1
  endwhile
  let nextline = getline(next)

  " Blank before a banner → close both the output-section fold (level 2)
  " and the banner fold (level 1) at once.
  if s:IsBanner(nextline)
    return '<1'
  endif

  " Blank before an output-section header → close only the section fold.
  if nextline =~# '\t.*#\s*Offset:'
    return '<2'
  endif

  return '='
endfunction

" Custom fold label shown when a fold is closed.
function! EldMapFoldText()
  let line  = getline(v:foldstart)
  let lines = v:foldend - v:foldstart

  if v:foldlevel == 1
    " Level-1 fold (banner or LinkStats): show the banner text + line count.
    return line . '  [' . lines . ' lines]'
  else
    " Level-2 fold (output section): show section name + Offset metadata.
    let secname = matchstr(line, '^\S\+')
    let meta    = matchstr(line, '#\s*Offset:.*$')
    return '  ' . secname . '  ' . meta . '  [' . lines . ' lines]'
  endif
endfunction

setlocal foldmethod=expr
setlocal foldexpr=EldMapFoldExpr(v:lnum)
setlocal foldtext=EldMapFoldText()
setlocal foldlevel=0
