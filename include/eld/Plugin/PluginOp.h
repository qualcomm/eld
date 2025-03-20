//===- PluginOp.h----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGIN_PLUGINOP_H
#define ELD_PLUGIN_PLUGINOP_H

#include <string>

namespace eld::plugin {
class LinkerWrapper;
} // namespace eld::plugin

namespace eld {
class Fragment;
class ELFSection;
class Relocation;
class ResolveInfo;
class RuleContainer;
class OutputSectionEntry;

class PluginOp {
public:
  enum PluginOpType : uint8_t {
    ChangeOutputSection,
    AddChunk,
    RemoveChunk,
    RemoveSymbol,
    ResetOffset,
    UpdateChunks,
    RelocationData,
  };

  explicit PluginOp(plugin::LinkerWrapper *, PluginOpType T,
                    std::string Annotation);

  PluginOpType getPluginOpType() const { return OpType; }

  std::string getAnnotation() const { return Annotation; }

  std::string getPluginName() const;

  virtual std::string getPluginOpStr() const { return ""; }

  virtual ~PluginOp() {}

protected:
  plugin::LinkerWrapper *Wrapper = nullptr;
  PluginOpType OpType;
  std::string Annotation;
};

class ChangeOutputSectionPluginOp : public PluginOp {
public:
  ChangeOutputSectionPluginOp(plugin::LinkerWrapper *W, eld::ELFSection *S,
                              std::string OutputSection,
                              std::string Annotation);

  static bool classof(const PluginOp *P) {
    return (P->getPluginOpType() == PluginOpType::ChangeOutputSection);
  }

  void setModifiedRule(RuleContainer *R) { ModifiedRule = R; }

  eld::ELFSection *getELFSection() const { return ESection; }

  std::string getOutputSectionName() const { return OutputSection; }

  std::string getPluginOpStr() const override { return "C"; }

  RuleContainer *getOrigRule() const { return OrigRule; }

  RuleContainer *getModifiedRule() const { return ModifiedRule; }

private:
  RuleContainer *OrigRule = nullptr;
  RuleContainer *ModifiedRule = nullptr;
  eld::ELFSection *ESection = nullptr;
  std::string OutputSection;
};

class AddChunkPluginOp : public PluginOp {
public:
  AddChunkPluginOp(plugin::LinkerWrapper *W, RuleContainer *Rule,
                   eld::Fragment *F, std::string Annotation);

  static bool classof(const PluginOp *P) {
    return P->getPluginOpType() == PluginOpType::AddChunk;
  }

  std::string getPluginOpStr() const override { return "A"; }

  RuleContainer *getRule() const { return Rule; }

  eld::Fragment *getFrag() const { return Frag; }

private:
  RuleContainer *Rule = nullptr;
  eld::Fragment *Frag = nullptr;
};

class RemoveChunkPluginOp : public PluginOp {
public:
  RemoveChunkPluginOp(plugin::LinkerWrapper *W, RuleContainer *Rule,
                      eld::Fragment *F, std::string Annotation);

  static bool classof(const PluginOp *P) {
    return P->getPluginOpType() == PluginOpType::RemoveChunk;
  }

  std::string getPluginOpStr() const override { return "R"; }

  RuleContainer *getRule() const { return Rule; }

  eld::Fragment *getFrag() const { return Frag; }

private:
  RuleContainer *Rule = nullptr;
  eld::Fragment *Frag = nullptr;
};

class UpdateChunksPluginOp : public PluginOp {
public:
  enum Type : uint8_t { Start, End };

  UpdateChunksPluginOp(plugin::LinkerWrapper *W, RuleContainer *Rule, Type T,
                       std::string Annotation);

  static bool classof(const PluginOp *P) {
    return P->getPluginOpType() == PluginOpType::UpdateChunks;
  }

  std::string getPluginOpStr() const override {
    if (T == Start)
      return "U_S";
    return "U_E";
  }

  RuleContainer *getRule() const { return Rule; }

private:
  RuleContainer *Rule = nullptr;
  Type T = Start;
};

class RemoveSymbolPluginOp : public PluginOp {
public:
  RemoveSymbolPluginOp(plugin::LinkerWrapper *W, std::string Annotation,
                       const eld::ResolveInfo *S);

  static bool classof(const PluginOp *P) {
    return P->getPluginOpType() == PluginOpType::RemoveSymbol;
  }

  std::string getPluginOpStr() const override { return "RS"; }

  const ResolveInfo *getRemovedSymbol() const { return RemovedSymbol; }

private:
  const ResolveInfo *RemovedSymbol;
};

class RelocationDataPluginOp : public PluginOp {
public:
  RelocationDataPluginOp(plugin::LinkerWrapper *W, const eld::Relocation *R,
                         const std::string &Annotation = {});

  static bool classof(const PluginOp *P) {
    return P->getPluginOpType() == PluginOpType::RelocationData;
  }

  std::string getPluginOpStr() const override { return "RD"; }

  const eld::Relocation *getRelocation() const { return Relocation; }
  const eld::Fragment *getFrag() const;

private:
  const eld::Relocation *Relocation;
};

class ResetOffsetPluginOp : public PluginOp {
public:
  ResetOffsetPluginOp(plugin::LinkerWrapper *W, const OutputSectionEntry *O,
                      uint32_t OldOffset, const std::string &Annotation = {});

  static bool classof(const PluginOp *P) {
    return P->getPluginOpType() == PluginOpType::ResetOffset;
  }

  std::string getPluginOpStr() const override { return "RO"; }

  const OutputSectionEntry *getOutputSection() const { return O; }

  uint32_t getOldOffset() const { return OldOffset; }

private:
  const OutputSectionEntry *O;
  uint32_t OldOffset;
};

} // namespace eld

#endif
