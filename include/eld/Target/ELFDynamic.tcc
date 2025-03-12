//===- ELFDynamic.tcc ---------------------------------------------------===//
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

//===----------------------------------------------------------------------===//
/// 32-bit dynamic entry
Entry<32, true>::Entry()
{
  m_Pair.d_tag = 0;
  m_Pair.d_un.d_val = 0;
}

Entry<32, true>::~Entry()
{
}

void Entry<32, true>::setValue(uint64_t pTag, uint64_t pValue)
{
  m_Pair.d_tag = pTag;
  m_Pair.d_un.d_val = pValue;
}

size_t Entry<32, true>::emit(uint8_t* pAddress) const
{
  memcpy(reinterpret_cast<void*>(pAddress),
         reinterpret_cast<const void*>(&m_Pair),
         sizeof(Pair));
  return sizeof(Pair);
}

//===----------------------------------------------------------------------===//
/// 64-bit dynamic entry
Entry<64, true>::Entry()
{
  m_Pair.d_tag = 0;
  m_Pair.d_un.d_val = 0;
}

Entry<64, true>::~Entry()
{
}

void Entry<64, true>::setValue(uint64_t pTag, uint64_t pValue)
{
  m_Pair.d_tag = pTag;
  m_Pair.d_un.d_val = pValue;
}

size_t Entry<64, true>::emit(uint8_t* pAddress) const
{
  memcpy(reinterpret_cast<void*>(pAddress),
         reinterpret_cast<const void*>(&m_Pair),
         sizeof(Pair));
  return sizeof(Pair);
}

