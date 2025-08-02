//===- CreateScriptProgramHeaders.hpp -------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

// TODO : remove the common parts and make them as private functions.
bool GNULDBackend::createScriptProgramHdrs() {
  LinkerScript &script = m_Module.getScript();
  uint64_t vma = 0x0, pma = 0x0;
  bool hasError = false;
  bool disconnect_lma_vma = false;
  bool is_previous_start_of_segment = false;
  // The Load segment of the current section being iterated.
  ELFSegment *curLoadSegment = nullptr;
  ELFSection *last_seen_alloc_section = nullptr;
  std::vector<ELFSegment *> segmentsForSection;
  ELFSection *prev = nullptr;
  SectionMap::iterator out, outBegin, outEnd;
  GeneralOptions::AddressMapType::iterator addr,
      addrEnd = config().options().addressMap().end();
  bool hasInterp = false;
  uint32_t m_AtTableIndex = 0;
  // Dot symbol.
  LDSymbol *dotSymbol = m_Module.getNamePool().findSymbol(".");

  // Module AT Table.
  std::vector<ELFSection *> &atTable = m_Module.getAtTable();

  ELFSection *interp = script.sectionMap().find(".interp");
  if (interp && interp->size())
    hasInterp = true;

  addTargetSpecificSegments();

  auto assign_filehdr = [&] {
    // Assign fileheader / program header to the segment.
    for (auto &Phdr : m_Module.getScript().phdrList()) {
      if (Phdr->spec().hasFileHdr()) {
        _segmentsForSection[m_ehdr->getOutputSection()].push_back(
            _segments[Phdr->spec().name()]);
      }
    }
  };

  auto assign_phdr = [&] {
    for (auto &Phdr : m_Module.getScript().phdrList()) {
      if (Phdr->spec().hasPhdr()) {
        _segmentsForSection[m_phdr->getOutputSection()].push_back(
            _segments[Phdr->spec().name()]);
      }
    }
    const ELFSegment *PhdrLoadSegment =
        getLoadSegmentForOutputSection(m_phdr->getOutputSection());
    const ELFSegment *EhdrInLoadSegment =
        getLoadSegmentForOutputSection(m_ehdr->getOutputSection());
    if (EhdrInLoadSegment && !PhdrLoadSegment)
      _segmentsForSection[m_phdr->getOutputSection()].push_back(
          _segments[EhdrInLoadSegment->getSpec()->name()]);
  };

  auto reset_state = [&](OutputSectionEntry *OS = nullptr) {
    // Reset the state, and start from beginning.
    outBegin = script.sectionMap().begin();
    outEnd = script.sectionMap().end();
    out = outBegin;
    disconnect_lma_vma = false;
    last_seen_alloc_section = nullptr;
    is_previous_start_of_segment = false;
    if (OS) {
      for (auto &seg : _segmentsForSection[OS])
        seg->clear();
    }
    for (auto &seg : _segments)
      seg.getValue()->clear();
    if (m_phdr)
      m_phdr->setSize(elfSegmentTable().size() * getOnePhdrSize());
    noLoadSections.clear();
    resetNewSectionsAddedToLayout();
    vma = getImageBase(hasInterp, false);
    m_ImageStartVMA = vma;
    // Set initial dot symbol value.
    dotSymbol->setValue(vma);
    resetTrackedAssignments();
    restoreLSSymbolsUsingSnapshot();
    restorePartiallyEvalAssignsAndSymbolsUsingSnapshot();
  };

  reset_state();
  llvm::SaveAndRestore saved(TrackAssignments, true);

  while (out != outEnd) {
    bool useSetLMA = false;

    bool isStartOfSegment = false;

    bool curIsDebugSection = false;

    ELFSection *cur = (*out)->getSection();

    bool isCurAllocSection = cur->isAlloc();

    //
    // Add file header to layout, if it is not present.
    //
    if (isEhdrNeeded() && !isEhdrInLayout()) {
      addFileHeaderToLayout();
      assign_filehdr();
    }

    //
    // Add program header to layout, if it is not present.
    //
    if (isPhdrNeeded() && !isPhdrInLayout()) {
      addProgramHeaderToLayout();
      assign_phdr();
    }

    //
    // If there were new sections added, reset and resume
    //
    if (isNewSectionsAddedToLayout()) {
      reset_state();
      continue;
    }

    bool isNoLoad = ((*out)->prolog().type() == OutputSectDesc::NOLOAD);

    bool hasVMARegion = false;
    bool hasLMARegion = false;
    bool hasFixedLMA = false;

    if (isNoLoad)
      cur->setType(llvm::ELF::SHT_NOBITS);

    // Skip ehdr and phdr if the linker configuration does not
    // need file header and program header to be loaded
    if (!isEhdrNeeded() && (cur == m_ehdr)) {
      ++out;
      continue;
    }

    if (!isPhdrNeeded() && (cur == m_phdr)) {
      ++out;
      continue;
    }

    bool isRelocationSection = false;
    if (cur->isRelocationSection())
      isRelocationSection = true;

    if (!isCurAllocSection && isRelocationSection) {
      ++out;
      continue;
    }

    for (auto &seg : _segmentsForSection[*out]) {
      if (seg->empty() && seg->isLoadSegment()) {
        curLoadSegment = seg;
        isStartOfSegment = true;
      }
    }

    if ((!isCurAllocSection) && cur->size())
      curIsDebugSection = true;

    std::optional<uint64_t> scriptvma;
    bool doAlign = true;
    // If the output section specified a VMA value.
    if ((*out)->prolog().hasVMA()) {
      (*out)->prolog().vma().evaluateAndRaiseError();
      scriptvma = (*out)->prolog().vma().result();
      if (isCurAllocSection)
        dotSymbol->setValue(*scriptvma);
      if ((*out)->epilog().hasRegion() &&
          (*out)->epilog().region().containsVMA(scriptvma.value())) {
        hasVMARegion = true;
      }
      // If the output section descriptor has an alignment specified
      // honor the alignment specified, the alignment would have been
      // reflected in the section alignment.
      // The linker doesn't align the section if there was no alignment
      // specified for the output section but a "VMA" was specified.
      if (!(*out)->prolog().hasAlign())
        doAlign = false;
    }

    // If we find that a section is specified with an address
    addr = config().options().addressMap().find(cur->name());
    if (addr != addrEnd) {
      vma = addr->getValue();
      scriptvma = vma;
      if (isCurAllocSection)
        dotSymbol->setValue(vma);
    }

    // Check if the user specified MEMORY
    if ((*out)->epilog().hasRegion() && !scriptvma) {
      ScriptMemoryRegion &R = (*out)->epilog().region();
      vma = R.getVirtualAddr(*out);
      if (isCurAllocSection)
        dotSymbol->setValue(vma);
      hasVMARegion = true;
    }
    // If there is an AT Table. Lets try to check the dot value with the
    // section specified in the AT Table.
    if (atTable.size() && m_AtTableIndex < atTable.size()) {
      ELFSection *atSection = atTable[m_AtTableIndex];
      if (curIsDebugSection ||
          ((atSection->addr() < dotSymbol->value()) ||
           (scriptvma && (*scriptvma > atSection->addr())))) {
        // If the AT section falls within the bounds of the current address and
        // the size of the current section, try to place the AT section inside
        // the output section while traversing through the assignments.
        auto &segmentsForSection = _segmentsForSection[*out];
        out = script.sectionMap().insert(out, atSection);
        SectionMap::iterator outPrev = --out;
        SectionMap::iterator outCur = ++out;
        (*outCur)->setOrder((*outPrev)->order());
        if (out != script.sectionMap().begin()) {
          (*outCur)->moveSectionAssignments(*outPrev);
          _segmentsForSection[*outCur];
        }
        if (m_Module.getPrinter()->isVerbose())
          config().raise(Diag::verbose_inserting_section_at_fixed_addr)
              << atSection->name() << cur->addr()
              << atSection->getInputFile()->getInput()->decoratedPath()
              << (*out)->name();
        if (atSection->hasSectionData()) {
          for (auto &f : atSection->getFragmentList())
            f->getOwningSection()->setOutputSection(*outCur);
        }
        // Pick the PHDR's from the current section if the previous section is
        // a
        // nullptr section.
        if ((*outPrev)->getSection()->isNullKind())
          _segmentsForSection[*outCur] = segmentsForSection;
        else
          _segmentsForSection[*outCur] = _segmentsForSection[*outPrev];
        m_AtTableIndex++;
        reset_state(*outCur);
        continue;
      }
    }

    if (curIsDebugSection || (*out)->isDiscard()) {
      cur->setAddr(dotSymbol->value());
      evaluateAssignments(*out, m_AtTableIndex);
      evaluateAssignmentsAtEndOfOutputSection(*out);
      cur->setAddr(0);
      cur->setPaddr(0);
      ++out;
      continue;
    }

    if (cur->isFixedAddr()) {
      vma = cur->addr();
      if (isCurAllocSection)
        dotSymbol->setValue(vma);
    }

    // Get the value from the dot value.
    vma = dotSymbol->value();

    if (doAlign)
      alignAddress(vma, cur->getAddrAlign());

    // Check if the segment has a LMA address that is set.
    if (isStartOfSegment) {
      if (curLoadSegment->hasFixedLMA()) {
        hasFixedLMA = true;
        disconnect_lma_vma = true;
      }
    }

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

    // Adjust the offset with VMA.
    if (prev && (!useSetLMA || !hasLMARegion || !hasFixedLMA) &&
        disconnect_lma_vma) {
      // if pma was previously defined but not for this section then increment
      // this pma by the prev->pAddr + size and include the vmaoffset if this
      // not the start of a new segment.
      if (config().options().isCompact())
        pma = prev->pAddr() +
              ((prev->isBSS() || prev->isTBSS()) ? 0 : prev->size());
      else if (!is_previous_start_of_segment)
        pma = prev->pAddr() + (vma - prev->addr());
    } else {
      pma = vma;
    }

    if (cur->isFixedAddr() && (vma != cur->addr())) {
      config().raise(Diag::cannot_set_at_address) << cur->name();
      hasError = true;
    }

    cur->setAddr(vma);

    if (useSetLMA) {
      (*out)->prolog().lma().evaluateAndRaiseError();
      pma = (*out)->prolog().lma().result();
    } else if (hasVMARegion || hasLMARegion) {
      ScriptMemoryRegion &R = (*out)->epilog().lmaRegion();
      pma = R.getPhysicalAddr(*out);
      if (!(*out)->prolog().hasAlignWithInput() && !hasLMARegion)
        alignAddress(pma, cur->getAddrAlign());
    } else if (hasFixedLMA) {
      // If the current segment has a fixed LMA address, then
      curLoadSegment->fixedLMA()->evaluateAndRaiseError();
      pma = curLoadSegment->fixedLMA()->result();
    } else {
      alignAddress(pma, cur->getAddrAlign());
    }

    cur->setPaddr(pma);
    evaluateAssignments(*out, m_AtTableIndex);

    if (hasVMARegion)
      (*out)->epilog().region().addOutputSectionVMA(*out);
    // If LMA is set explicitly then there is no LMA region for this
    // section.
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
    cur->setWanted(cur->wantedInOutput() || cur->size());
    if (cur->isWanted()) {
      changeSymbolsFromAbsoluteToGlobal(*out);
      if (is_previous_start_of_segment)
        is_previous_start_of_segment = false;
    } else if (isStartOfSegment)
      is_previous_start_of_segment = true;

    if (cur->isAlloc())
      last_seen_alloc_section = cur;
    prev = cur;

    if (isNoLoad)
      noLoadSections.push_back(cur);

    // Add sections for the segment.
    for (auto &seg : _segmentsForSection[*out]) {
      // If there is a VMA assigned for the section,
      // lets just add the section.
      if (cur->isWanted() || cur->wantedInOutput() ||
          (scriptvma && scriptvma.value())) {
        seg->append(*out);
        seg->updateFlagPhdr(getSegmentFlag(cur->getFlags()));
      }
      if (seg->isLoadSegment() && !config().options().isOMagic())
        seg->setAlign(abiPageSize());
      else
        seg->setAlign(seg->getMaxSectionAlign());
    }

    // Evaluate Assignments at the end of the output section.
    evaluateAssignmentsAtEndOfOutputSection(*out);
    cur->setWanted(cur->wantedInOutput() || cur->size());
    if (!config().getDiagEngine()->diagnose()) {
      return false;
    }

    ++out;
  }

  for (auto &seg : elfSegmentTable()) {
    // Handle empty segments
    if (!seg->size()) {
      if (seg->isLoadSegment() && !config().options().isOMagic())
        seg->setAlign(abiPageSize());
      else
        seg->setAlign(config().targets().is32Bits() ? 4 : 8);
    }
  }

  evaluateTargetSymbolsBeforeRelaxation();

  m_Module.getAtTable().clear();

  return hasError;
}
