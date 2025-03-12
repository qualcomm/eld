//===- GroupReader.h-------------------------------------------------------===//
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
#ifndef ELD_READERS_GROUPREADER_H
#define ELD_READERS_GROUPREADER_H

#include "eld/Core/Module.h"

namespace eld {
class Archive;
class ArchiveReader;
class LinkerConfig;
class ObjectReader;
class BitcodeReader;
class Node;

/** \class GroupReader
 *  \brief GroupReader handles the Group Node in InputTree
 *
 *  Group Node is the root of sub-tree in InputTree which includes the iputs in
 *  the command line options --start-group and --end-group options
 */
class GroupReader {
public:
  GroupReader(Module &pModule, ObjectLinker *ObjLinker);

  ~GroupReader();

  bool readGroup(InputBuilder::InputIteratorT &node, InputBuilder &pBuilder,
                 LinkerConfig &pConfig, bool isPostLTOPhase = false);

private:
  Module &m_Module;
  ObjectLinker *m_ObjLinker;
};

} // namespace eld

#endif
