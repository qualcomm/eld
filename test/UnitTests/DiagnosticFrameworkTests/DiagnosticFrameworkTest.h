//===- DiagnosticFrameworkTest.h ----------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef UNITTESTS_DIAGNOSTIC_FRAMEWORK_TEST_H
#define UNITTESTS_DIAGNOSTIC_FRAMEWORK_TEST_H

#include <gtest/gtest.h>

namespace eld {
class LinkerConfig;
class DiagnosticEngine;
} // namespace eld

/** \class DiagnosticFrameworkTest
 *  \brief The testcases for diagnostic framework
 *
 *  \see DiagnosticFrameworkTest
 */
class DiagnosticFrameworkTest : public ::testing::Test {
public:
  // Constructor can do set-up work for all test here.
  DiagnosticFrameworkTest();

  // Destructor can do clean-up work that doesn't throw exceptions here.
  virtual ~DiagnosticFrameworkTest() override;

  // SetUp() will be called immediately before each test.
  virtual void SetUp() override;

  // TearDown() will be called immediately after each test.
  virtual void TearDown() override;

protected:
  eld::LinkerConfig *m_Config = nullptr;
  eld::DiagnosticEngine *m_DiagEngine = nullptr;
};

#endif
