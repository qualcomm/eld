//===- VersionedSymbolsSymbolResolutionTest.cpp---------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SymbolResolutionTest.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Linker.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/Input.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/Target.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/NamePool.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "gtest/gtest.h"

using namespace eld;

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
namespace {

class FakeTargetInfo final : public TargetInfo {
public:
  explicit FakeTargetInfo(LinkerConfig &Config) : TargetInfo(Config) {}

  uint32_t machine() const override { return llvm::ELF::EM_NONE; }
  std::string getMachineStr() const override { return "fake"; }
  uint64_t flags() const override { return 0; }
  uint64_t startAddr(bool, bool, bool) const override { return 0; }
};

class FakeBackend final : public GNULDBackend {
public:
  FakeBackend(Module &M, TargetInfo *Info) : GNULDBackend(M, Info) {}

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

GNULDBackend *createFakeBackend(Module &M) {
  return make<FakeBackend>(M, make<FakeTargetInfo>(M.getConfig()));
}

class VersionedSymbolsSymbolResolutionTest : public SymbolResolutionTest {
public:
  VersionedSymbolsSymbolResolutionTest() : SymbolResolutionTest() {
    Linker *L = make<Linker>(*m_Module, *m_Config);
    Target T;
    T.Name = "fake";
    T.Machine = llvm::ELF::EM_NONE;
    T.Is64bit = true;
    T.GNULDBackendCtorFn = createFakeBackend;
    EXPECT_TRUE(L->initBackend(&T));
    std::vector<InputAction *> InputActions;
    EXPECT_TRUE(L->initializeInputTree(InputActions));
  }

protected:
  InputFile *object(llvm::StringRef Name) {
    Input *I = make<Input>(Name.str(), m_DiagEngine);
    I->setResolvedPath(Name.str());
    ELFObjectFile *Obj = make<ELFObjectFile>(I, m_DiagEngine);
    m_Module->getObjectList().push_back(Obj);
    return Obj;
  }

  ELFSection *textSection() {
    return make<ELFSection>(LDFileFormat::Kind::Regular, ".text.foo",
                            llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, 0,
                            0, llvm::ELF::SHT_PROGBITS, 0, nullptr, 0, 0);
  }

  LDSymbol *define(InputFile &File, llvm::StringRef Name,
                   ResolveInfo::Binding Binding, uint64_t Value) {
    return m_IRBuilder->addSymbol(
        File, Name.str(), ResolveInfo::Type::Function,
        ResolveInfo::Desc::Define, Binding, /*pSize=*/12, Value, textSection(),
        ResolveInfo::Visibility::Default, /*isPostLTOPhase=*/false,
        /*shndx=*/1, /*idx=*/1);
  }

  LDSymbol *ref(InputFile &File, llvm::StringRef Name) {
    return m_IRBuilder->addSymbol(
        File, Name.str(), ResolveInfo::Type::Function,
        ResolveInfo::Desc::Undefined, ResolveInfo::Binding::Global,
        /*pSize=*/0, /*pValue=*/0, /*pSection=*/nullptr,
        ResolveInfo::Visibility::Default, /*isPostLTOPhase=*/false,
        /*shndx=*/0, /*idx=*/1);
  }

  LDSymbol *findInfo(llvm::StringRef Name) {
    ResolveInfo *Info = m_Module->getNamePool().findInfo(Name.str());
    return Info ? Info->outSymbol() : nullptr;
  }

  void normalizeSymbols() { m_IRBuilder->normalizeSymbols(); }
};

} // namespace

TEST_F(VersionedSymbolsSymbolResolutionTest,
       StrongDefaultDefinitionResolvesPlainReference) {
  InputFile *File1 = object("1.o");
  LDSymbol *FooDefault = define(*File1, "foo@@V1", ResolveInfo::Binding::Global,
                                /*Value=*/0x10);

  InputFile *File2 = object("2.o");
  LDSymbol *FooRef = ref(*File2, "foo");

  normalizeSymbols();
  ASSERT_NE(FooDefault, nullptr);
  ASSERT_NE(FooRef, nullptr);
  EXPECT_EQ(FooRef->resolveInfo()->outSymbol(), FooDefault);
  EXPECT_TRUE(FooDefault->resolveInfo()->isDefaultVersion());
}

TEST_F(VersionedSymbolsSymbolResolutionTest,
       StrongDefaultDefinitionResolvesVersionedReference) {
  InputFile *File1 = object("1.o");
  LDSymbol *FooDefault = define(*File1, "foo@@V1", ResolveInfo::Binding::Global,
                                /*Value=*/0x10);

  InputFile *File2 = object("2.o");
  LDSymbol *FooV1Ref = ref(*File2, "foo@V1");

  normalizeSymbols();
  ASSERT_NE(FooDefault, nullptr);
  ASSERT_NE(FooV1Ref, nullptr);
  EXPECT_EQ(FooV1Ref->resolveInfo()->outSymbol(), FooDefault);
  EXPECT_EQ(FooV1Ref->resolveInfo(), FooDefault->resolveInfo());
}

TEST_F(VersionedSymbolsSymbolResolutionTest,
       StrongNonDefaultDefinitionResolvesVersionedReference) {
  InputFile *File1 = object("1.o");
  LDSymbol *FooV1 = define(*File1, "foo@V1", ResolveInfo::Binding::Global,
                           /*Value=*/0x10);

  InputFile *File2 = object("2.o");
  LDSymbol *FooV1Ref = ref(*File2, "foo@V1");

  normalizeSymbols();
  ASSERT_NE(FooV1, nullptr);
  ASSERT_NE(FooV1Ref, nullptr);
  EXPECT_EQ(FooV1Ref->resolveInfo()->outSymbol(), FooV1);
  EXPECT_EQ(FooV1Ref->resolveInfo(), FooV1->resolveInfo());
  EXPECT_FALSE(FooV1->resolveInfo()->isDefaultVersion());
}

TEST_F(VersionedSymbolsSymbolResolutionTest,
       StrongDefaultBeatsWeakNonDefaultForVersionedReference) {
  InputFile *File1 = object("1.o");
  LDSymbol *FooDefault = define(*File1, "foo@@V1", ResolveInfo::Binding::Global,
                                /*Value=*/0x10);

  InputFile *File2 = object("2.o");
  LDSymbol *FooWeakV1 = define(*File2, "foo@V1", ResolveInfo::Binding::Weak,
                               /*Value=*/0x20);

  InputFile *File3 = object("3.o");
  LDSymbol *FooV1Ref = ref(*File3, "foo@V1");

  normalizeSymbols();
  ASSERT_NE(FooDefault, nullptr);
  ASSERT_NE(FooWeakV1, nullptr);
  ASSERT_NE(FooV1Ref, nullptr);
  EXPECT_EQ(FooV1Ref->resolveInfo()->outSymbol(), FooDefault);
  EXPECT_EQ(FooWeakV1->resolveInfo()->outSymbol(), FooDefault);
  EXPECT_EQ(FooV1Ref->resolveInfo(), FooDefault->resolveInfo());
}

TEST_F(VersionedSymbolsSymbolResolutionTest,
       StrongNonDefaultBeatsWeakDefaultForVersionedSlot) {
  InputFile *File1 = object("1.o");
  LDSymbol *FooWeakDefault =
      define(*File1, "foo@@V1", ResolveInfo::Binding::Weak, /*Value=*/0x10);

  InputFile *File2 = object("2.o");
  LDSymbol *FooV1 = define(*File2, "foo@V1", ResolveInfo::Binding::Global,
                           /*Value=*/0x20);

  InputFile *File3 = object("3.o");
  LDSymbol *FooRef = ref(*File3, "foo");
  LDSymbol *FooV1Ref = ref(*File3, "foo@V1");

  normalizeSymbols();
  ASSERT_NE(FooWeakDefault, nullptr);
  ASSERT_NE(FooV1, nullptr);
  ASSERT_NE(FooRef, nullptr);
  ASSERT_NE(FooV1Ref, nullptr);
  EXPECT_EQ(FooRef->resolveInfo()->outSymbol(), FooV1);
  EXPECT_EQ(FooV1Ref->resolveInfo()->outSymbol(), FooV1);
  EXPECT_EQ(FooRef->resolveInfo(), FooV1->resolveInfo());
  EXPECT_EQ(FooRef->resolveInfo(), FooV1->resolveInfo());
  EXPECT_TRUE(FooV1->resolveInfo()->isDefaultVersion());
}

TEST_F(VersionedSymbolsSymbolResolutionTest,
       StrongPlainBeatsWeakDefaultForPlainSlot) {
  InputFile *File1 = object("1.o");
  LDSymbol *FooWeakDefault =
      define(*File1, "foo@@V1", ResolveInfo::Binding::Weak, /*Value=*/0x10);

  InputFile *File2 = object("2.o");
  LDSymbol *Foo = define(*File2, "foo", ResolveInfo::Binding::Global,
                         /*Value=*/0x20);

  InputFile *File3 = object("3.o");
  LDSymbol *FooRef = ref(*File3, "foo");
  LDSymbol *FooV1Ref = ref(*File3, "foo@V1");

  normalizeSymbols();
  ASSERT_NE(FooWeakDefault, nullptr);
  ASSERT_NE(Foo, nullptr);
  ASSERT_NE(FooRef, nullptr);
  ASSERT_NE(FooV1Ref, nullptr);
  EXPECT_EQ(FooRef->resolveInfo()->outSymbol(), Foo);
  EXPECT_EQ(FooV1Ref->resolveInfo()->outSymbol(), Foo);
  EXPECT_EQ(FooWeakDefault->resolveInfo()->outSymbol(), Foo);
  EXPECT_EQ(FooRef->resolveInfo(), Foo->resolveInfo());
  EXPECT_EQ(FooV1Ref->resolveInfo(), FooWeakDefault->resolveInfo());
}

TEST_F(VersionedSymbolsSymbolResolutionTest,
       StrongDefaultBeatsWeakPlainAndWeakNonDefault) {
  InputFile *File1 = object("1.o");
  LDSymbol *FooDefault = define(*File1, "foo@@V1", ResolveInfo::Binding::Global,
                                /*Value=*/0x10);

  InputFile *File2 = object("2.o");
  LDSymbol *FooWeakV1 = define(*File2, "foo@V1", ResolveInfo::Binding::Weak,
                               /*Value=*/0x20);

  InputFile *File3 = object("3.o");
  LDSymbol *FooWeak = define(*File3, "foo", ResolveInfo::Binding::Weak,
                             /*Value=*/0x30);

  InputFile *File4 = object("4.o");
  LDSymbol *FooRef = ref(*File4, "foo");
  LDSymbol *FooV1Ref = ref(*File4, "foo@V1");

  normalizeSymbols();
  ASSERT_NE(FooDefault, nullptr);
  ASSERT_NE(FooWeakV1, nullptr);
  ASSERT_NE(FooWeak, nullptr);
  ASSERT_NE(FooRef, nullptr);
  ASSERT_NE(FooV1Ref, nullptr);
  EXPECT_EQ(FooRef->resolveInfo()->outSymbol(), FooDefault);
  EXPECT_EQ(FooV1Ref->resolveInfo()->outSymbol(), FooDefault);
  EXPECT_EQ(FooWeakV1->resolveInfo()->outSymbol(), FooDefault);
  EXPECT_EQ(FooWeak->resolveInfo()->outSymbol(), FooDefault);
  EXPECT_EQ(FooWeakV1->resolveInfo(), FooDefault->resolveInfo());
  EXPECT_EQ(FooWeak->resolveInfo(), FooRef->resolveInfo());
  EXPECT_EQ(FooV1Ref->resolveInfo(), FooDefault->resolveInfo());
}

TEST_F(VersionedSymbolsSymbolResolutionTest,
       StrongDefaultKeepsVersionedSlotOverStrongNonDefault) {
  InputFile *File1 = object("1.o");
  LDSymbol *FooDefault = define(*File1, "foo@@V1", ResolveInfo::Binding::Global,
                                /*Value=*/0x10);

  InputFile *File2 = object("2.o");
  m_DiagEngine->setInfoMap(std::make_unique<DiagnosticInfos>(*m_Config));
  testing::internal::CaptureStderr();
  LDSymbol *FooV1 = define(*File2, "foo@V1", ResolveInfo::Binding::Global,
                           /*Value=*/0x20);
  std::string Output = testing::internal::GetCapturedStderr();

  normalizeSymbols();
  ASSERT_NE(FooDefault, nullptr);
  ASSERT_NE(FooV1, nullptr);
  EXPECT_NE(Output.find("Error: multiple definition"), std::string::npos);
}

TEST_F(VersionedSymbolsSymbolResolutionTest,
       TwoStrongDefaultsKeepSeparateVersionedSlots) {
  InputFile *File1 = object("1.o");
  LDSymbol *FooDefaultV1 =
      define(*File1, "foo@@V1", ResolveInfo::Binding::Global,
             /*Value=*/0x10);

  InputFile *File2 = object("2.o");
  m_DiagEngine->setInfoMap(std::make_unique<DiagnosticInfos>(*m_Config));
  testing::internal::CaptureStderr();
  LDSymbol *FooDefaultV2 =
      define(*File2, "foo@@V2", ResolveInfo::Binding::Global,
             /*Value=*/0x20);

  normalizeSymbols();
  std::string Output = testing::internal::GetCapturedStderr();
  ASSERT_NE(FooDefaultV1, nullptr);
  ASSERT_NE(FooDefaultV2, nullptr);
  ASSERT_NE(findInfo("foo"), nullptr);
  EXPECT_NE(Output.find("Error: multiple definition of symbol `foo' in file "
                        "1.o and 2.o\n"),
            std::string::npos);
  EXPECT_NE(Output.find("Error: multiple definition of symbol `foo@V2' in "
                        "file 2.o and 1.o\n"),
            std::string::npos);
  EXPECT_EQ(findInfo("foo")->resolveInfo()->value(),
            static_cast<uint64_t>(0x20));
  EXPECT_EQ(findInfo("foo@V1"), FooDefaultV1);
  EXPECT_EQ(findInfo("foo@V2"), FooDefaultV2);
  EXPECT_TRUE(FooDefaultV1->resolveInfo()->isDefaultVersion());
  EXPECT_TRUE(FooDefaultV2->resolveInfo()->isDefaultVersion());
}

TEST_F(VersionedSymbolsSymbolResolutionTest,
       StrongPlainAndStrongNonDefaultBeatWeakDefaultSlots) {
  InputFile *File1 = object("1.o");
  LDSymbol *FooWeakDefault =
      define(*File1, "foo@@V1", ResolveInfo::Binding::Weak, /*Value=*/0x10);

  InputFile *File2 = object("2.o");
  LDSymbol *FooV1 = define(*File2, "foo@V1", ResolveInfo::Binding::Global,
                           /*Value=*/0x20);

  InputFile *File3 = object("3.o");
  LDSymbol *Foo = define(*File3, "foo", ResolveInfo::Binding::Global,
                         /*Value=*/0x30);

  m_DiagEngine->setInfoMap(std::make_unique<DiagnosticInfos>(*m_Config));
  testing::internal::CaptureStderr();
  normalizeSymbols();
  std::string Output = testing::internal::GetCapturedStderr();
  ASSERT_NE(FooWeakDefault, nullptr);
  ASSERT_NE(FooV1, nullptr);
  ASSERT_NE(Foo, nullptr);
  EXPECT_NE(Output.find("Error: multiple definition of symbol `foo@V1' in "
                        "file 2.o and 3.o\n"),
            std::string::npos);
  EXPECT_EQ(findInfo("foo@V1"), FooV1);
  EXPECT_EQ(findInfo("foo"), Foo);
  EXPECT_TRUE(FooV1->resolveInfo()->isDefaultVersion());
}

TEST_F(VersionedSymbolsSymbolResolutionTest,
       StrongNonDefaultBeatsWeakDefaultAndWeakPlainForVersionedReference) {
  InputFile *File1 = object("1.o");
  LDSymbol *FooWeakDefault =
      define(*File1, "foo@@V1", ResolveInfo::Binding::Weak, /*Value=*/0x10);

  InputFile *File2 = object("2.o");
  LDSymbol *FooWeak = define(*File2, "foo", ResolveInfo::Binding::Weak,
                             /*Value=*/0x20);

  InputFile *File3 = object("3.o");
  LDSymbol *FooV1 = define(*File3, "foo@V1", ResolveInfo::Binding::Global,
                           /*Value=*/0x30);

  InputFile *File4 = object("4.o");
  LDSymbol *FooRef = ref(*File4, "foo");
  LDSymbol *FooV1Ref = ref(*File4, "foo@V1");

  normalizeSymbols();
  ASSERT_NE(FooWeakDefault, nullptr);
  ASSERT_NE(FooWeak, nullptr);
  ASSERT_NE(FooV1, nullptr);
  ASSERT_NE(FooRef, nullptr);
  ASSERT_NE(FooV1Ref, nullptr);
  EXPECT_EQ(FooRef->resolveInfo()->outSymbol(), FooV1);
  EXPECT_EQ(FooV1Ref->resolveInfo()->outSymbol(), FooV1);
  EXPECT_EQ(FooWeak->resolveInfo()->outSymbol(), FooV1);
  EXPECT_EQ(FooWeakDefault->resolveInfo()->outSymbol(), FooV1);
  EXPECT_TRUE(FooV1->resolveInfo()->isDefaultVersion());
}

#else
TEST_F(SymbolResolutionTest, VersionedSymbolsRequireSymbolVersioning) {
  GTEST_SKIP() << "ELD_ENABLE_SYMBOL_VERSIONING is disabled";
}
#endif
