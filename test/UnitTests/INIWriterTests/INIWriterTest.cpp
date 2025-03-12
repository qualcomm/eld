#include "eld/Support/INIWriter.h"
#include "gtest/gtest.h"
#include <iostream>

using namespace llvm;

class INIWriterTest : public ::testing::Test {};

/// Creates a simple INIWiter and verifies its structure
TEST_F(INIWriterTest, SimpleTest) {
  auto ini = eld::INIWriter();
  ini["Misc"]["bar"] = "foo";
  ini["Misc"]["foo"] = "bar";
  ini["Options"]["Debug"] = "true";
  ini["Options"]["Optimization"] = "true";
  ini["Options"]["Verbose"] = "false";

  auto expected = "[Misc]\n"
                  "bar=foo\n"
                  "foo=bar\n"
                  "\n[Options]\n"
                  "Debug=true\n"
                  "Optimization=true\n"
                  "Verbose=false\n\n";

  std::stringstream ss;
  ss << ini;
  auto actual = ss.str();

  EXPECT_EQ(expected, actual);
}