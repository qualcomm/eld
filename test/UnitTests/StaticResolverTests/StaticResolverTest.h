//===- headerTest.h -------------------------------------------------------===//
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
class StaticResolver;
class ResolveInfoFactory;
class DiagnosticPrinter;

} // namespace eld

/** \class StaticResolverTest
 *  \brief The testcases for static resolver
 *
 *  \see StaticResolver
 */
class StaticResolverTest : public ::testing::Test {
 public:
  // Constructor can do set-up work for all test here.
  StaticResolverTest();

  // Destructor can do clean-up work that doesn't throw exceptions here.
  virtual ~StaticResolverTest();

  // SetUp() will be called immediately before each test.
  virtual void SetUp();

  // TearDown() will be called immediately after each test.
  virtual void TearDown();

 protected:
   eld::StaticResolver *m_pResolver;
   eld::LinkerConfig *m_pConfig;
   eld::DiagnosticEngine *m_pDiagEngine = nullptr;
};

#endif
