#include "eld/Config/GeneralOptions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/ArchiveFile.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/InputBuilder.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Object/SectionMap.h"
#include "eld/Readers/CommonELFSection.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Script/ExcludeFiles.h"
#include "eld/Script/InputSectDesc.h"
#include "eld/Script/OutputSectDesc.h"
#include "eld/Script/ScriptFile.h"
#include "eld/Script/SectionsCmd.h"
#include "eld/Script/StringList.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/ScriptParser/ScriptParser.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MemoryArea.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"
#include <array>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

using namespace eld;

namespace {

enum class InputKind { NonArchive, Archive, ThinArchive, Count };

static_assert(static_cast<size_t>(InputKind::Count) == 3,
              "Update matching expectations for changes to input kinds");

struct RuleMatchingExpectations {
  bool NonArchive;
  bool Archive;
  bool ThinArchive;

  bool forInputKind(InputKind Kind) const {
    switch (Kind) {
    case InputKind::NonArchive:
      return NonArchive;
    case InputKind::Archive:
      return Archive;
    case InputKind::ThinArchive:
      return ThinArchive;
    case InputKind::Count:
      break;
    }
    llvm_unreachable("Invalid input kind for matching expectations");
  }
};

// Named outcomes avoid positional boolean triples in the case table.
constexpr RuleMatchingExpectations AllInputs{true, true, true};
constexpr RuleMatchingExpectations NoInputs{false, false, false};
constexpr RuleMatchingExpectations NonArchiveOnly{true, false, false};
constexpr RuleMatchingExpectations ArchivesOnly{false, true, true};
constexpr RuleMatchingExpectations RegularArchiveOnly{false, true, false};
constexpr RuleMatchingExpectations ThinArchiveOnly{false, false, true};
constexpr RuleMatchingExpectations NonArchiveAndThinArchive{true, false, true};
constexpr RuleMatchingExpectations NonArchiveAndRegularArchive{true, true,
                                                               false};

struct RuleMatchingCase {
  const char *Name;
  const char *Rule;
  const char *SectionName;
  RuleMatchingExpectations Matches;
  // Set for CommonELFSection fixtures; omit for ordinary ELF sections.
  bool Common = false;
};

// Each case runs against non-archive dir/file.o and both forms of dir/lib.a:
// a regular archive member file.o and a thin archive member members/file.o.
// Entry fields: test name, linker-script rule, section name, matching inputs,
// and an optional common-section flag. Keep related cases together below.
const RuleMatchingCase MatchingCases[] = {
    // Section globs: names, wildcard operators, case sensitivity, and lists.
    {"AllFilesAndSections", "*(*)", ".text.foo", AllInputs},
    {"ExactSection", "*(.text.foo)", ".text.foo", AllInputs},
    {"SectionPrefix", "*(.text*)", ".text.foo", AllInputs},
    {"SectionSuffix", "*(*foo)", ".text.foo", AllInputs},
    {"SectionInfix", "*(*text*)", ".text.foo", AllInputs},
    {"SectionQuestionMark", "*(.text.fo?)", ".text.foo", AllInputs},
    {"SectionCharacterClass", "*(.text.f[op]o)", ".text.foo", AllInputs},
    {"SectionNegatedClass", "*(.text.f[!a]o)", ".text.foo", AllInputs},
    {"SectionMismatch", "*(.data*)", ".text.foo", NoInputs},
    {"SectionCaseSensitive", "*(.TEXT*)", ".text.foo", NoInputs},
    {"MultipleSections", "*(.data* .text*)", ".text.foo", AllInputs},
    {"MultipleSectionsMismatch", "*(.data* .bss*)", ".text.foo", NoInputs},

    // GNU file patterns match full non-archive paths; archive selection
    // requires
    // an archive-qualified rule (apart from the special all-files wildcard).
    {"ExactFilePath", "dir/file.o(.text*)", ".text.foo", NonArchiveOnly},
    {"FileBasename", "file.o(.text*)", ".text.foo", NoInputs},
    {"FileSuffix", "*file.o(.text*)", ".text.foo", NonArchiveOnly},
    {"FileDirectory", "dir/*(.text*)", ".text.foo", NonArchiveOnly},
    {"ArchiveAsFile", "*lib.a(.text*)", ".text.foo", NoInputs},
    {"FileMismatch", "*missing.o(.text*)", ".text.foo", NoInputs},

    // A leading ':' explicitly selects non-archive inputs, including quoted
    // patterns and rules that omit the section list.
    {"NonArchiveExactPath", ":dir/file.o(.text*)", ".text.foo", NonArchiveOnly},
    {"NonArchiveFileSuffix", ":*file.o(.text*)", ".text.foo", NonArchiveOnly},
    {"NonArchiveAllFiles", ":*(.text*)", ".text.foo", NonArchiveOnly},
    {"NonArchiveDirectory", ":dir/*(.text*)", ".text.foo", NonArchiveOnly},
    {"NonArchiveQuestionMark", ":dir/fil?.o(.text*)", ".text.foo",
     NonArchiveOnly},
    {"NonArchiveCharacterClass", ":dir/fil[ae].o(.text*)", ".text.foo",
     NonArchiveOnly},
    {"NonArchiveBasenameMismatch", ":file.o(.text*)", ".text.foo", NoInputs},
    {"NonArchiveFileMismatch", ":*missing.o(.text*)", ".text.foo", NoInputs},
    {"NonArchiveSectionMismatch", ":*file.o(.data*)", ".text.foo", NoInputs},
    {"NonArchiveArchiveNameMismatch", ":*lib.a(.text*)", ".text.foo", NoInputs},
    {"NonArchiveQuotedPath", "\":dir/file.o\"(.text*)", ".text.foo",
     NonArchiveOnly},
    {"NonArchiveWithoutSections", ":dir/file.o", ".data.foo", NonArchiveOnly},
    {"NonArchiveWildcardWithoutSections", ":*.o", ".bss.foo", NonArchiveOnly},

    // Archive-qualified patterns distinguish regular member basenames from thin
    // member paths; a member suffix wildcard matches both archive kinds.
    {"WholeArchive", "dir/lib.a:(.text*)", ".text.foo", ArchivesOnly},
    {"ArchiveWildcard", "*lib.a:(.text*)", ".text.foo", ArchivesOnly},
    {"AllArchives", "*:*(.text*)", ".text.foo", ArchivesOnly},
    {"ExactArchiveMember", "dir/lib.a:file.o(.text*)", ".text.foo",
     RegularArchiveOnly},
    {"ArchiveMemberSuffix", "dir/lib.a:*file.o(.text*)", ".text.foo",
     ArchivesOnly},
    {"ThinArchiveMemberPath", "dir/lib.a:members/file.o(.text*)", ".text.foo",
     ThinArchiveOnly},
    {"ArchiveBasenameMismatch", "lib.a:*file.o(.text*)", ".text.foo", NoInputs},
    {"ArchiveNameMismatch", "missing.a:*file.o(.text*)", ".text.foo", NoInputs},
    {"ArchiveMemberMismatch", "dir/lib.a:missing.o(.text*)", ".text.foo",
     NoInputs},
    {"ArchiveSectionMismatch", "dir/lib.a:*(.data*)", ".text.foo", NoInputs},

    // Unqualified file exclusions can remove non-archive files and members.
    {"ExcludeAllMatchingFiles", "*(EXCLUDE_FILE(*file.o) .text*)", ".text.foo",
     NoInputs},

    // Leading-colon exclusions remove only non-archive inputs. Quoted and
    // unquoted spellings must have the same result.
    {"ExcludeNonArchiveWildcard", "*(EXCLUDE_FILE(:*file.o) .text*)",
     ".text.foo", ArchivesOnly},
    {"ExcludeNonArchiveExactPath", "*(EXCLUDE_FILE(:dir/file.o) .text*)",
     ".text.foo", ArchivesOnly},
    {"ExcludeNonArchiveQuotedExactPath",
     "*(EXCLUDE_FILE(\":dir/file.o\") .text*)", ".text.foo", ArchivesOnly},
    {"ExcludeNonArchiveQuotedWildcard", "*(EXCLUDE_FILE(\":*file.o\") .text*)",
     ".text.foo", ArchivesOnly},
    {"OutsideExcludeNonArchiveQuotedExactPath",
     "EXCLUDE_FILE(\":dir/file.o\") *(.text*)", ".text.foo", ArchivesOnly},
    {"ExcludeNonArchiveAllFiles", "*(EXCLUDE_FILE(:*) .text*)", ".text.foo",
     ArchivesOnly},
    {"ExcludeNonArchiveMismatch", "*(EXCLUDE_FILE(:missing.o) .text*)",
     ".text.foo", AllInputs},
    {"ExcludeNonArchiveArchiveName", "*(EXCLUDE_FILE(:dir/lib.a) .text*)",
     ".text.foo", AllInputs},

    // Archive-qualified exclusions must preserve non-archive inputs. In
    // particular, member patterns must not accidentally match the archive name.
    {"ExcludeWholeArchive", "*(EXCLUDE_FILE(dir/lib.a:) .text*)", ".text.foo",
     NonArchiveOnly},
    {"ExcludeExactArchiveMember", "*(EXCLUDE_FILE(dir/lib.a:file.o) .text*)",
     ".text.foo", NonArchiveAndThinArchive},
    {"ExcludeArchiveMemberWildcard",
     "*(EXCLUDE_FILE(dir/lib.a:*file.o) .text*)", ".text.foo", NonArchiveOnly},
    {"ExcludeArchiveAllMembers", "*(EXCLUDE_FILE(dir/lib.a:*) .text*)",
     ".text.foo", NonArchiveOnly},
    {"ExcludeArchiveNameMismatch", "*(EXCLUDE_FILE(other.a:*) .text*)",
     ".text.foo", AllInputs},
    {"ExcludeArchiveMemberMustNotMatchArchiveName",
     "*(EXCLUDE_FILE(dir/lib.a:*lib.a) .text*)", ".text.foo", AllInputs},
    {"OutsideExcludeArchiveMemberWildcard",
     "EXCLUDE_FILE(dir/lib.a:*file.o) *(.text*)", ".text.foo", NonArchiveOnly},
    {"ExcludeThinArchiveMemberPath",
     "*(EXCLUDE_FILE(dir/lib.a:members/file.o) .text*)", ".text.foo",
     NonArchiveAndRegularArchive},
    {"ExcludeArchiveAsFile", "*(EXCLUDE_FILE(*lib.a) .text*)", ".text.foo",
     NonArchiveOnly},

    // Exclusion scope: outside EXCLUDE_FILE applies to all section patterns;
    // inside EXCLUDE_FILE applies only to the following section pattern.
    {"ExcludeMultiplePatterns", "*(EXCLUDE_FILE(:*file.o dir/lib.a:*) .text*)",
     ".text.foo", NoInputs},
    {"OutsideExcludeNonArchive", "EXCLUDE_FILE(:*file.o) *(.text*)",
     ".text.foo", ArchivesOnly},
    {"OutsideExcludeAllSections", "EXCLUDE_FILE(:*file.o) *(.data* .text*)",
     ".text.foo", ArchivesOnly},
    {"InsideExcludeOnlyNextPattern", "*(EXCLUDE_FILE(:*file.o) .data* .text*)",
     ".text.foo", AllInputs},
    {"InsideExcludeLaterSection", "*(EXCLUDE_FILE(:*file.o) .text* .data*)",
     ".data.foo", AllInputs},
    {"MultipleExcludesText",
     "*(EXCLUDE_FILE(:*file.o) .text* EXCLUDE_FILE(dir/lib.a:) .data*)",
     ".text.foo", ArchivesOnly},
    {"MultipleExcludesData",
     "*(EXCLUDE_FILE(:*file.o) .text* EXCLUDE_FILE(dir/lib.a:) .data*)",
     ".data.foo", NonArchiveOnly},
    {"CombinedInsideOutsideExclude",
     "EXCLUDE_FILE(dir/lib.a:) *(EXCLUDE_FILE(:*file.o) .text*)", ".text.foo",
     NoInputs},

    // KEEP and SORT wrappers preserve file selection and exclusion semantics.
    {"KeepNonArchive", "KEEP(:*file.o(.text*))", ".text.foo", NonArchiveOnly},
    {"SortByName", "*(SORT_BY_NAME(.text*))", ".text.foo", AllInputs},
    {"SortByAlignment", "*(SORT_BY_ALIGNMENT(.text*))", ".text.foo", AllInputs},
    {"SortByInitPriority", "*(SORT_BY_INIT_PRIORITY(.init_array.*))",
     ".init_array.100", AllInputs},
    {"SortNone", "*(SORT_NONE(.text*))", ".text.foo", AllInputs},
    {"NestedSort", "*(SORT_BY_NAME(SORT_BY_ALIGNMENT(.text*)))", ".text.foo",
     AllInputs},
    {"ExcludeInsideSort", "*(SORT_BY_NAME(EXCLUDE_FILE(:*file.o) .text*))",
     ".text.foo", ArchivesOnly},
    {"ExcludeInsideNestedSort",
     "*(SORT_BY_NAME(SORT_BY_ALIGNMENT(EXCLUDE_FILE(:*file.o) .text*)))",
     ".text.foo", ArchivesOnly},

    // COMMON and small-common shorthand require a CommonELFSection fixture.
    // CommonRequiresCommonSection deliberately uses an ordinary ELF section.
    {"CommonSyntacticSugar", "*(COMMON)", "COMMON.symbol", AllInputs, true},
    {"CommonRequiresCommonSection", "*(COMMON)", "COMMON.symbol", NoInputs},
    {"CommonExactSymbol", "*(COMMON.symbol)", "COMMON.symbol", AllInputs, true},
    {"SmallCommonOne", "*(.scommon.1)", ".scommon.1.symbol", AllInputs, true},
    {"SmallCommonTwo", "*(.scommon.2)", ".scommon.2.symbol", AllInputs, true},
    {"SmallCommonFour", "*(.scommon.4)", ".scommon.4.symbol", AllInputs, true},
    {"SmallCommonEight", "*(.scommon.8)", ".scommon.8.symbol", AllInputs, true},
    {"SmallCommonMismatch", "*(.scommon.2)", ".scommon.1.symbol", NoInputs,
     true},
    {"NonArchiveCommon", ":*(COMMON)", "COMMON.symbol", NonArchiveOnly, true},
    {"ExcludeNonArchiveCommon", "*(EXCLUDE_FILE(:*) COMMON)", "COMMON.symbol",
     ArchivesOnly, true},
};

class RuleMatchingTest : public ::testing::Test {
protected:
  void SetUp() override {
    Diag = std::make_unique<DiagnosticEngine>(false);
    Config = std::make_unique<LinkerConfig>(Diag.get());
    Config->options().disableThreads();
    Config->targets().setTriple("x86_64-unknown-linux-gnu");
    Config->targets().setBitClass(64);
    Config->targets().setEndian(TargetOptions::Little);
    Script = std::make_unique<LinkerScript>(Diag.get());
    Mod = std::make_unique<Module>(*Script, *Config, nullptr);
    Builder = std::make_unique<InputBuilder>(*Config);
    ASSERT_TRUE(Mod->createInternalInputs());
  }

  Input *createInput(const std::string &Path) {
    auto *Result = make<Input>(Path, Diag.get());
    Result->setResolvedPath(Path);
    Result->setResolvedPathHash(Input::computeFilePathHash(Path));
    Result->setMemberNameHash(Input::computeFilePathHash(Path));
    Result->setMemArea(
        make<MemoryArea>(llvm::MemoryBuffer::getMemBufferCopy("", Path)));
    return Result;
  }

  ELFObjectFile *createObject(InputKind Kind,
                              const std::string &Path = "dir/file.o",
                              const std::string &ArchivePath = "dir/lib.a",
                              const std::string &MemberName = "") {
    Input *ObjectInput = nullptr;
    if (Kind == InputKind::NonArchive)
      ObjectInput = createInput(Path);
    else {
      auto *Archive = make<ArchiveFile>(createInput(ArchivePath), Diag.get());
      if (Kind == InputKind::ThinArchive)
        Archive->setThinArchive();
      std::string Name =
          MemberName.empty()
              ? (Kind == InputKind::ThinArchive ? "members/file.o" : "file.o")
              : MemberName;
      ObjectInput = make<ArchiveMemberInput>(
          Diag.get(), Archive, Name,
          make<MemoryArea>(llvm::MemoryBuffer::getMemBufferCopy("")), 0);
    }
    auto *Object = make<ELFObjectFile>(ObjectInput, Diag.get());
    ObjectInput->setInputFile(Object);
    return Object;
  }

  ELFSection *createSection(InputFile *Object, const std::string &Name,
                            bool Common = false) {
    ELFSection *Section =
        Common ? make<CommonELFSection>(Name, Object, 4)
               : Script->sectionMap().createELFSection(
                     Name, LinkerSectionKind::Regular, llvm::ELF::SHT_PROGBITS,
                     llvm::ELF::SHF_ALLOC, 0);
    Section->setInputFile(Object);
    return Section;
  }

  std::vector<RuleContainer *> parseScript(const std::string &Text) {
    Input *ScriptInput = createInput("rules.t");
    ScriptInput->setMemArea(make<MemoryArea>(
        llvm::MemoryBuffer::getMemBufferCopy(Text, "rules.t")));
    auto *InputFile = make<LinkerScriptFile>(ScriptInput, Diag.get());
    auto *File =
        make<ScriptFile>(ScriptFile::LDScript, *Mod, *InputFile, *Builder);
    eld::v2::ScriptParser Parser(*Config, *File);
    Parser.readLinkerScript();
    EXPECT_EQ(Config->getPrinter()->getNumErrors(), 0u) << Text;
    EXPECT_EQ(Config->getPrinter()->getNumFatalErrors(), 0u) << Text;
    std::vector<RuleContainer *> Rules;
    for (auto *Command : *File) {
      auto *Sections = llvm::dyn_cast<SectionsCmd>(Command);
      if (!Sections)
        continue;
      for (auto *SectionCommand : Sections->commands()) {
        auto *Output = llvm::dyn_cast<OutputSectDesc>(SectionCommand);
        if (!Output)
          continue;
        for (auto *OutputCommand : Output->commands()) {
          auto *Description = llvm::dyn_cast<InputSectDesc>(OutputCommand);
          if (Description && !Description->isSpecial())
            Rules.push_back(Script->sectionMap()
                                .insert(*Description, *Output)
                                .first.second);
        }
      }
    }
    return Rules;
  }

  std::vector<RuleContainer *> parseRules(const std::string &Rules) {
    return parseScript("SECTIONS { .output : { " + Rules + " } }");
  }

  bool matches(const RuleContainer &Rule, const ELFSection &Section) {
    InputFile *Object = Section.getInputFile();
    const Input *ObjectInput = Object->getInput();
    return Script->sectionMap().matched(
        Rule, Object, ObjectInput->getResolvedPath().native(),
        Section.name().str(), llvm::isa<ArchiveMemberInput>(ObjectInput),
        ObjectInput->getName(), Section.sectionNameHash(),
        ObjectInput->getResolvedPathHash(),
        ObjectInput->getArchiveMemberNameHash(),
        llvm::isa<CommonELFSection>(&Section));
  }

  std::unique_ptr<DiagnosticEngine> Diag;
  std::unique_ptr<LinkerConfig> Config;
  std::unique_ptr<LinkerScript> Script;
  std::unique_ptr<Module> Mod;
  std::unique_ptr<InputBuilder> Builder;
};

using MatchingParam = std::tuple<RuleMatchingCase, InputKind>;

class ParameterizedRuleMatchingTest
    : public RuleMatchingTest,
      public ::testing::WithParamInterface<MatchingParam> {};

TEST_P(ParameterizedRuleMatchingTest, ParsedRuleMatchesInputSection) {
  const auto &[TestCase, Kind] = GetParam();
  SCOPED_TRACE(TestCase.Rule);
  SCOPED_TRACE(TestCase.SectionName);
  auto Rules = parseRules(TestCase.Rule);
  ASSERT_EQ(Rules.size(), 1u);
  auto *Section =
      createSection(createObject(Kind), TestCase.SectionName, TestCase.Common);
  const bool Expected = TestCase.Matches.forInputKind(Kind);
  EXPECT_EQ(matches(*Rules.front(), *Section), Expected);
  // Both entry points use GNU matching; this flag only bypasses section-name
  // remapping, so the expected matching result is unchanged.
  for (bool DoNotUseRuleMatchingName : {false, true}) {
    SCOPED_TRACE(DoNotUseRuleMatchingName);
    EXPECT_EQ(Script->sectionMap().doesRuleMatchWithSection(
                  *Rules.front(), *Section, DoNotUseRuleMatchingName),
              Expected);
  }
}

std::string
matchingParameterName(const ::testing::TestParamInfo<MatchingParam> &Info) {
  const auto &[TestCase, Kind] = Info.param;
  const char *KindName = Kind == InputKind::NonArchive ? "NonArchive"
                         : Kind == InputKind::Archive  ? "Archive"
                                                       : "ThinArchive";
  return std::string(TestCase.Name) + "_" + KindName + "_GNU";
}

INSTANTIATE_TEST_SUITE_P(
    FileSectionAndExclusionPatterns, ParameterizedRuleMatchingTest,
    ::testing::Combine(::testing::ValuesIn(MatchingCases),
                       ::testing::Values(InputKind::NonArchive,
                                         InputKind::Archive,
                                         InputKind::ThinArchive)),
    matchingParameterName);

TEST_F(RuleMatchingTest, HashAndGlobMatchingAgree) {
  struct WildcardCase {
    const char *Pattern;
    const char *Name;
    bool Matches;
  };
  const WildcardCase Cases[] = {
      {"file.o", "file.o", true},
      {"file.o", "FILE.o", false},
      {"file.o", "dir/file.o", false},
      {"*.o", "dir/file.o", true},
      {"*.o", "file.o.bak", false},
      {"dir/*.o", "dir/sub/file.o", true},
      {"dir/*", "other/file.o", false},
      {"file?.o", "file1.o", true},
      {"file?.o", "file12.o", false},
      {"file[0-2].o", "file1.o", true},
      {"file[0-2].o", "file3.o", false},
      {"file[!0-2].o", "file3.o", true},
      {"file[!0-2].o", "file1.o", false},
      {R"(file\*.o)", "file*.o", true},
      {R"(file\?.o)", "file?.o", true},
      {R"(file\?.o)", "file1.o", false},
      {R"(\)", R"(\)", false},
      {"*", "", false},
  };
  auto &Map = Script->sectionMap();
  for (const auto &TestCase : Cases) {
    SCOPED_TRACE(TestCase.Pattern);
    SCOPED_TRACE(TestCase.Name);
    WildcardPattern Pattern(TestCase.Pattern);
    EXPECT_EQ(Pattern.matched(TestCase.Name), TestCase.Matches);
    EXPECT_EQ(Map.matched(Pattern, TestCase.Name,
                          Input::computeFilePathHash(TestCase.Name)),
              TestCase.Matches);
  }
}

TEST_F(RuleMatchingTest,
       ArchiveQualifiedWildcardDoesNotExcludeNonArchiveFiles) {
  auto Rules = parseRules("*(EXCLUDE_FILE(dir/lib.a:*file.o) .text*)");
  ASSERT_EQ(Rules.size(), 1u);
  auto *Section =
      createSection(createObject(InputKind::NonArchive), ".text.foo");
  EXPECT_TRUE(matches(*Rules.front(), *Section));
  EXPECT_TRUE(Script->sectionMap().doesRuleMatchWithSection(*Rules.front(),
                                                            *Section, false));
}

TEST_F(RuleMatchingTest, ThinArchiveCompatibilityUsesMemberBasename) {
  const std::array<const char *, 4> Patterns = {
      "dir/lib.a:file.o(.text*)", "dir/lib.a:*file.o(.text*)",
      "dir/lib.a:members/file.o(.text*)", "dir/lib.a:missing.o(.text*)"};
  const std::array<bool, 4> DefaultMatches = {false, true, true, false};
  const std::array<bool, 4> CompatibleMatches = {true, true, false, false};
  auto *Section =
      createSection(createObject(InputKind::ThinArchive), ".text.foo");
  std::vector<RuleContainer *> Rules;
  for (size_t Index = 0; Index < Patterns.size(); ++Index) {
    auto Parsed = parseRules(Patterns[Index]);
    ASSERT_EQ(Parsed.size(), 1u);
    Rules.push_back(Parsed.front());
    EXPECT_EQ(matches(*Rules.back(), *Section), DefaultMatches[Index])
        << Patterns[Index];
  }
  Config->options().setThinArchiveRuleMatchingCompatibility();
  for (size_t Index = 0; Index < Patterns.size(); ++Index)
    EXPECT_EQ(matches(*Rules[Index], *Section), CompatibleMatches[Index])
        << Patterns[Index];
}

TEST_F(RuleMatchingTest, ThinArchiveCompatibilityStillRejectsNonArchiveRules) {
  Config->options().setThinArchiveRuleMatchingCompatibility();
  auto Rules = parseRules(":*(.text*) :*file.o(.text*)");
  ASSERT_EQ(Rules.size(), 2u);
  auto *Section =
      createSection(createObject(InputKind::ThinArchive), ".text.foo");
  for (auto *Rule : Rules)
    EXPECT_FALSE(matches(*Rule, *Section));
}

TEST_F(RuleMatchingTest, ThinArchiveCompatibilityDoesNotRewriteExclusions) {
  Config->options().setThinArchiveRuleMatchingCompatibility();
  auto Rules = parseRules("*(EXCLUDE_FILE(dir/lib.a:file.o) .text*) "
                          "*(EXCLUDE_FILE(dir/lib.a:members/file.o) .text*)");
  ASSERT_EQ(Rules.size(), 2u);
  auto *Section =
      createSection(createObject(InputKind::ThinArchive), ".text.foo");
  EXPECT_TRUE(matches(*Rules[0], *Section));
  EXPECT_FALSE(matches(*Rules[1], *Section));
}

TEST_F(RuleMatchingTest, FirstMatchingOutputSectionWins) {
  auto Rules = parseScript("SECTIONS { .first : { :*(.text*) } "
                           ".second : { *(.text*) } }");
  ASSERT_EQ(Rules.size(), 2u);
  auto &Map = Script->sectionMap();
  for (InputKind Kind :
       {InputKind::NonArchive, InputKind::Archive, InputKind::ThinArchive}) {
    auto *Section = createSection(createObject(Kind), ".text.foo");
    Input *ObjectInput = Section->getInputFile()->getInput();
    auto Result = Map.findIn(
        Map.begin(), ObjectInput->getResolvedPath().native(), *Section,
        Kind != InputKind::NonArchive, ObjectInput->getName(),
        Section->sectionNameHash(), ObjectInput->getResolvedPathHash(),
        ObjectInput->getArchiveMemberNameHash());
    auto *Expected = Rules[Kind == InputKind::NonArchive ? 0 : 1];
    ASSERT_NE(Result.first, nullptr);
    EXPECT_EQ(Result.second, Expected);
    EXPECT_EQ(Result.first, Expected->getSection()->getOutputSection());
  }
}

TEST_F(RuleMatchingTest, FirstMatchingRuleWithinOutputSectionWins) {
  auto Rules = parseRules(":*missing.o(.text*) :*(.text*) *(.text*)");
  ASSERT_EQ(Rules.size(), 3u);
  auto *Section =
      createSection(createObject(InputKind::NonArchive), ".text.foo");
  Input *ObjectInput = Section->getInputFile()->getInput();
  auto &Map = Script->sectionMap();
  auto Result = Map.findOnlyIn(
      Map.findIter(".output"), ObjectInput->getResolvedPath().native(),
      *Section, false, ObjectInput->getName(), Section->sectionNameHash(),
      ObjectInput->getResolvedPathHash(),
      ObjectInput->getArchiveMemberNameHash());
  EXPECT_EQ(Result.second, Rules[1]);
  EXPECT_EQ(Rules[0]->getNextRule(), Rules[1]);
  EXPECT_EQ(Rules[1]->getNextRule(), Rules[2]);
}

TEST_F(RuleMatchingTest, LookupDoesNotMatchUnlistedSections) {
  auto Rules = parseRules(":*(.text*)");
  ASSERT_EQ(Rules.size(), 1u);
  auto *Section =
      createSection(createObject(InputKind::NonArchive), ".data.foo");
  Input *ObjectInput = Section->getInputFile()->getInput();
  auto &Map = Script->sectionMap();
  auto Result =
      Map.findIn(Map.begin(), ObjectInput->getResolvedPath().native(), *Section,
                 false, ObjectInput->getName(), Section->sectionNameHash(),
                 ObjectInput->getResolvedPathHash(),
                 ObjectInput->getArchiveMemberNameHash());
  EXPECT_EQ(Result.first, nullptr);
  EXPECT_EQ(Result.second, nullptr);
}

TEST_F(RuleMatchingTest, KeepPolicyPreservesNonArchiveRestriction) {
  auto Rules = parseRules("KEEP(:*file.o(.text*))");
  ASSERT_EQ(Rules.size(), 1u);
  EXPECT_EQ(Rules.front()->policy(), InputSectDesc::Keep);
  EXPECT_TRUE(Rules.front()->spec().isNonArchiveFile());
  EXPECT_FALSE(Rules.front()->spec().isArchive());
  EXPECT_FALSE(Rules.front()->spec().hasArchiveMember());
  EXPECT_EQ(Rules.front()->spec().file().name(), "*file.o");
}

TEST_F(RuleMatchingTest, RuleSpecEqualityIncludesNonArchiveRestriction) {
  auto Rules = parseRules(":*(.text*)");
  ASSERT_EQ(Rules.size(), 1u);
  InputSectDesc::Spec Same = Rules.front()->spec();
  EXPECT_TRUE(Same == Rules.front()->spec());
  Same.InputIsNonArchiveFile = false;
  EXPECT_FALSE(Same == Rules.front()->spec());
}

TEST_F(RuleMatchingTest,
       ExclusionSpecDistinguishesNonArchiveAndArchiveMembers) {
  auto Rules = parseRules("*(EXCLUDE_FILE(:file.o lib.a:file.o lib.a: file.o) "
                          ".text*)");
  ASSERT_EQ(Rules.size(), 1u);
  auto *Pattern =
      llvm::cast<WildcardPattern>(Rules.front()->spec().sections().front());
  const ExcludeFiles *Exclusions = Pattern->excludeFiles();
  ASSERT_NE(Exclusions, nullptr);
  ASSERT_EQ(Exclusions->size(), 4u);
  auto Iterator = Exclusions->patterns().begin();
  EXPECT_TRUE((*Iterator)->isNonArchiveFile());
  EXPECT_FALSE((*Iterator)->isArchive());
  EXPECT_TRUE((*Iterator)->isFile());
  ++Iterator;
  EXPECT_FALSE((*Iterator)->isNonArchiveFile());
  EXPECT_TRUE((*Iterator)->isFileInArchive());
  ++Iterator;
  EXPECT_TRUE((*Iterator)->isArchive());
  EXPECT_FALSE((*Iterator)->isFile());
  ++Iterator;
  EXPECT_FALSE((*Iterator)->isArchive());
  EXPECT_FALSE((*Iterator)->isNonArchiveFile());
  EXPECT_TRUE((*Iterator)->isFile());
}

TEST_F(RuleMatchingTest, DumpPreservesFilePatternSyntax) {
  for (const char *Pattern :
       {":dir/file.o(.text*)", ":*(.text*)", "\":dir/file.o\"(.text*)",
        "dir/lib.a:*file.o(.text*)"}) {
    SCOPED_TRACE(Pattern);
    auto Rules = parseRules(Pattern);
    ASSERT_EQ(Rules.size(), 1u);
    std::string Text;
    llvm::raw_string_ostream Stream(Text);
    Rules.front()->desc()->dumpSpec(Stream);
    EXPECT_EQ(Text, Pattern);
    auto RoundTrip = parseRules(Text);
    ASSERT_EQ(RoundTrip.size(), 1u);
    EXPECT_EQ(RoundTrip.front()->spec().isNonArchiveFile(),
              Rules.front()->spec().isNonArchiveFile());
    EXPECT_EQ(RoundTrip.front()->spec().isArchive(),
              Rules.front()->spec().isArchive());
    EXPECT_EQ(RoundTrip.front()->spec().file().name(),
              Rules.front()->spec().file().name());
  }
}

TEST_F(RuleMatchingTest, DumpPreservesInsideAndOutsideNonArchiveExclusions) {
  for (const char *Pattern : {"*(EXCLUDE_FILE(:dir/file.o) .text*)",
                              "EXCLUDE_FILE(:dir/file.o) *(.text*)"}) {
    auto Rules = parseRules(Pattern);
    ASSERT_EQ(Rules.size(), 1u);
    std::string Text;
    llvm::raw_string_ostream Stream(Text);
    Rules.front()->desc()->dumpMap(Stream, false, false, false, false);
    EXPECT_NE(Text.find("EXCLUDE_FILE ( :dir/file.o )"), std::string::npos);
    auto RoundTrip = parseRules(Text);
    ASSERT_EQ(RoundTrip.size(), 1u);
    for (InputKind Kind :
         {InputKind::NonArchive, InputKind::Archive, InputKind::ThinArchive}) {
      auto *Section = createSection(createObject(Kind), ".text.foo");
      EXPECT_EQ(matches(*RoundTrip.front(), *Section),
                Kind != InputKind::NonArchive);
    }
  }
}

TEST_F(RuleMatchingTest, RuleMatchingSectionNameMapCanBeBypassed) {
  auto Rules = parseRules(":*(.text.remapped) :*(.text.original)");
  ASSERT_EQ(Rules.size(), 2u);
  auto *Object = createObject(InputKind::NonArchive);
  auto *Section = createSection(Object, ".text.original");
  Section->setIndex(7);
  Object->setRuleMatchingSectionNameMap({{7, ".text.remapped"}});
  auto &Map = Script->sectionMap();
  EXPECT_TRUE(Map.doesRuleMatchWithSection(*Rules[0], *Section, false));
  EXPECT_FALSE(Map.doesRuleMatchWithSection(*Rules[1], *Section, false));
  EXPECT_FALSE(Map.doesRuleMatchWithSection(*Rules[0], *Section, true));
  EXPECT_TRUE(Map.doesRuleMatchWithSection(*Rules[1], *Section, true));
}

TEST_F(RuleMatchingTest, RemappedSectionNameDoesNotBypassFileRestriction) {
  auto Rules = parseRules(":*(.text.remapped)");
  ASSERT_EQ(Rules.size(), 1u);
  for (InputKind Kind : {InputKind::Archive, InputKind::ThinArchive}) {
    auto *Object = createObject(Kind);
    auto *Section = createSection(Object, ".text.original");
    Section->setIndex(7);
    Object->setRuleMatchingSectionNameMap({{7, ".text.remapped"}});
    EXPECT_FALSE(Script->sectionMap().doesRuleMatchWithSection(
        *Rules.front(), *Section, false));
  }
}

} // namespace
