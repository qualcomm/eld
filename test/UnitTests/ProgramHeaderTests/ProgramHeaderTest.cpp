//===- ProgramHeaderTest.cpp----------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Linker.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include "eld/Input/ZOption.h"
#include "eld/Object/ScriptMemoryRegion.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Script/Expression.h"
#include "eld/Script/MemoryCmd.h"
#include "eld/Script/StrToken.h"
#include "eld/Support/Memory.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/TargetInfo.h"
#include "gtest/gtest.h"

using namespace eld;

namespace {

class FakeTargetInfo final : public TargetInfo {
public:
  explicit FakeTargetInfo(LinkerConfig &Config) : TargetInfo(Config) {}

  uint32_t machine() const override { return llvm::ELF::EM_NONE; }
  std::string getMachineStr() const override { return "fake"; }
  uint64_t flags() const override { return 0; }

  uint64_t startAddr(bool, bool, bool) const override { return 0x10000; }

  bool needEhdr(Module &, bool, bool IsPhdr) override { return IsPhdr; }
};

class FakeBackend final : public GNULDBackend {
public:
  FakeBackend(Module &M, TargetInfo *Info) : GNULDBackend(M, Info) {
    m_DynamicSectionHeadersInputFile =
        llvm::dyn_cast<ELFObjectFile>(m_Module.createInternalInputFile(
            make<Input>("Dynamic section headers", config().getDiagEngine()),
            /*CreateElfObjectFile=*/true));
  }

  bool finalizeTargetSymbols() override { return true; }
  Relocator *getRelocator() const override { return nullptr; }
  Stub *getBranchIslandStub(Relocation *, int64_t) const override {
    return nullptr;
  }
  bool initRelocator() override { return true; }
  void initTargetSections(ObjectBuilder &) override {}
  void initTargetSymbols() override {}
  size_t getRelEntrySize() override { return 0; }
  size_t getRelaEntrySize() override { return 0; }
  ELFDynamic *dynamic() override { return nullptr; }
  std::size_t PLTEntriesCount() const override { return 0; }
  std::size_t GOTEntriesCount() const override { return 0; }
};

struct Harness {
  DiagnosticEngine *Diag = nullptr;
  LinkerConfig *Config = nullptr;
  LinkerScript *Script = nullptr;
  Module *Mod = nullptr;
  Linker *L = nullptr;
  FakeTargetInfo *Info = nullptr;
  FakeBackend *Backend = nullptr;
};

static Harness createHarness() {
  Harness H;
  H.Diag = make<DiagnosticEngine>(/*useColor=*/false);
  H.Config = make<LinkerConfig>(H.Diag);
  H.Config->targets().setTriple("x86_64-unknown-linux-gnu");
  H.Config->targets().setBitClass(64);
  H.Config->targets().setEndian(TargetOptions::Little);
  H.Config->setCodeGenType(LinkerConfig::Exec);
  H.Script = make<LinkerScript>(H.Diag);
  H.Mod = make<Module>(*H.Script, *H.Config, /*layoutInfo=*/nullptr);
  H.L = make<Linker>(*H.Mod, *H.Config);
  EXPECT_TRUE(H.Mod->createInternalInputs());
  H.Info = make<FakeTargetInfo>(*H.Config);
  H.Backend = make<FakeBackend>(*H.Mod, H.Info);
  return H;
}

static ELFSection *addOutputSection(Module &M, llvm::StringRef Name,
                                    uint32_t Type, uint32_t Flags,
                                    uint64_t Size, uint64_t Align) {
  SectionMap &Map = M.getScript().sectionMap();
  ELFSection *Sec =
      Map.createELFSection(Name.str(), LDFileFormat::Regular, Type, Flags, 0);
  Sec->setAddrAlign(Align);
  Sec->setSize(Size);
  auto It = Map.insert(Map.end(), Sec);
  Sec->setOutputSection(*It);
  return Sec;
}

static size_t countSegments(const GNULDBackend &B, uint32_t Type) {
  size_t Count = 0;
  for (auto *Seg : B.elfSegmentTable().segments())
    if (Seg->type() == Type)
      ++Count;
  return Count;
}

static bool segmentContains(const ELFSegment &Seg, llvm::StringRef Name) {
  for (auto *OSE : Seg.sections())
    if (OSE && OSE->getSection() && OSE->getSection()->name() == Name)
      return true;
  return false;
}

static uint64_t alignTo(uint64_t Value, uint64_t Align) {
  if (!Align)
    return Value;
  return ((Value + Align - 1) / Align) * Align;
}

static void setOutputSectionLMA(Module &M, ELFSection &Sec, uint64_t LMA) {
  auto *OS = Sec.getOutputSection();
  ASSERT_NE(OS, nullptr);
  auto *Expr = make<Integer>(M, Sec.name().str(), LMA);
  Expr->setContext((Sec.name() + " LMA").str());
  OS->prolog().setLMA(Expr);
}

static void setOutputSectionVMA(Module &M, ELFSection &Sec, uint64_t Addr) {
  auto *OS = Sec.getOutputSection();
  ASSERT_NE(OS, nullptr);
  auto *Expr = make<Integer>(M, (Sec.name() + ".vma").str(), Addr);
  Expr->setContext((Sec.name() + " VMA").str());
  OS->prolog().OutputSectionVMA = Expr;
}

static MemoryCmd *ensureMemoryCmd(LinkerScript &Script) {
  if (auto *Cmd = Script.getMemoryCommand())
    return Cmd;
  auto *Cmd = make<MemoryCmd>();
  Script.setMemoryCommand(Cmd);
  return Cmd;
}

static ScriptMemoryRegion *createMemoryRegion(Module &M, llvm::StringRef Name,
                                              llvm::StringRef Attrs,
                                              uint64_t Origin,
                                              uint64_t Length) {
  MemoryCmd *Cmd = ensureMemoryCmd(M.getScript());
  auto *NameTok = make<StrToken>(Name.str());
  const StrToken *AttrTok =
      Attrs.empty() ? nullptr : make<StrToken>(Attrs.str());
  auto *OriginExpr = make<Integer>(M, (Name + ".origin").str(), Origin);
  OriginExpr->setContext((Name + " ORIGIN").str());
  auto *LengthExpr = make<Integer>(M, (Name + ".length").str(), Length);
  LengthExpr->setContext((Name + " LENGTH").str());
  auto *Desc =
      make<MemoryDesc>(MemorySpec(NameTok, AttrTok, OriginExpr, LengthExpr));
  static Input *MemInput = nullptr;
  static InputFile *MemFile = nullptr;
  if (!MemInput) {
    MemInput = make<Input>("memory-region-test", M.getConfig().getDiagEngine());
    MemInput->setResolvedPath("memory-region-test");
    MemFile = InputFile::create(MemInput, InputFile::GNULinkerScriptKind,
                                M.getConfig().getDiagEngine());
  }
  Desc->setInputFileInContext(MemFile);
  Desc->setLineNumberInContext(1);
  Cmd->pushBack(Desc);
  auto Activate = Desc->activate(M);
  EXPECT_TRUE(Activate.has_value());
  auto Region =
      M.getScript().getMemoryRegion(Name, (Name + " region lookup").str());
  EXPECT_TRUE(Region.has_value());
  return Region.value();
}

static void assignRegion(ELFSection &Sec, ScriptMemoryRegion &Region,
                         llvm::StringRef Name) {
  auto *OS = Sec.getOutputSection();
  ASSERT_NE(OS, nullptr);
  auto *Token = make<StrToken>(Name.str());
  OS->epilog().setRegion(&Region, Token);
  if (!OS->epilog().hasLMARegion() && !OS->prolog().hasLMA())
    OS->epilog().setLMARegion(&Region, Token);
}

static void assignLMARegion(ELFSection &Sec, ScriptMemoryRegion &Region,
                            llvm::StringRef Name) {
  auto *OS = Sec.getOutputSection();
  ASSERT_NE(OS, nullptr);
  OS->epilog().setLMARegion(&Region, make<StrToken>(Name.str()));
}

TEST(CreateProgramHeaders, InterpCreatesPhdrAndInterpSegments) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".interp", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC, /*Size=*/8, /*Align=*/1);
  addOutputSection(*H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
                   /*Size=*/16, /*Align=*/16);

  EXPECT_TRUE(H.Backend->relax());

  ELFSegment *PhdrSeg = H.Backend->elfSegmentTable().find(llvm::ELF::PT_PHDR);
  ASSERT_NE(PhdrSeg, nullptr);
  EXPECT_TRUE(segmentContains(*PhdrSeg, "__pHdr__"));

  ELFSegment *InterpSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_INTERP);
  ASSERT_NE(InterpSeg, nullptr);
  EXPECT_TRUE(segmentContains(*InterpSeg, ".interp"));
}

TEST(CreateProgramHeaders, ProgramHeadersLoadedWhenInterpPresent) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".interp", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC, /*Size=*/8, /*Align=*/1);
  addOutputSection(*H.Mod, ".dynamic", llvm::ELF::SHT_DYNAMIC,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                   /*Size=*/0x30, /*Align=*/8);

  EXPECT_TRUE(H.Backend->relax());

  ELFSegment *PhdrSeg = H.Backend->elfSegmentTable().find(llvm::ELF::PT_PHDR);
  ASSERT_NE(PhdrSeg, nullptr);
  EXPECT_TRUE(segmentContains(*PhdrSeg, "__pHdr__"));
  EXPECT_TRUE(H.Backend->isFileHeaderLoaded());
  EXPECT_TRUE(H.Backend->isPHDRSLoaded());
}

TEST(CreateProgramHeaders, DoesNotLoadProgramHeadersWhenUnneeded) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
                   /*Size=*/16, /*Align=*/16);

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(H.Backend->elfSegmentTable().find(llvm::ELF::PT_PHDR), nullptr);
  EXPECT_FALSE(H.Backend->isFileHeaderLoaded());
  EXPECT_FALSE(H.Backend->isPHDRSLoaded());
}

TEST(CreateProgramHeaders, DynamicEhFrameSFrameAndRelroSegments) {
  Harness H = createHarness();

  EXPECT_TRUE(H.Config->options().addZOption(ZOption(ZOption::Relro, 0)));
  H.Config->options().setSFrameHdr(true);

  ELFSection *Dynamic =
      addOutputSection(*H.Mod, ".dynamic", llvm::ELF::SHT_DYNAMIC,
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                       /*Size=*/32, /*Align=*/8);
  addOutputSection(*H.Mod, ".eh_frame_hdr", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC, /*Size=*/16, /*Align=*/4);
  addOutputSection(*H.Mod, ".sframe", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC, /*Size=*/16, /*Align=*/4);
  ELFSection *Data = addOutputSection(
      *H.Mod, ".data", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/16, /*Align=*/8);

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(countSegments(*H.Backend, llvm::ELF::PT_DYNAMIC), 1u);
  EXPECT_EQ(countSegments(*H.Backend, llvm::ELF::PT_GNU_EH_FRAME), 1u);
  EXPECT_EQ(countSegments(*H.Backend, llvm::ELF::PT_GNU_SFRAME), 1u);
  EXPECT_EQ(countSegments(*H.Backend, llvm::ELF::PT_GNU_RELRO), 1u);

  const ELFSegment *RelroSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_GNU_RELRO);
  ASSERT_NE(RelroSeg, nullptr);
  EXPECT_TRUE(segmentContains(*RelroSeg, ".dynamic"));

  uint64_t DynamicEnd = Dynamic->addr() + Dynamic->size();
  EXPECT_GE(Data->addr(), DynamicEnd + H.Backend->abiPageSize());
}

TEST(CreateProgramHeaders, GnuStackFromZExecStack) {
  Harness H = createHarness();

  EXPECT_TRUE(H.Config->options().addZOption(ZOption(ZOption::ExecStack, 0)));
  addOutputSection(*H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
                   /*Size=*/16, /*Align=*/16);

  EXPECT_TRUE(H.Backend->relax());

  const ELFSegment *StackSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_GNU_STACK);
  ASSERT_NE(StackSeg, nullptr);
  EXPECT_NE(StackSeg->flag() & llvm::ELF::PF_X, 0u);
}

TEST(CreateProgramHeaders, GnuStackFromNoteGNUStack) {
  Harness H = createHarness();

  ELFSection *NoteGNUStack =
      addOutputSection(*H.Mod, ".note.GNU-stack", llvm::ELF::SHT_PROGBITS,
                       llvm::ELF::SHF_EXECINSTR, /*Size=*/1, /*Align=*/1);
  addOutputSection(*H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
                   /*Size=*/16, /*Align=*/16);

  EXPECT_TRUE(H.Backend->relax());

  const ELFSegment *StackSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_GNU_STACK);
  ASSERT_NE(StackSeg, nullptr);
  EXPECT_NE(StackSeg->flag() & llvm::ELF::PF_X, 0u);

  EXPECT_EQ(NoteGNUStack->offset(), 0u);
}

TEST(CreateProgramHeaders, TLSCreatesPTTLS) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".tdata", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE |
                       llvm::ELF::SHF_TLS,
                   /*Size=*/8, /*Align=*/8);
  addOutputSection(*H.Mod, ".tbss", llvm::ELF::SHT_NOBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE |
                       llvm::ELF::SHF_TLS,
                   /*Size=*/8, /*Align=*/8);

  EXPECT_TRUE(H.Backend->relax());

  const ELFSegment *TLSSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_TLS);
  ASSERT_NE(TLSSeg, nullptr);
  EXPECT_TRUE(segmentContains(*TLSSeg, ".tdata"));
  EXPECT_TRUE(segmentContains(*TLSSeg, ".tbss"));

  ELFSection *TData = H.Mod->getScript().sectionMap().find(".tdata");
  ELFSection *TBss = H.Mod->getScript().sectionMap().find(".tbss");
  ASSERT_NE(TData, nullptr);
  ASSERT_NE(TBss, nullptr);
  auto *TDataLoad =
      H.Backend->getLoadSegmentForOutputSection(TData->getOutputSection());
  auto *TBssLoad =
      H.Backend->getLoadSegmentForOutputSection(TBss->getOutputSection());
  EXPECT_EQ(TDataLoad, TBssLoad);
}

TEST(CreateProgramHeaders, NoteSegmentsReuseWithinSameLoadSegment) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".note.a", llvm::ELF::SHT_NOTE, llvm::ELF::SHF_ALLOC,
                   /*Size=*/8, /*Align=*/4);
  addOutputSection(*H.Mod, ".note.b", llvm::ELF::SHT_NOTE, llvm::ELF::SHF_ALLOC,
                   /*Size=*/8, /*Align=*/4);

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(countSegments(*H.Backend, llvm::ELF::PT_NOTE), 1u);

  const ELFSegment *NoteSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_NOTE);
  ASSERT_NE(NoteSeg, nullptr);
  EXPECT_TRUE(segmentContains(*NoteSeg, ".note.a"));
  EXPECT_TRUE(segmentContains(*NoteSeg, ".note.b"));
}

TEST(CreateProgramHeaders, NoteSegmentsSplitWithNewLoadSegment) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".note.a", llvm::ELF::SHT_NOTE, llvm::ELF::SHF_ALLOC,
                   /*Size=*/8, /*Align=*/4);
  addOutputSection(*H.Mod, ".data", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                   /*Size=*/16, /*Align=*/8);
  addOutputSection(*H.Mod, ".note.b", llvm::ELF::SHT_NOTE, llvm::ELF::SHF_ALLOC,
                   /*Size=*/8, /*Align=*/4);

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(countSegments(*H.Backend, llvm::ELF::PT_NOTE), 2u);
}

TEST(CreateProgramHeaders, MixProgBitsAndBssCreatesSeparateLoadSegments) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".data1", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                   /*Size=*/16, /*Align=*/8);
  addOutputSection(*H.Mod, ".bss1", llvm::ELF::SHT_NOBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                   /*Size=*/16, /*Align=*/8);
  addOutputSection(*H.Mod, ".data2", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                   /*Size=*/16, /*Align=*/8);

  EXPECT_TRUE(H.Backend->relax());

  std::vector<ELFSegment *> Loads;
  for (auto *Seg : H.Backend->elfSegmentTable().segments())
    if (Seg->type() == llvm::ELF::PT_LOAD)
      Loads.push_back(Seg);

  ASSERT_GE(Loads.size(), 2u);
  EXPECT_TRUE(segmentContains(*Loads[0], ".data1"));
  EXPECT_TRUE(segmentContains(*Loads[0], ".bss1"));
  EXPECT_TRUE(segmentContains(*Loads[1], ".data2"));
}

TEST(CreateProgramHeaders, OnlyBssSectionsProduceAtMostOneLoadSegment) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".bss1", llvm::ELF::SHT_NOBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                   /*Size=*/16, /*Align=*/8);
  addOutputSection(*H.Mod, ".bss2", llvm::ELF::SHT_NOBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                   /*Size=*/32, /*Align=*/8);

  EXPECT_TRUE(H.Backend->relax());

  std::vector<ELFSegment *> Loads;
  for (auto *Seg : H.Backend->elfSegmentTable().segments())
    if (Seg->type() == llvm::ELF::PT_LOAD)
      Loads.push_back(Seg);

  ASSERT_LE(Loads.size(), 1u);
  if (!Loads.empty()) {
    EXPECT_TRUE(segmentContains(*Loads[0], ".bss1"));
    EXPECT_TRUE(segmentContains(*Loads[0], ".bss2"));
  }
}

TEST(CreateProgramHeaders, EmptyAllocSectionsShareLoadSegment) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".empty", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC, /*Size=*/0, /*Align=*/1);
  addOutputSection(*H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
                   /*Size=*/16, /*Align=*/16);

  EXPECT_TRUE(H.Backend->relax());

  std::vector<ELFSegment *> Loads;
  for (auto *Seg : H.Backend->elfSegmentTable().segments())
    if (Seg->type() == llvm::ELF::PT_LOAD)
      Loads.push_back(Seg);

  ASSERT_EQ(Loads.size(), 1u);
  EXPECT_TRUE(segmentContains(*Loads[0], ".text"));
}

TEST(CreateProgramHeaders, PhysicalAddrEqualsVirtualAddrWithoutLma) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
                   /*Size=*/16, /*Align=*/16);
  addOutputSection(*H.Mod, ".data", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                   /*Size=*/16, /*Align=*/8);

  EXPECT_TRUE(H.Backend->relax());

  ELFSection *Text = H.Mod->getScript().sectionMap().find(".text");
  ELFSection *Data = H.Mod->getScript().sectionMap().find(".data");
  ASSERT_NE(Text, nullptr);
  ASSERT_NE(Data, nullptr);

  EXPECT_EQ(Text->pAddr(), Text->addr());
  EXPECT_EQ(Data->pAddr(), Data->addr());
}

TEST(CreateProgramHeaders, NoAllocSectionsProduceNoLoadSegments) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".debug", llvm::ELF::SHT_PROGBITS,
                   /*Flags=*/0, /*Size=*/16, /*Align=*/1);
  addOutputSection(*H.Mod, ".comment", llvm::ELF::SHT_PROGBITS,
                   /*Flags=*/0, /*Size=*/8, /*Align=*/1);

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(countSegments(*H.Backend, llvm::ELF::PT_LOAD), 0u);
}

TEST(CreateProgramHeaders, EmptySectionWithLMAHasNoEffectOnFollowingLoad) {
  Harness H = createHarness();

  ELFSection *EmptyText = addOutputSection(
      *H.Mod, ".empty.text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0,
      /*Align=*/16);
  setOutputSectionLMA(*H.Mod, *EmptyText, 0x20000);

  addOutputSection(*H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
                   /*Size=*/0x200, /*Align=*/16);

  EXPECT_TRUE(H.Backend->relax());

  ELFSection *Text = H.Mod->getScript().sectionMap().find(".text");
  ASSERT_NE(Text, nullptr);
  ASSERT_NE(EmptyText, nullptr);

  EXPECT_NE(EmptyText->pAddr(), EmptyText->addr());
  EXPECT_EQ(Text->pAddr(), Text->addr());

  EXPECT_EQ(countSegments(*H.Backend, llvm::ELF::PT_LOAD), 1u);
}

TEST(CreateProgramHeaders, MemoryRegionsWithDifferentDescriptorsSplitLoads) {
  Harness H = createHarness();

  ScriptMemoryRegion *Rom =
      createMemoryRegion(*H.Mod, "ROM", "rx", 0x10000, 0x2000);
  ScriptMemoryRegion *Ram =
      createMemoryRegion(*H.Mod, "RAM", "rw", 0x20000, 0x2000);

  ELFSection *Text = addOutputSection(
      *H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0x40,
      /*Align=*/16);
  ELFSection *Data = addOutputSection(
      *H.Mod, ".data", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x40,
      /*Align=*/16);

  setOutputSectionVMA(*H.Mod, *Text, 0x10000);
  setOutputSectionVMA(*H.Mod, *Data, 0x20000);
  assignRegion(*Text, *Rom, "ROM");
  assignRegion(*Data, *Ram, "RAM");

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(Text->addr(), 0x10000u);
  EXPECT_EQ(Data->addr(), 0x20000u);

  ELFSegment *TextSeg = nullptr;
  ELFSegment *DataSeg = nullptr;
  for (auto *Seg : H.Backend->elfSegmentTable().segments()) {
    if (Seg->type() != llvm::ELF::PT_LOAD)
      continue;
    if (segmentContains(*Seg, ".text"))
      TextSeg = Seg;
    if (segmentContains(*Seg, ".data"))
      DataSeg = Seg;
  }
  ASSERT_NE(TextSeg, nullptr);
  ASSERT_NE(DataSeg, nullptr);
  EXPECT_NE(TextSeg, DataSeg);
}

TEST(CreateProgramHeaders, MemoryRegionWithDistinctLMAControlsPhysicalAddr) {
  Harness H = createHarness();

  ScriptMemoryRegion *Flash =
      createMemoryRegion(*H.Mod, "FLASH", "rx", 0x4000, 0x1000);
  ScriptMemoryRegion *Sram =
      createMemoryRegion(*H.Mod, "SRAM", "rw", 0x8000, 0x1000);

  ELFSection *InitData =
      addOutputSection(*H.Mod, ".initdata", llvm::ELF::SHT_PROGBITS,
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                       /*Size=*/0x20, /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *InitData, 0x4000);
  assignRegion(*InitData, *Flash, "FLASH");
  assignLMARegion(*InitData, *Sram, "SRAM");

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(InitData->addr(), 0x4000u);
  EXPECT_EQ(InitData->pAddr(), 0x8000u);
  EXPECT_NE(InitData->addr(), InitData->pAddr());
}

TEST(CreateProgramHeaders, MemoryRegionSequentialPlacementRespectsAlignment) {
  Harness H = createHarness();

  ScriptMemoryRegion *Rom =
      createMemoryRegion(*H.Mod, "ROM", "rx", 0x12000, 0x2000);

  ELFSection *Text = addOutputSection(
      *H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0x30,
      /*Align=*/0x20);
  ELFSection *Rodata =
      addOutputSection(*H.Mod, ".rodata", llvm::ELF::SHT_PROGBITS,
                       llvm::ELF::SHF_ALLOC, /*Size=*/0x18,
                       /*Align=*/0x40);

  setOutputSectionVMA(*H.Mod, *Text, 0x12000);
  assignRegion(*Text, *Rom, "ROM");
  assignRegion(*Rodata, *Rom, "ROM");

  EXPECT_TRUE(H.Backend->relax());

  uint64_t ExpectedRodata =
      alignTo(Text->addr() + Text->size(), Rodata->getAddrAlign());
  EXPECT_EQ(Text->addr(), 0x12000u);
  EXPECT_EQ(Rodata->addr(), ExpectedRodata);
  EXPECT_EQ(Text->pAddr(), Text->addr());
  EXPECT_EQ(Rodata->pAddr(), Rodata->addr());

  auto *TextSeg =
      H.Backend->getLoadSegmentForOutputSection(Text->getOutputSection());
  auto *RodataSeg =
      H.Backend->getLoadSegmentForOutputSection(Rodata->getOutputSection());
  EXPECT_EQ(TextSeg, RodataSeg);
}

TEST(CreateProgramHeaders, MemoryRegionExplicitLMAExpressionOverridesRegion) {
  Harness H = createHarness();

  ScriptMemoryRegion *Ram =
      createMemoryRegion(*H.Mod, "RAM", "rw", 0x3000, 0x4000);

  ELFSection *Data = addOutputSection(
      *H.Mod, ".data", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x40,
      /*Align=*/0x20);

  setOutputSectionVMA(*H.Mod, *Data, 0x3000);
  assignRegion(*Data, *Ram, "RAM");
  setOutputSectionLMA(*H.Mod, *Data, 0xA000);

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(Data->addr(), 0x3000u);
  EXPECT_EQ(Data->pAddr(), 0xA000u);
}

TEST(CreateProgramHeaders, MemoryRegionPermissionChangeSplitsLoadSegments) {
  Harness H = createHarness();

  H.Config->options().setROSegment(true);
  H.Config->options().setAlignSegments(false);

  ScriptMemoryRegion *Mem =
      createMemoryRegion(*H.Mod, "MEM", "rwx", 0x10000, 0x10000);

  ELFSection *Text = addOutputSection(
      *H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0x40,
      /*Align=*/0x10);
  ELFSection *Data = addOutputSection(
      *H.Mod, ".data", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x20,
      /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *Text, 0x10000);
  setOutputSectionVMA(*H.Mod, *Data, 0x11000);
  assignRegion(*Text, *Mem, "MEM");
  assignRegion(*Data, *Mem, "MEM");

  EXPECT_TRUE(H.Backend->relax());

  auto *TextSeg =
      H.Backend->getLoadSegmentForOutputSection(Text->getOutputSection());
  auto *DataSeg =
      H.Backend->getLoadSegmentForOutputSection(Data->getOutputSection());
  ASSERT_NE(TextSeg, nullptr);
  ASSERT_NE(DataSeg, nullptr);
  EXPECT_NE(TextSeg, DataSeg);

  EXPECT_NE(TextSeg->flag() & llvm::ELF::PF_X, 0u);
  EXPECT_EQ(DataSeg->flag() & llvm::ELF::PF_X, 0u);
  EXPECT_NE(DataSeg->flag() & llvm::ELF::PF_W, 0u);
}

TEST(CreateProgramHeaders, MemoryRegionDifferentLMARegionsSplitLoadSegments) {
  Harness H = createHarness();

  H.Config->options().setAlignSegments(false);

  ScriptMemoryRegion *Vma =
      createMemoryRegion(*H.Mod, "VMA", "rw", 0x20000, 0x2000);
  ScriptMemoryRegion *Lma1 =
      createMemoryRegion(*H.Mod, "LMA1", "rw", 0x8000, 0x2000);
  ScriptMemoryRegion *Lma2 =
      createMemoryRegion(*H.Mod, "LMA2", "rw", 0x9000, 0x2000);

  ELFSection *Data1 = addOutputSection(
      *H.Mod, ".data1", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x20,
      /*Align=*/1);
  ELFSection *Data2 = addOutputSection(
      *H.Mod, ".data2", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x20,
      /*Align=*/1);

  setOutputSectionVMA(*H.Mod, *Data1, 0x20000);
  setOutputSectionVMA(*H.Mod, *Data2, 0x20020);
  assignRegion(*Data1, *Vma, "VMA");
  assignRegion(*Data2, *Vma, "VMA");
  assignLMARegion(*Data1, *Lma1, "LMA1");
  assignLMARegion(*Data2, *Lma2, "LMA2");

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(Data1->pAddr(), 0x8000u);
  EXPECT_EQ(Data2->pAddr(), 0x9000u);

  auto *Seg1 =
      H.Backend->getLoadSegmentForOutputSection(Data1->getOutputSection());
  auto *Seg2 =
      H.Backend->getLoadSegmentForOutputSection(Data2->getOutputSection());
  ASSERT_NE(Seg1, nullptr);
  ASSERT_NE(Seg2, nullptr);
  EXPECT_NE(Seg1, Seg2);
}

TEST(CreateProgramHeaders, MemoryRegionSharedLMARegionUsesSequentialPhysAddrs) {
  Harness H = createHarness();

  H.Config->options().setAlignSegments(false);

  ScriptMemoryRegion *Vma =
      createMemoryRegion(*H.Mod, "VMA", "rw", 0x30000, 0x2000);
  ScriptMemoryRegion *Flash =
      createMemoryRegion(*H.Mod, "FLASH", "rw", 0xA000, 0x2000);

  ELFSection *Data1 = addOutputSection(
      *H.Mod, ".data1", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x10,
      /*Align=*/1);
  ELFSection *Data2 = addOutputSection(
      *H.Mod, ".data2", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x10,
      /*Align=*/1);

  setOutputSectionVMA(*H.Mod, *Data1, 0x30000);
  setOutputSectionVMA(*H.Mod, *Data2, 0x30010);
  assignRegion(*Data1, *Vma, "VMA");
  assignRegion(*Data2, *Vma, "VMA");
  assignLMARegion(*Data1, *Flash, "FLASH");
  assignLMARegion(*Data2, *Flash, "FLASH");

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(Data1->pAddr(), 0xA000u);
  EXPECT_EQ(Data2->pAddr(), 0xA010u);
  EXPECT_NE(Data1->pAddr(), Data1->addr());
  EXPECT_NE(Data2->pAddr(), Data2->addr());
}

TEST(CreateProgramHeaders, MemoryRegionMixProgBitsAndBssStillSplitsAfterBss) {
  Harness H = createHarness();

  ScriptMemoryRegion *Ram =
      createMemoryRegion(*H.Mod, "RAM", "rw", 0x20000, 0x2000);

  ELFSection *Data1 = addOutputSection(
      *H.Mod, ".data1", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x10,
      /*Align=*/0x10);
  ELFSection *Bss1 =
      addOutputSection(*H.Mod, ".bss1", llvm::ELF::SHT_NOBITS,
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                       /*Size=*/0x10, /*Align=*/0x10);
  ELFSection *Data2 = addOutputSection(
      *H.Mod, ".data2", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x10,
      /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *Data1, 0x20000);
  setOutputSectionVMA(*H.Mod, *Bss1, 0x20010);
  setOutputSectionVMA(*H.Mod, *Data2, 0x20020);
  assignRegion(*Data1, *Ram, "RAM");
  assignRegion(*Bss1, *Ram, "RAM");
  assignRegion(*Data2, *Ram, "RAM");

  EXPECT_TRUE(H.Backend->relax());

  ELFSegment *Load0 = nullptr;
  ELFSegment *Load1 = nullptr;
  for (auto *Seg : H.Backend->elfSegmentTable().segments()) {
    if (Seg->type() != llvm::ELF::PT_LOAD)
      continue;
    if (!Load0)
      Load0 = Seg;
    else if (!Load1)
      Load1 = Seg;
  }
  ASSERT_NE(Load0, nullptr);
  ASSERT_NE(Load1, nullptr);

  EXPECT_TRUE(segmentContains(*Load0, ".data1"));
  EXPECT_TRUE(segmentContains(*Load0, ".bss1"));
  EXPECT_TRUE(segmentContains(*Load1, ".data2"));
}

TEST(CreateProgramHeaders, MemoryRegionOnlyBssProducesAtMostOneLoadSegment) {
  Harness H = createHarness();

  ScriptMemoryRegion *Ram =
      createMemoryRegion(*H.Mod, "RAM", "rw", 0x30000, 0x2000);

  ELFSection *Bss1 =
      addOutputSection(*H.Mod, ".bss1", llvm::ELF::SHT_NOBITS,
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                       /*Size=*/0x20, /*Align=*/0x10);
  ELFSection *Bss2 =
      addOutputSection(*H.Mod, ".bss2", llvm::ELF::SHT_NOBITS,
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                       /*Size=*/0x20, /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *Bss1, 0x30000);
  setOutputSectionVMA(*H.Mod, *Bss2, 0x30040);
  assignRegion(*Bss1, *Ram, "RAM");
  assignRegion(*Bss2, *Ram, "RAM");

  EXPECT_TRUE(H.Backend->relax());

  std::vector<ELFSegment *> Loads;
  for (auto *Seg : H.Backend->elfSegmentTable().segments())
    if (Seg->type() == llvm::ELF::PT_LOAD)
      Loads.push_back(Seg);

  ASSERT_LE(Loads.size(), 1u);
  if (!Loads.empty()) {
    EXPECT_TRUE(segmentContains(*Loads[0], ".bss1"));
    EXPECT_TRUE(segmentContains(*Loads[0], ".bss2"));
  }
}

TEST(CreateProgramHeaders, MemoryRegionNonAllocInMiddleNotInLoadSegment) {
  Harness H = createHarness();

  ScriptMemoryRegion *Rom =
      createMemoryRegion(*H.Mod, "ROM", "rx", 0x40000, 0x2000);

  ELFSection *Text = addOutputSection(
      *H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0x40,
      /*Align=*/0x10);
  ELFSection *Debug =
      addOutputSection(*H.Mod, ".debug", llvm::ELF::SHT_PROGBITS,
                       /*Flags=*/0, /*Size=*/0x10,
                       /*Align=*/1);
  ELFSection *Data = addOutputSection(
      *H.Mod, ".data", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x20,
      /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *Text, 0x40000);
  setOutputSectionVMA(*H.Mod, *Data, 0x41000);
  assignRegion(*Text, *Rom, "ROM");
  assignRegion(*Data, *Rom, "ROM");

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(Debug->addr(), 0u);
  EXPECT_EQ(Debug->pAddr(), 0u);
  for (auto *Seg : H.Backend->elfSegmentTable().segments()) {
    if (Seg->type() == llvm::ELF::PT_LOAD)
      EXPECT_FALSE(segmentContains(*Seg, ".debug"));
  }

  EXPECT_EQ(Text->addr(), 0x40000u);
  EXPECT_EQ(Data->addr(), 0x41000u);
}

TEST(CreateProgramHeaders, MemoryRegionDiscardSectionNotInLoadSegment) {
  Harness H = createHarness();

  ScriptMemoryRegion *Ram =
      createMemoryRegion(*H.Mod, "RAM", "rw", 0x50000, 0x2000);

  ELFSection *Text = addOutputSection(
      *H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0x20,
      /*Align=*/0x10);
  ELFSection *Discard =
      addOutputSection(*H.Mod, "/DISCARD/", llvm::ELF::SHT_PROGBITS,
                       llvm::ELF::SHF_ALLOC, /*Size=*/0x10, /*Align=*/1);
  ELFSection *Data = addOutputSection(
      *H.Mod, ".data", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x20,
      /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *Text, 0x50000);
  setOutputSectionVMA(*H.Mod, *Data, 0x50100);
  assignRegion(*Text, *Ram, "RAM");
  assignRegion(*Data, *Ram, "RAM");
  assignRegion(*Discard, *Ram, "RAM");

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(Discard->addr(), 0u);
  EXPECT_EQ(Discard->pAddr(), 0u);
  for (auto *Seg : H.Backend->elfSegmentTable().segments()) {
    if (Seg->type() == llvm::ELF::PT_LOAD)
      EXPECT_FALSE(segmentContains(*Seg, "/DISCARD/"));
  }
}

TEST(CreateProgramHeaders,
     MemoryRegionEmptySectionWithLmaRegionDoesNotShiftPhysAddrs) {
  Harness H = createHarness();

  ScriptMemoryRegion *Vma =
      createMemoryRegion(*H.Mod, "ROM", "rx", 0x60000, 0x2000);
  ScriptMemoryRegion *Lma =
      createMemoryRegion(*H.Mod, "FLASH", "rx", 0x1000, 0x2000);

  ELFSection *Empty = addOutputSection(
      *H.Mod, ".empty", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0,
      /*Align=*/0x10);
  ELFSection *Text = addOutputSection(
      *H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0x20,
      /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *Empty, 0x60000);
  setOutputSectionVMA(*H.Mod, *Text, 0x61000);
  assignRegion(*Empty, *Vma, "ROM");
  assignRegion(*Text, *Vma, "ROM");
  assignLMARegion(*Empty, *Lma, "FLASH");
  assignLMARegion(*Text, *Lma, "FLASH");

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(Empty->addr(), 0x60000u);
  EXPECT_EQ(Empty->pAddr(), 0x1000u);

  EXPECT_EQ(Text->addr(), 0x61000u);
  EXPECT_EQ(Text->pAddr(), 0x1000u);
}

TEST(CreateProgramHeaders, MemoryRegionWithInterpLoadsProgramHeaders) {
  Harness H = createHarness();

  ScriptMemoryRegion *Rom =
      createMemoryRegion(*H.Mod, "ROM", "rx", 0x10000, 0x2000);

  ELFSection *Interp = addOutputSection(
      *H.Mod, ".interp", llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC,
      /*Size=*/8, /*Align=*/1);
  ELFSection *Text = addOutputSection(
      *H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0x20,
      /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *Interp, 0x10000);
  setOutputSectionVMA(*H.Mod, *Text, 0x10100);
  assignRegion(*Interp, *Rom, "ROM");
  assignRegion(*Text, *Rom, "ROM");

  EXPECT_TRUE(H.Backend->relax());

  ELFSegment *PhdrSeg = H.Backend->elfSegmentTable().find(llvm::ELF::PT_PHDR);
  ASSERT_NE(PhdrSeg, nullptr);
  EXPECT_TRUE(segmentContains(*PhdrSeg, "__pHdr__"));

  ELFSegment *InterpSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_INTERP);
  ASSERT_NE(InterpSeg, nullptr);
  EXPECT_TRUE(segmentContains(*InterpSeg, ".interp"));

  auto *LoadInterp =
      H.Backend->getLoadSegmentForOutputSection(Interp->getOutputSection());
  ASSERT_NE(LoadInterp, nullptr);
  EXPECT_TRUE(segmentContains(*LoadInterp, ".interp"));
}

TEST(CreateProgramHeaders, RoSegmentSplitsRXAndRIntoSeparateLoadSegments) {
  Harness H = createHarness();

  H.Config->options().setROSegment(true);
  H.Config->options().setAlignSegments(false);

  addOutputSection(*H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
                   /*Size=*/0x200, /*Align=*/16);
  addOutputSection(*H.Mod, ".rodata", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC, /*Size=*/0x44, /*Align=*/4);

  EXPECT_TRUE(H.Backend->relax());

  ELFSegment *TextSeg = nullptr;
  ELFSegment *RodataSeg = nullptr;
  for (auto *Seg : H.Backend->elfSegmentTable().segments()) {
    if (Seg->type() != llvm::ELF::PT_LOAD)
      continue;
    if (segmentContains(*Seg, ".text"))
      TextSeg = Seg;
    if (segmentContains(*Seg, ".rodata"))
      RodataSeg = Seg;
  }

  ASSERT_NE(TextSeg, nullptr);
  ASSERT_NE(RodataSeg, nullptr);
  EXPECT_NE(TextSeg, RodataSeg);
}

TEST(CreateProgramHeaders, NoRoSegmentMergesRODataIntoTextLoadSegment) {
  Harness H = createHarness();

  H.Config->options().setROSegment(false);
  H.Config->options().setAlignSegments(false);

  addOutputSection(*H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
                   /*Size=*/0x200, /*Align=*/16);
  addOutputSection(*H.Mod, ".rodata", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC, /*Size=*/0x44, /*Align=*/4);

  EXPECT_TRUE(H.Backend->relax());

  ELFSegment *LoadSeg = nullptr;
  for (auto *Seg : H.Backend->elfSegmentTable().segments()) {
    if (Seg->type() != llvm::ELF::PT_LOAD)
      continue;
    if (segmentContains(*Seg, ".text") && segmentContains(*Seg, ".rodata")) {
      LoadSeg = Seg;
      break;
    }
  }

  ASSERT_NE(LoadSeg, nullptr);
}

TEST(CreateProgramHeaders,
     SeparateCodePageAlignsBoundaryWhenAlignSegmentsDisabled) {
  Harness H = createHarness();

  H.Config->options().setROSegment(true);
  H.Config->options().setAlignSegments(false);
  EXPECT_TRUE(
      H.Config->options().addZOption(ZOption(ZOption::SeparateCode, 0)));

  ELFSection *Text = addOutputSection(
      *H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0x200,
      /*Align=*/16);
  ELFSection *Rodata =
      addOutputSection(*H.Mod, ".rodata", llvm::ELF::SHT_PROGBITS,
                       llvm::ELF::SHF_ALLOC, /*Size=*/0x44,
                       /*Align=*/4);

  EXPECT_TRUE(H.Backend->relax());

  uint64_t Expected =
      alignTo(Text->addr() + Text->size(), H.Backend->abiPageSize());
  EXPECT_EQ(Rodata->addr(), Expected);
  EXPECT_EQ(Rodata->addr() % H.Backend->abiPageSize(), 0u);
}

TEST(CreateProgramHeaders,
     NoSeparateCodeAvoidsPageAlignWhenAlignSegmentsDisabled) {
  Harness H = createHarness();

  H.Config->options().setROSegment(true);
  H.Config->options().setAlignSegments(false);
  EXPECT_TRUE(
      H.Config->options().addZOption(ZOption(ZOption::NoSeparateCode, 0)));

  ELFSection *Text = addOutputSection(
      *H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0x200,
      /*Align=*/16);
  ELFSection *Rodata =
      addOutputSection(*H.Mod, ".rodata", llvm::ELF::SHT_PROGBITS,
                       llvm::ELF::SHF_ALLOC, /*Size=*/0x44,
                       /*Align=*/4);

  EXPECT_TRUE(H.Backend->relax());

  uint64_t Expected =
      alignTo(Text->addr() + Text->size(), Rodata->getAddrAlign());
  EXPECT_EQ(Rodata->addr(), Expected);
  EXPECT_NE(Rodata->addr() % H.Backend->abiPageSize(), 0u);
}

TEST(CreateProgramHeaders, OnlyTdataCreatesPTTLS) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".tdata", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE |
                       llvm::ELF::SHF_TLS,
                   /*Size=*/8, /*Align=*/8);

  EXPECT_TRUE(H.Backend->relax());

  const ELFSegment *TLSSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_TLS);
  ASSERT_NE(TLSSeg, nullptr);
  EXPECT_TRUE(segmentContains(*TLSSeg, ".tdata"));
  EXPECT_FALSE(segmentContains(*TLSSeg, ".tbss"));
}

TEST(CreateProgramHeaders, OnlyTbssCreatesPTTLS) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".tbss", llvm::ELF::SHT_NOBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE |
                       llvm::ELF::SHF_TLS,
                   /*Size=*/8, /*Align=*/8);

  EXPECT_TRUE(H.Backend->relax());

  const ELFSegment *TLSSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_TLS);
  ASSERT_NE(TLSSeg, nullptr);
  EXPECT_TRUE(segmentContains(*TLSSeg, ".tbss"));
  EXPECT_FALSE(segmentContains(*TLSSeg, ".tdata"));
}

TEST(CreateProgramHeaders, NonAllocSectionInMiddleNotInLoadSegment) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
                   /*Size=*/0x20, /*Align=*/16);
  ELFSection *Debug =
      addOutputSection(*H.Mod, ".debug", llvm::ELF::SHT_PROGBITS,
                       /*Flags=*/0, /*Size=*/0x10,
                       /*Align=*/1);
  addOutputSection(*H.Mod, ".data", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                   /*Size=*/0x20, /*Align=*/8);

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(Debug->addr(), 0u);
  EXPECT_EQ(Debug->pAddr(), 0u);

  for (auto *Seg : H.Backend->elfSegmentTable().segments()) {
    if (Seg->type() == llvm::ELF::PT_LOAD)
      EXPECT_FALSE(segmentContains(*Seg, ".debug"));
  }
}

TEST(CreateProgramHeaders, DiscardSectionIsNotPlacedInLoadSegment) {
  Harness H = createHarness();

  addOutputSection(*H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
                   /*Size=*/0x20, /*Align=*/16);
  ELFSection *Discard =
      addOutputSection(*H.Mod, "/DISCARD/", llvm::ELF::SHT_PROGBITS,
                       llvm::ELF::SHF_ALLOC, /*Size=*/0x10, /*Align=*/1);
  addOutputSection(*H.Mod, ".data", llvm::ELF::SHT_PROGBITS,
                   llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                   /*Size=*/0x20, /*Align=*/8);

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(Discard->addr(), 0u);
  EXPECT_EQ(Discard->pAddr(), 0u);

  for (auto *Seg : H.Backend->elfSegmentTable().segments()) {
    if (Seg->type() == llvm::ELF::PT_LOAD)
      EXPECT_FALSE(segmentContains(*Seg, "/DISCARD/"));
  }
}

TEST(CreateProgramHeaders, PhdrsSpecifiedUsesScriptSegments) {
  Harness H = createHarness();

  H.Mod->getScript().setPhdrsSpecified();

  PhdrSpec LoadSpec;
  LoadSpec.init();
  LoadSpec.Name = make<StrToken>("LOAD");
  LoadSpec.ThisType = llvm::ELF::PT_LOAD;
  LoadSpec.ScriptHasFileHdr = true;
  LoadSpec.ScriptHasPhdr = true;
  H.Mod->getScript().insertPhdrSpec(LoadSpec);

  ELFSection *Text = addOutputSection(
      *H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/16,
      /*Align=*/16);

  auto *OS = Text->getOutputSection();
  ASSERT_NE(OS, nullptr);
  auto *PhdrsList = make<StringList>();
  PhdrsList->pushBack(make<StrToken>("LOAD"));
  OS->epilog().ScriptPhdrs = PhdrsList;

  EXPECT_FALSE(H.Backend->createSegmentsFromLinkerScript());
  EXPECT_TRUE(H.Backend->relax());
  EXPECT_GE(countSegments(*H.Backend, llvm::ELF::PT_LOAD), 1u);

  ASSERT_NE(H.Backend->getEhdr(), nullptr);
  ASSERT_NE(H.Backend->getPhdr(), nullptr);
  EXPECT_NE(H.Backend->getEhdr()->getOutputSection(), nullptr);
  EXPECT_NE(H.Backend->getPhdr()->getOutputSection(), nullptr);
}

TEST(CreateProgramHeaders, PhdrsCommandCreatesSegmentsFromScript) {
  Harness H = createHarness();

  LinkerScript &Script = H.Mod->getScript();
  Script.setPhdrsSpecified();

  auto makeSpec = [&](llvm::StringRef Name, uint32_t Type) {
    PhdrSpec Spec;
    Spec.init();
    Spec.Name = make<StrToken>(Name.str());
    Spec.ThisType = Type;
    Script.insertPhdrSpec(Spec);
  };

  makeSpec("PH_TEXT", llvm::ELF::PT_LOAD);
  makeSpec("PH_TLS", llvm::ELF::PT_TLS);
  makeSpec("PH_STACK", llvm::ELF::PT_GNU_STACK);

  ELFSection *Text = addOutputSection(
      *H.Mod, ".text", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, /*Size=*/0x20,
      /*Align=*/16);
  ELFSection *Tdata = addOutputSection(
      *H.Mod, ".tdata", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE | llvm::ELF::SHF_TLS,
      /*Size=*/0x10, /*Align=*/8);

  auto attachPhdr = [&](ELFSection *Sec, llvm::StringRef Name) {
    auto *List = make<StringList>();
    List->pushBack(make<StrToken>(Name.str()));
    Sec->getOutputSection()->epilog().ScriptPhdrs = List;
  };
  attachPhdr(Text, "PH_TEXT");
  attachPhdr(Tdata, "PH_TLS");

  EXPECT_FALSE(H.Backend->createSegmentsFromLinkerScript());
  EXPECT_TRUE(H.Backend->relax());

  ELFSegment *TextSeg = nullptr;
  ELFSegment *TlsSeg = nullptr;
  for (auto *Seg : H.Backend->elfSegmentTable().segments()) {
    if (Seg->name() == "PH_TEXT")
      TextSeg = Seg;
    if (Seg->name() == "PH_TLS")
      TlsSeg = Seg;
  }

  ASSERT_NE(TextSeg, nullptr);
  ASSERT_NE(TlsSeg, nullptr);
  EXPECT_TRUE(segmentContains(*TextSeg, ".text"));
  EXPECT_TRUE(segmentContains(*TlsSeg, ".tdata"));
}

TEST(CreateProgramHeaders, TlsSectionsRespectMemoryRegions) {
  Harness H = createHarness();

  ScriptMemoryRegion *Tls =
      createMemoryRegion(*H.Mod, "TLSRAM", "rw", 0x7000, 0x1000);

  ELFSection *Tdata = addOutputSection(
      *H.Mod, ".tdata", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE | llvm::ELF::SHF_TLS,
      /*Size=*/0x20, /*Align=*/0x10);
  ELFSection *Tbss = addOutputSection(
      *H.Mod, ".tbss", llvm::ELF::SHT_NOBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE | llvm::ELF::SHF_TLS,
      /*Size=*/0x20, /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *Tdata, 0x7000);
  setOutputSectionVMA(*H.Mod, *Tbss, 0x7020);
  assignRegion(*Tdata, *Tls, "TLSRAM");
  assignRegion(*Tbss, *Tls, "TLSRAM");

  EXPECT_TRUE(H.Backend->relax());

  const ELFSegment *TlsSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_TLS);
  ASSERT_NE(TlsSeg, nullptr);
  EXPECT_TRUE(segmentContains(*TlsSeg, ".tdata"));
  EXPECT_TRUE(segmentContains(*TlsSeg, ".tbss"));

  EXPECT_EQ(Tdata->addr(), 0x7000u);
  EXPECT_EQ(Tbss->addr(), 0x7020u);
  EXPECT_EQ(Tdata->pAddr(), Tdata->addr());
  EXPECT_EQ(Tbss->pAddr(), Tbss->addr());

  auto *LoadTdata =
      H.Backend->getLoadSegmentForOutputSection(Tdata->getOutputSection());
  auto *LoadTbss =
      H.Backend->getLoadSegmentForOutputSection(Tbss->getOutputSection());
  EXPECT_EQ(LoadTdata, LoadTbss);
}

TEST(CreateProgramHeaders, OnlyTdataInMemoryRegionCreatesPTTLS) {
  Harness H = createHarness();

  ScriptMemoryRegion *Tls =
      createMemoryRegion(*H.Mod, "TLSRAM", "rw", 0x9000, 0x1000);

  ELFSection *Tdata = addOutputSection(
      *H.Mod, ".tdata", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE | llvm::ELF::SHF_TLS,
      /*Size=*/0x20, /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *Tdata, 0x9000);
  assignRegion(*Tdata, *Tls, "TLSRAM");

  EXPECT_TRUE(H.Backend->relax());

  const ELFSegment *TlsSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_TLS);
  ASSERT_NE(TlsSeg, nullptr);
  EXPECT_TRUE(segmentContains(*TlsSeg, ".tdata"));
  EXPECT_FALSE(segmentContains(*TlsSeg, ".tbss"));
}

TEST(CreateProgramHeaders, OnlyTbssInMemoryRegionCreatesPTTLS) {
  Harness H = createHarness();

  ScriptMemoryRegion *Tls =
      createMemoryRegion(*H.Mod, "TLSRAM", "rw", 0xA000, 0x1000);

  ELFSection *Tbss = addOutputSection(
      *H.Mod, ".tbss", llvm::ELF::SHT_NOBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE | llvm::ELF::SHF_TLS,
      /*Size=*/0x20, /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *Tbss, 0xA000);
  assignRegion(*Tbss, *Tls, "TLSRAM");

  EXPECT_TRUE(H.Backend->relax());

  const ELFSegment *TlsSeg =
      H.Backend->elfSegmentTable().find(llvm::ELF::PT_TLS);
  ASSERT_NE(TlsSeg, nullptr);
  EXPECT_TRUE(segmentContains(*TlsSeg, ".tbss"));
  EXPECT_FALSE(segmentContains(*TlsSeg, ".tdata"));
}

TEST(CreateProgramHeaders, AlignWithInputKeepsPhysicalDelta) {
  Harness H = createHarness();

  ScriptMemoryRegion *Ram =
      createMemoryRegion(*H.Mod, "RAM", "rw", 0x8000, 0x2000);

  ELFSection *Data = addOutputSection(
      *H.Mod, ".data", llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*Size=*/0x30,
      /*Align=*/0x10);
  ELFSection *Bss =
      addOutputSection(*H.Mod, ".bss", llvm::ELF::SHT_NOBITS,
                       llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
                       /*Size=*/0x20, /*Align=*/0x10);

  setOutputSectionVMA(*H.Mod, *Data, 0x8000);
  setOutputSectionVMA(*H.Mod, *Bss, 0x8040);
  assignRegion(*Data, *Ram, "RAM");
  assignRegion(*Bss, *Ram, "RAM");
  Bss->getOutputSection()->prolog().setAlignWithInput();

  EXPECT_TRUE(H.Backend->relax());

  EXPECT_EQ(Data->pAddr(), Data->addr());
  EXPECT_EQ(Bss->pAddr() - Data->pAddr(), Bss->addr() - Data->addr());
}

} // namespace
