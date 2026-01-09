//===- CreateProgramHeaders.hpp -------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

/// createProgramHdrs - base on output sections to create the program headers
bool GNULDBackend::createProgramHdrs() {
  LinkerScript &script = m_Module.getScript();
  uint64_t vma = 0x0, pma = 0x0;
  bool is64bit = config().targets().is64Bits();
  ELFSection *prev = nullptr;
  bool disconnect_lma_vma = false;
  bool last_section_needs_new_segment = false;
  bool load_ehdr = false;

  uint32_t cur_flag, prev_flag = 0x0;
  std::string cur_mem_region, prev_mem_region;
  ELFSegment *load_seg = nullptr;
  ELFSegment *note_seg = nullptr;
  OutputSectionEntry *prevOut = nullptr;
  SectionMap::iterator out, outBegin, outEnd;
  outBegin = script.sectionMap().begin();
  outEnd = script.sectionMap().end();
  bool hasError = false;
  bool linkerScriptHasSectionsCommand =
      m_Module.getScript().linkerScriptHasSectionsCommand();
  uint64_t segAlign = abiPageSize();
  m_NumReservedSegments = 0;

  bool linkerScriptHasMemoryCommand = m_Module.getScript().hasMemoryCommand();

  if (linkerScriptHasMemoryCommand)
    clearMemoryRegions();

  // Module AT Table.
  std::vector<ELFSection *> &atTable = m_Module.getAtTable();

  GeneralOptions::AddressMapType::iterator addr,
      addrEnd = config().options().addressMap().end();

  // Support for PT_GNU_RELRO.
  llvm::DenseMap<OutputSectionEntry *, ELFSegment *> _relRoSegmentSections;

  // Setup alignment and check for layout consistency if TLS is used
  bool seenTLS = setupTLS();
  if (!config().getDiagEngine()->diagnose())
    return false;

  // If PHDRS are specified and the segment tables are empty
  // lets create the program headers as defined by the script
  if (m_Module.getScript().phdrsSpecified()) {
    if (elfSegmentTable().empty())
      hasError = createSegmentsFromLinkerScript();
    if (hasError)
      return hasError;
    hasError = createScriptProgramHdrs();
    return hasError;
  }

  elfSegmentTable().clear();
  _segmentsForSection.clear();
  _noteSegmentsForSection.clear();

  ELFSegment *phdrSeg = nullptr;

  bool wantPhdr = false;
  bool hasInterp = false;

  ELFSection *interp = script.sectionMap().find(".interp");
  if (interp && interp->size()) {
    hasInterp = true;
    wantPhdr = true;
    phdrSeg = make<ELFSegment>(llvm::ELF::PT_PHDR);
    elfSegmentTable().addSegment(phdrSeg);
    // If program header was in layout, add it to the PHDR segment
    // if it is present
    if (isPhdrInLayout()) {
      phdrSeg->append(m_phdr->getOutputSection());
      phdrSeg->updateFlag(getSegmentFlag(m_phdr->getFlags()));
      phdrSeg->setAlign(m_phdr->getAddrAlign());
    }
    ++m_NumReservedSegments;
  }

  ELFSection *dynamic = script.sectionMap().find(".dynamic");
  ELFSection *eh_frame_hdr = script.sectionMap().find(".eh_frame_hdr");

  ELFSegment *pt_gnu_relro = nullptr;
  wantPhdr = seenTLS;
  // If there is PT_DYNAMIC, or PT_TLS or PT_GNU_EH_FRAME load the program
  // header, as the loader goes through the segments to do the corresponding
  // work.
  if ((dynamic && dynamic->size()) || (eh_frame_hdr && eh_frame_hdr->size()))
    wantPhdr = true;

  // check if we need save a space for elf header + phdr
  wantPhdr =
      m_pInfo->needEhdr(m_Module, linkerScriptHasSectionsCommand, wantPhdr);

  if (!linkerScriptHasSectionsCommand &&
      LinkerConfig::DynObj == config().codeGenType())
    wantPhdr = true;

  if (!linkerScriptHasSectionsCommand && wantPhdr)
    load_ehdr = true;

  if (dynamic && dynamic->size())
    ++m_NumReservedSegments;

  // make PT_GNU_EH_FRAME
  if (eh_frame_hdr && eh_frame_hdr->size())
    ++m_NumReservedSegments;

  if (seenTLS)
    ++m_NumReservedSegments;

  // make PT_GNU_STACK
  ELFSection *noteGNUStack = nullptr;
  bool needStackSegment = false;
  uint32_t gnuStackFlag = 0x0;
  if (config().options().hasStackSet()) {
    needStackSegment = true;
    if (config().options().hasExecStack())
      gnuStackFlag = llvm::ELF::PF_X;
  } else if (m_pInfo->processNoteGNUSTACK()) {
    noteGNUStack = script.sectionMap().find(".note.GNU-stack");
    if (noteGNUStack) {
      // Linker need discard this section
      noteGNUStack->setOffset(0);
      needStackSegment = true;
      if (0 != (llvm::ELF::SHF_EXECINSTR & noteGNUStack->getFlags()))
        gnuStackFlag = llvm::ELF::PF_X;
    }
  }
  if (needStackSegment)
    ++m_NumReservedSegments;

  // This flag is used to control if we want to add a section to the RELRO
  // segment. If we found that a non-RELRO section follows a RELRO section
  // we want to immediately disable adding new sections to the RELRO segment, as
  // the RELRO segment needs to be contiguous.
  bool enable_RELRO = true;

  uint64_t startvma = 0;

  LDSymbol *dotSymbol = m_Module.getNamePool().findSymbol(".");

  auto reset_state = [&]() {
    outBegin = script.sectionMap().begin();
    outEnd = script.sectionMap().end();
    out = outBegin;
    prev_flag = 0;
    prev = nullptr;
    prevOut = nullptr;
    prev_mem_region = "";
    if (linkerScriptHasMemoryCommand)
      clearMemoryRegions();
    if (m_phdr)
      m_phdr->setSize(elfSegmentTable().size() * getOnePhdrSize() +
                      (numReservedSegments() * getOnePhdrSize()));
    // Reset VMA.
    startvma = getImageBase(hasInterp, load_ehdr);
    m_ImageStartVMA = startvma;
    dotSymbol->setValue(startvma);
    disconnect_lma_vma = false;
    noLoadSections.clear();
    resetNewSectionsAddedToLayout();
    enable_RELRO = true;

    {
      eld::RegisterTimer T("Evaluate Script Assignments", "Establish Layout",
                           m_Module.getConfig().options().printTimingStats());
      evaluateScriptAssignments(/*afterLayout=*/false);
    }
    changedOutputSection = nullptr;
  };

  if (load_ehdr)
    setNeedEhdr();

  if (wantPhdr)
    setNeedPhdr();

  int preRelaxPassIter = 0;
  do {
    reset_state();
    while (out != outEnd) {
      bool createPT_LOAD = false;
      bool createNOTE_segment = false;
      // Handle AT
      bool useSetLMA = false;
      // Handle MEMORY with VMA region
      bool hasVMARegion = false;
      // Handle explicit LMA specified with
      // LMA region
      bool hasLMARegion = false;

      bool curIsDebugSection = false;

      ELFSection *cur = (*out)->getSection();

      bool isCurAlloc = cur->isAlloc();

      //
      // Add file header to layout, if it is not present.
      //
      if (!isEhdrInLayout())
        addFileHeaderToLayout();

      //
      // Add program header to layout, if it is not present.
      //
      if (!isPhdrInLayout())
        addProgramHeaderToLayout();

      //
      // If there were new sections added, reset and resume
      //
      if (isNewSectionsAddedToLayout()) {
        reset_state();
        continue;
      }

      bool isPrevTBSS = prev && prev->isTBSS();

      bool isPrevRelRO =
          config().options().hasRelro() && prev && isRelROSection(prev);

      bool isCurRelRO =
          config().options().hasRelro() && cur && isRelROSection(cur);

      bool isNoLoad = ((*out)->prolog().type() == OutputSectDesc::NOLOAD);

      if (isNoLoad)
        cur->setType(llvm::ELF::SHT_NOBITS);

      // Skip ehdr and phdr if the linker configuration does not
      // need file header and program header to be loaded
      if (!isEhdrNeeded() && (cur == m_ehdr)) {
        ++out;
        continue;
      }

      cur->setWanted(cur->wantedInOutput() || cur->size());

      if (!isPhdrNeeded() && (cur == m_phdr)) {
        ++out;
        continue;
      }

      if ((!isCurAlloc) && cur->isWanted())
        curIsDebugSection = true;

      // Linker script overriding below.
      std::optional<uint64_t> scriptvma;
      bool doAlign = true;
      // If the output section specified a VMA value.
      if ((*out)->prolog().hasVMA()) {
        (*out)->prolog().vma().evaluateAndRaiseError();
        // If the output section descriptor has an alignment specified
        // honor the alignment specified, the alignment would have been
        // reflected in the section alignment.
        // The linker doesn't align the section if there was no alignment
        // specified for the output section but a "VMA" was specified.
        if (!(*out)->prolog().hasAlign())
          doAlign = false;
        scriptvma = (*out)->prolog().vma().result();
        if (isCurAlloc)
          dotSymbol->setValue(*scriptvma);
        if ((*out)->epilog().hasRegion() &&
            (*out)->epilog().region().containsVMA(scriptvma.value())) {
          hasVMARegion = true;
        }
      }

      // If we find that a section is specified with an address
      addr = config().options().addressMap().find(cur->name());
      if (addr != addrEnd) {
        vma = addr->getValue();
        if (isCurAlloc)
          createPT_LOAD = true;
        if (isCurAlloc)
          dotSymbol->setValue(vma);
      }

      // Check if the user specified MEMORY
      if ((*out)->epilog().hasRegion() && !scriptvma) {
        hasVMARegion = true;
        ScriptMemoryRegion &R = (*out)->epilog().region();
        vma = R.getVirtualAddr(*out);
        if (isCurAlloc)
          dotSymbol->setValue(vma);
      }

      // If there is an AT Table. Lets try to check the dot value with the
      // section specified in the AT Table.
      if (atTable.size() && m_AtTableIndex < atTable.size()) {
        ELFSection *atSection = atTable[m_AtTableIndex];
        if (curIsDebugSection ||
            ((atSection->addr() < dotSymbol->value()) ||
             (scriptvma && (*scriptvma > atSection->addr())))) {
          out = script.sectionMap().insert(out, atSection);
          SectionMap::iterator outPrev = --out;
          SectionMap::iterator outCur = ++out;
          (*outCur)->setOrder((*outPrev)->order());
          if (m_Module.getPrinter()->isVerbose())
            config().raise(Diag::verbose_inserting_section_at_fixed_addr)
                << atSection->name() << cur->addr()
                << atSection->getInputFile()->getInput()->decoratedPath()
                << (*out)->name();
          if (out != script.sectionMap().begin())
            (*outCur)->moveSectionAssignments(*outPrev);
          if (atSection->hasSectionData()) {
            for (auto &f : atSection->getFragmentList())
              f->getOwningSection()->setOutputSection(*outCur);
          }
          m_AtTableIndex++;
          reset_state();
          continue;
        }
      }

      if (curIsDebugSection || (*out)->isDiscard()) {
        setVMA(*cur, dotSymbol->value(), /*ignoreChangedSection=*/true);
        evaluateAssignments(*out, m_AtTableIndex);
        evaluateAssignmentsAtEndOfOutputSection(*out);
        cur->setWanted(cur->wantedInOutput() || cur->size());
        ++out;
        setVMA(*cur, 0, /*ignoreChangedSection=*/true);
        setLMA(*cur, 0);
        continue;
      }

      // Whatever the linker wants to set would go here.
      if (cur->isFixedAddr()) {
        vma = cur->addr();
        if (isCurAlloc)
          dotSymbol->setValue(vma);
      }

      // Get the value from the dot value.
      vma = dotSymbol->value();
      if (doAlign)
        alignAddress(vma, cur->getAddrAlign());

      // check if the physical address is being set, otherwise if
      // if this is the first section and the VMA was forced then
      // set PMA = VMA. For all sections following the first section
      // PMA will be calculated separately below
      if ((*out)->prolog().hasLMA()) {
        useSetLMA = true;
        disconnect_lma_vma = true;
      }

      // Check if the user specified MEMORY for LMA
      if ((*out)->epilog().hasLMARegion()) {
        hasLMARegion = true;
        disconnect_lma_vma = true;
      }

      cur_flag = getSegmentFlag(cur->getFlags());

      if (linkerScriptHasMemoryCommand && (*out)->epilog().hasRegion())
        cur_mem_region = (*out)
                             ->epilog()
                             .region()
                             .getMemoryDesc()
                             ->getMemorySpec()
                             ->getMemoryDescriptor();

      // If the user specifies the linker to create a separate rosegment, do
      // that.
      if (!config().options().rosegment() || config().options().isOMagic())
        cur_flag = (cur_flag & ~llvm::ELF::PF_X);

      if (config().options().isOMagic())
        cur_flag = (cur_flag & ~llvm::ELF::PF_W);

      // getSegmentFlag returns 0 if the section is not allocatable.
      if ((cur_flag != prev_flag) && (isCurAlloc))
        createPT_LOAD = true;

      if (linkerScriptHasMemoryCommand && (cur_mem_region != prev_mem_region))
        createPT_LOAD = true;

      if (linkerScriptHasMemoryCommand && (cur_mem_region != prev_mem_region))
        createPT_LOAD = true;

      // If the current section is alloc section and if the previous section is
      // NOBITS and current is PROGBITS, we need to create a new segment.
      if (isCurAlloc && cur->isWanted() && !isPrevTBSS) {
        if ((cur_flag == prev_flag) && handleBSS(prev, cur))
          createPT_LOAD = true;
      }

      // Calculate VMA offset
      int64_t vmaoffset = 0;
      // Adjust the offset with VMA.
      if (prev && !isPrevTBSS) {
        // Calculate VMA only if section is allocatable
        if (!createPT_LOAD && isCurAlloc) {
          // it's possible that the location counter was set with an address
          // which is less than the previous output section address + size, in
          // which case the vmaoffset should 0x0
          if (vma >= (prev->addr() + prev->size()))
            vmaoffset = vma - (prev->addr() + prev->size());
          else
            // Without program headers, we always create a new segment if the
            // previous address is larger than the current address. Calculating
            // the offsets is taken care automatically. The gnu linker may
            // choose to place the sections in decreasing order of addresses and
            // later decide to sort it as is done in CreateScriptProgramheaders.
            // TODO: raise an error to see LMA needs to be assigned.
            createPT_LOAD = true;

          // If Program headers are not specified and the vma difference is big
          // lets create a PT_LOAD to adjust the offset.
          if (std::abs(vmaoffset) > (int64_t)segAlign)
            createPT_LOAD = true;
        }
      }
      // create a PT_LOAD if the pma and vma are decoupled
      bool createNewSegmentDueToLMADifference = false;
      if ((useSetLMA || hasLMARegion) && (pma != vma) && (isCurAlloc) &&
          !createPT_LOAD) {
        // This will allow us to enter the condition that a PT_LOAD would need
        // to be created, since pma is not equal vma.
        createPT_LOAD = true;
        // Lets set a boolean flag to indicate that a new segment is being
        // requested because of VMA and PMA are not equal. Linker is going to
        // later decide, if this new segment was really needed.
        createNewSegmentDueToLMADifference = true;
      }

      // If we dont have a load segment created, create a PT_LOAD.
      if (!load_seg)
        createPT_LOAD = true;

      bool sectionHasLoadSeg = false;
      bool sectionHasNoteSeg = false;
      bool newSegmentCreated = false;

      if (_segmentsForSection.find(*out) != _segmentsForSection.end()) {
        load_seg = _segmentsForSection[*out].front();
        sectionHasLoadSeg = true;
      }

      if (isCurAlloc && (createPT_LOAD || last_section_needs_new_segment)) {
        if (config().options().alignSegmentsToPage())
          alignAddress(vma, segAlign);
        if (cur->isFixedAddr() && (vma != cur->addr())) {
          config().raise(Diag::cannot_set_at_address) << cur->name();
          hasError = true;
        }
        setVMA(*cur, vma, /*ignoreChangedSection=*/false);
        // Handle setting LMA and alignment of LMA
        // Explicitly align LMA to make the code easy to read
        if (useSetLMA) {
          (*out)->prolog().lma().evaluateAndRaiseError();
          pma = (*out)->prolog().lma().result();
        } else if (hasVMARegion || hasLMARegion) {
          ScriptMemoryRegion &R = (*out)->epilog().lmaRegion();
          pma = R.getPhysicalAddr(*out);
          // If LMA region has been explicitly specified
          // do not align LMA. A LMA region exists for the VMA
          // region just for making it easy for us to handle assigning
          // physical addresses
          if (!(*out)->prolog().hasAlignWithInput() && !hasLMARegion)
            alignAddress(pma, cur->getAddrAlign());
        } else if (!prev || !disconnect_lma_vma) {
          pma = vma;
          alignAddress(pma, cur->getAddrAlign());
        } else if ((last_section_needs_new_segment) && (!disconnect_lma_vma)) {
          // If the LMA and VMA are disconnected, the physical address should
          // always be taken as the difference of the current virtual address
          // minus the previous virtual address.
          pma = prev->pAddr() + prev->size();
          alignAddress(pma, cur->getAddrAlign());
        } else {
          vmaoffset = vma - (prev->addr() + prev->size());
          pma = prev->pAddr() + prev->size() + vmaoffset;
          alignAddress(pma, cur->getAddrAlign());
        }

        // FIXME : remove this option alignSegmentsToPage
        // Handle the case without any linker scripts,
        if (config().options().alignSegmentsToPage())
          alignAddress(pma, segAlign);

        setLMA(*cur, pma);
        if (createNewSegmentDueToLMADifference) {
          if (pma == vma)
            createPT_LOAD = false;
          else {
            int64_t pmaoffset = 0;
            if (pma >= (prev->pAddr() + prev->size())) {
              pmaoffset = pma - (prev->pAddr() + prev->size());
              if (vma >= (prev->addr() + prev->size()))
                vmaoffset = vma - (prev->addr() + prev->size());
              if (pmaoffset == vmaoffset)
                createPT_LOAD = false;
            }
          }
        }
        evaluateAssignments(*out, m_AtTableIndex);
        cur->setWanted(cur->wantedInOutput() || cur->size());
        if (hasVMARegion)
          (*out)->epilog().region().addOutputSectionVMA(*out);
        if (!useSetLMA && (hasVMARegion || hasLMARegion))
          (*out)->epilog().lmaRegion().addOutputSectionLMA(*out);
        if (!config().getDiagEngine()->diagnose()) {
          return false;
        }

        if (m_AtTableIndex < atTable.size() &&
            (atTable[m_AtTableIndex]->addr() < (cur->addr() + cur->size()))) {
          config().raise(Diag::cannot_place_at_section)
              << atTable[m_AtTableIndex]->name() << cur->name();
          ++m_AtTableIndex;
          hasError = true;
        }
        // The flag last_section_needs_new_segment controls that if there was a
        // need to create a PT_LOAD segment but we determined that the size of
        // the section is 0, we set the flag and move on.
        if (!cur->isWanted())
          last_section_needs_new_segment = true;
        else if (!sectionHasLoadSeg &&
                 (createPT_LOAD || (cur->addr() < prev->addr()))) {
          load_seg = make<ELFSegment>(llvm::ELF::PT_LOAD,
                                      getSegmentFlag(cur->getFlags()));
          elfSegmentTable().addSegment(load_seg);
          last_section_needs_new_segment = false;
          newSegmentCreated = true;
        } else
          // If there was no segment created and the section is in the same load
          // segment.
          last_section_needs_new_segment = false;
      } else {
        // If the previous section is a RELRO section and the current section is
        // not a RELRO section, move the non RELRO section to a new page, as the
        // dynamic linker will mprotect the page after dynamic relocation.
        // If this doesn't move to the next page, then any writes to the section
        // will incur a pagefault and crash!. If there is a linker script dont
        // do any of this, as the user may want to configure sections to be in
        // the same page. This is a difference between ELD and GNU linker. This
        // is done only if the previous section and the new section fall in the
        // same segment.
        if (enable_RELRO && isPrevRelRO && !isCurRelRO &&
            !linkerScriptHasSectionsCommand)
          vma += abiPageSize();
        if (cur->isFixedAddr() && (vma != cur->addr())) {
          config().raise(Diag::cannot_set_at_address) << cur->name();
          hasError = true;
        }
        if (doAlign)
          alignAddress(vma, cur->getAddrAlign());
        setVMA(*cur, vma, /*ignoreChangedSection=*/false);
        // FIXME : de-duplicate this case.
        if (useSetLMA) {
          (*out)->prolog().lma().evaluateAndRaiseError();
          pma = (*out)->prolog().lma().result();
        } else if (hasVMARegion || hasLMARegion) {
          ScriptMemoryRegion &R = (*out)->epilog().lmaRegion();
          pma = R.getPhysicalAddr(*out);
          if (!(*out)->prolog().hasAlignWithInput() && !hasLMARegion)
            alignAddress(pma, cur->getAddrAlign());
        } else if (!prev || !disconnect_lma_vma) {
          pma = vma;
          alignAddress(pma, cur->getAddrAlign());
        } else {
          vmaoffset = vma - (prev->addr() + prev->size());
          pma = prev->pAddr() + prev->size() + vmaoffset;
          alignAddress(pma, cur->getAddrAlign());
        }
        setLMA(*cur, pma);
        evaluateAssignments(*out, m_AtTableIndex);
        if (hasVMARegion)
          (*out)->epilog().region().addOutputSectionVMA(*out);
        if (!useSetLMA && (hasVMARegion || hasLMARegion))
          (*out)->epilog().lmaRegion().addOutputSectionLMA(*out);
        if (!config().getDiagEngine()->diagnose()) {
          return false;
        }
        if (m_AtTableIndex < atTable.size() &&
            (atTable[m_AtTableIndex]->addr() < (cur->addr() + cur->size()))) {
          config().raise(Diag::cannot_place_at_section)
              << atTable[m_AtTableIndex]->name() << cur->name();
          ++m_AtTableIndex;
          hasError = true;
        }
      }

      // Evaluate Assignments at end of output section.
      evaluateAssignmentsAtEndOfOutputSection(*out);
      cur->setWanted(cur->wantedInOutput() || cur->size());

      if (!config().getDiagEngine()->diagnose()) {
        return false;
      }
      // Append the section to the segment.
      if (load_seg && isCurAlloc) {
        if (!last_section_needs_new_segment &&
            (createPT_LOAD || cur->isWanted())) {
          if (isPrevRelRO && !isCurRelRO)
            enable_RELRO = false;
          // Check if the current section is a NOTE section and create
          // appropriate NOTE segments.
          if (cur->getType() == llvm::ELF::SHT_NOTE) {
            note_seg = nullptr;
            if (_noteSegmentsForSection.find(*out) !=
                _noteSegmentsForSection.end()) {
              note_seg = _noteSegmentsForSection[*out];
              sectionHasNoteSeg = true;
            }
            // If the current section has no NOTE segment, check if the previous
            // section had a note segment and check if we can reuse it.
            if (!sectionHasNoteSeg) {
              if (prev) {
                if (prev->getType() == llvm::ELF::SHT_NOTE &&
                    (cur->getFlags() == prev->getFlags()))
                  note_seg = _noteSegmentsForSection[prevOut];
              }
              // If there was no note segment that we can reuse, lets create
              // one.
              if (!note_seg)
                createNOTE_segment = true;
            }
            // If for some reason, we create a new load segment, create a
            // separate segment for NOTE too.
            if (newSegmentCreated)
              createNOTE_segment = true;
            if (createNOTE_segment) {
              note_seg = make<ELFSegment>(llvm::ELF::PT_NOTE,
                                          getSegmentFlag(cur->getFlags()));
              elfSegmentTable().addSegment(note_seg);
              newSegmentCreated = true;
            }
            // Append the section to the NOTE segment.
            if (!sectionHasNoteSeg) {
              note_seg->append(*out);
              note_seg->updateFlag(getSegmentFlag(cur->getFlags()));
              _noteSegmentsForSection[*out] = note_seg;
              note_seg->setAlign(note_seg->getMaxSectionAlign());
            }
          }
          // Handle GNU RELRO sections.
          if (isRelROSection(cur) && enable_RELRO) {
            bool sectionHasRelROSegment = true;
            bool needRelRoSegment = false;
            if (_relRoSegmentSections.find(*out) == _relRoSegmentSections.end())
              sectionHasRelROSegment = false;
            if (!sectionHasRelROSegment && !pt_gnu_relro)
              needRelRoSegment = true;
            if (needRelRoSegment) {
              pt_gnu_relro = make<ELFSegment>(llvm::ELF::PT_GNU_RELRO,
                                              getSegmentFlag(cur->getFlags()));
              elfSegmentTable().addSegment(pt_gnu_relro);
              newSegmentCreated = true;
            }
            // Append the section to the RELRO segment, if
            // this is the first section for the RELRO segment (or)
            // the previous section also was a RELRO section.
            if (!sectionHasRelROSegment &&
                (!pt_gnu_relro->size() || isRelROSection(prev))) {
              pt_gnu_relro->append(*out);
              pt_gnu_relro->updateFlag(getSegmentFlag(cur->getFlags()));
              _relRoSegmentSections[*out] = pt_gnu_relro;
              pt_gnu_relro->setAlign(pt_gnu_relro->getMaxSectionAlign());
            }
          }
          prev = cur;
          if (isNoLoad)
            noLoadSections.push_back(cur);
          prevOut = (*out);
          prev_flag = cur_flag;
          if (!cur_mem_region.empty())
            prev_mem_region = cur_mem_region;
          load_seg->updateFlag(getSegmentFlag(cur->getFlags()));
          changeSymbolsFromAbsoluteToGlobal(*out);
          last_section_needs_new_segment = false;
          if (!sectionHasLoadSeg) {
            _segmentsForSection[*out].push_back(load_seg);
            load_seg->append(*out);
            // Set the PT_LOAD alignment.
            load_seg->setAlign(config().options().isOMagic()
                                   ? load_seg->getMaxSectionAlign()
                                   : segAlign);
          }
          // Continue from beginning since we added a segment.
          if (newSegmentCreated) {
            reset_state();
            continue;
          }
        }
      }
      ++out;
    }
  } while (++preRelaxPassIter < maxPreRelaxationPasses && changedOutputSection);

  // If layout did not converge within the limit, emit a warning for the
  // changed section.
  if (changedOutputSection) {
    config().raise(Diag::note_section_address_not_converging)
        << changedOutputSection->name() << maxPreRelaxationPasses;
  }

  if (dynamic && dynamic->size()) {
    ELFSegment *dyn_seg = make<ELFSegment>(llvm::ELF::PT_DYNAMIC,
                                           llvm::ELF::PF_R | llvm::ELF::PF_W);
    elfSegmentTable().addSegment(dyn_seg);
    dyn_seg->append(dynamic->getOutputSection());
    if (!dyn_seg->segAlign())
      dyn_seg->setAlign(is64bit ? 8 : 4);
  }

  if (interp && interp->size()) {
    ELFSegment *interpSegment = make<ELFSegment>(llvm::ELF::PT_INTERP);
    elfSegmentTable().addSegment(interpSegment);
    if (!interpSegment->segAlign())
      interpSegment->setAlign(1);
    interpSegment->append(interp->getOutputSection());
  }

  // make PT_GNU_EH_FRAME
  if (eh_frame_hdr && eh_frame_hdr->size()) {
    ELFSegment *eh_seg = make<ELFSegment>(llvm::ELF::PT_GNU_EH_FRAME);
    elfSegmentTable().addSegment(eh_seg);
    eh_seg->append(eh_frame_hdr->getOutputSection());
  }

  // make PT_GNU_STACK
  if (needStackSegment) {
    ELFSegment *gnu_stack_seg =
        make<ELFSegment>(llvm::ELF::PT_GNU_STACK,
                         llvm::ELF::PF_R | llvm::ELF::PF_W | gnuStackFlag);
    if (noteGNUStack) {
      gnu_stack_seg->append(noteGNUStack->getOutputSection());
      gnu_stack_seg->setAlign(noteGNUStack->getAddrAlign());
    }
    elfSegmentTable().addSegment(gnu_stack_seg);
  }

  // Create PT_TLS segment
  if (seenTLS) {
    ELFSegment *pt_tls_segment = make<ELFSegment>(llvm::ELF::PT_TLS);
    for (auto &section : script.sectionMap()) {
      ELFSection *S = section->getSection();
      if (S->isTLS() && S->isWanted())
        pt_tls_segment->append(section);
    }
    pt_tls_segment->setAlign(pt_tls_segment->getMaxSectionAlign());
    elfSegmentTable().addSegment(pt_tls_segment);
  }

  doCreateProgramHdrs();

  evaluateTargetSymbolsBeforeRelaxation();

  elfSegmentTable().sortSegments();

  m_Module.getAtTable().clear();

  return hasError;
}
