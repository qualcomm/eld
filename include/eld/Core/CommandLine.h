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

  CommandLine(CmdType type) : T(type) {}

  CmdType getType() const { return T; }

private:
  CmdType T;
};

struct Flags : public CommandLine {
  Flags(const std::string &opt, bool flag)
      : CommandLine(CmdType::Flag), Option(opt), Flag(flag) {}

  bool getFlag() const { return Flag; }
  std::string getOption() const { return Option; }

  static bool classof(const CommandLine *cmd) {
    return cmd->getType() == CmdType::Flag;
  }

private:
  std::string Option;
  bool Flag;
};

struct Options : public CommandLine {
  Options(const std::string &opt, const std::string &arg)
      : CommandLine(CmdType::Option), Option(opt), Argument(arg) {}

  std::string getOption() const { return Option; }
  std::string getArgument() const { return Argument; }

  static bool classof(const CommandLine *cmd) {
    return cmd->getType() == CmdType::Option;
  }

private:
  std::string Option;
  std::string Argument;
};

struct MultiValueOption : public CommandLine {
  MultiValueOption(const std::string &opt, const std::vector<std::string> &arg)
      : CommandLine(CmdType::MultiValueOption), Option(opt), ArgumentList(arg) {
  }

  std::string getOption() const { return Option; }
  std::vector<std::string> getArgumentList() const { return ArgumentList; }

  static bool classof(const CommandLine *cmd) {
    return cmd->getType() == CmdType::MultiValueOption;
  }

private:
  std::string Option;
  std::vector<std::string> ArgumentList;
};

} // namespace eld

#endif
