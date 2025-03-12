//===- InputFileTest.h ----------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef STATICRESOLVER_TEST_H
#define STATICRESOLVER_TEST_H

#include "eld/Config/LinkerConfig.h"
#include <gtest/gtest.h>

namespace eld {
class DiagnosticEngine;
class DiagnosticPrinter;

} // namespace eld

/** \class InputFileTest
 *  \brief The testcases for input files
 *
 *  \see InputFileTest
 */
class InputFileTest : public ::testing::Test {
public:
  // Constructor can do set-up work for all test here.
  InputFileTest();

  // Destructor can do clean-up work that doesn't throw exceptions here.
  virtual ~InputFileTest();

  // SetUp() will be called immediately before each test.
  virtual void SetUp();

  // TearDown() will be called immediately after each test.
  virtual void TearDown();

protected:
  eld::LinkerConfig *m_pConfig;
  eld::DiagnosticEngine *m_pDiagEngine = nullptr;
};

#endif
