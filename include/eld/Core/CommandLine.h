//===- CommandLine.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_CORE_COMMANDLINE_H
#define ELD_CORE_COMMANDLINE_H

#include <vector>

namespace eld {

struct CommandLine {
public:
  typedef enum { Flag, Option, MultiValueOption } CmdType;

  CommandLine(CmdType Type) : T(Type) {}

  CmdType getType() const { return T; }

private:
  CmdType T;
};

struct Flags : public CommandLine {
  Flags(const std::string &Opt, bool Flag)
      : CommandLine(CmdType::Flag), Option(Opt), Flag(Flag) {}

  bool getFlag() const { return Flag; }
  std::string getOption() const { return Option; }

  static bool classof(const CommandLine *Cmd) {
    return Cmd->getType() == CmdType::Flag;
  }

private:
  std::string Option;
  bool Flag;
};

struct Options : public CommandLine {
  Options(const std::string &Opt, const std::string &Arg)
      : CommandLine(CmdType::Option), Option(Opt), Argument(Arg) {}

  std::string getOption() const { return Option; }
  std::string getArgument() const { return Argument; }

  static bool classof(const CommandLine *Cmd) {
    return Cmd->getType() == CmdType::Option;
  }

private:
  std::string Option;
  std::string Argument;
};

struct MultiValueOption : public CommandLine {
  MultiValueOption(const std::string &Opt, const std::vector<std::string> &Arg)
      : CommandLine(CmdType::MultiValueOption), Option(Opt), ArgumentList(Arg) {
  }

  std::string getOption() const { return Option; }
  std::vector<std::string> getArgumentList() const { return ArgumentList; }

  static bool classof(const CommandLine *Cmd) {
    return Cmd->getType() == CmdType::MultiValueOption;
  }

private:
  std::string Option;
  std::vector<std::string> ArgumentList;
};

} // namespace eld

#endif
