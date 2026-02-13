//===- LibReader.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef ELD_OBJECT_LIBREADER_H
#define ELD_OBJECT_LIBREADER_H

#include "eld/Core/Module.h"

namespace eld {

class LinkerConfig;
class ObjectLinker;

/** \class LibReader
 *  \brief LibReader handles the LibStart/LibEnd Node in InputTree
 *
 *  Lib nodes are created by the command line options --start-lib and --end-lib.
 *  All inputs in between are packaged into an in-memory archive and then
 *  processed as a normal archive input.
 */
class LibReader {
public:
  LibReader(Module &M, ObjectLinker *ObjLinker);

  ~LibReader();

  bool readLib(InputBuilder::InputIteratorT &Node, InputBuilder &Builder,
               LinkerConfig &Config, bool IsPostLtoPhase = false);

private:
  Module &MModule;
  ObjectLinker *MObjLinker = nullptr;
};

} // namespace eld

#endif
