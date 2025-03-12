//===- LinkerDriverTest.cpp
//-------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "eld/Driver/Driver.h"
#include "eld/Driver/GnuLdDriver.h"
#include "gtest/gtest.h"
using namespace eld;

class DriverTest : public ::testing::Test {
protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

//==========================================================================//
// Testcases
//
TEST_F(DriverTest, InvalidTarget) {
  Driver *driver = new Driver(Flavor::Invalid, "random");
  // Default to the first target.
  ASSERT_NE(driver->getLinker(), nullptr);
  delete driver;
}

TEST_F(DriverTest, ValidTarget) {
  Driver *driver = new Driver(Flavor::Hexagon, "hexagon");
  ASSERT_NE(driver->getLinker(), nullptr);
  delete driver;
}
