#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <iostream>

class GetRuleMatchingInputPlugin : public eld::plugin::LinkerPlugin {
public:
  GetRuleMatchingInputPlugin()
      : eld::plugin::LinkerPlugin("GetRuleMatchingInputPlugin") {}

  void VisitSections(eld::plugin::InputFile IF) override {
    if (endswith(IF.getFileName(), "2.o")) {
      // Remember 2.o; it will be used as the rule-matching input for a
      // section that actually belongs to 1.o.
      OtherInput = IF;
      return;
    }
    if (endswith(IF.getFileName(), "1.o")) {
      for (auto S : IF.getSections()) {
        if (S.getName() == ".text.foo")
          FooSect = S;
        else if (S.getName() == ".text.bar")
          BarSect = S;
      }
    }
  }

  void ActBeforeRuleMatching() override {
    auto expComSym = getLinker()->getSymbol("C");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), expComSym);

    eld::plugin::Symbol comSym = expComSym.value();

    // Override the rule-matching input of .text.bar (from 1.o) with 2.o.
    getLinker()->setRuleMatchingInput(BarSect, OtherInput);

    // .text.bar has an explicit rule-matching input, so we expect 2.o.
    std::cout << "BarSect rule-matching input: "
              << BarSect.getRuleMatchingInput().getFileName() << "\n";

    // .text.foo has no override, so it falls back to its current input (1.o).
    std::cout << "FooSect rule-matching input: "
              << FooSect.getRuleMatchingInput().getFileName() << "\n";

    // An empty section yields a null input file (empty file name).
    eld::plugin::Section EmptySect;
    std::cout << "Empty section rule-matching input: '"
              << EmptySect.getRuleMatchingInput().getFileName() << "'\n";

    // Common symbol rule-mathcing input
    eld::plugin::Section comSymSect = comSym.getChunk().getSection();
    std::cout << "Empty section rule-matching input: "
              << comSymSect.getRuleMatchingInput().getFileName() << "\n";
  }

private:
  bool endswith(const std::string &s, const std::string &suffix) {
    if (s.size() < suffix.size())
      return false;
    return s.substr(s.size() - suffix.size()) == suffix;
  }
  eld::plugin::Section FooSect;
  eld::plugin::Section BarSect;
  eld::plugin::InputFile OtherInput{nullptr};
};

ELD_REGISTER_PLUGIN(GetRuleMatchingInputPlugin)
