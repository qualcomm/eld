#include "eld/PluginAPI/LinkerWrapper.h"
#include "gtest/gtest.h"
#include <fstream>

class INIFileTest : public ::testing::Test {
protected:
  void SetUp() override {
    std::remove(m_TestFileName.c_str());
    std::ofstream testFile(m_TestFileName);
    testFile << "[variables]\n"
             << "asdf=4\n"
             << "bar=2\n"
             << "baz=3\n"
             << "foo=1\n";
    testFile.close();
  }

  void TearDown() override {
    std::remove(m_TestFileName.c_str());
  }

  const std::string m_TestFileName = "TestFile.ini";
};

TEST_F(INIFileTest, MoveOpTest) {
  eld::plugin::INIFile ini1(m_TestFileName);
  EXPECT_EQ(ini1.getValue("variables", "baz"), "3");
  EXPECT_EQ(ini1.getValue("variables", "foo"), "1");

  // Test move constructor
  eld::plugin::INIFile ini2 = std::move(ini1);
  EXPECT_TRUE(ini1.getReader() == nullptr);
  EXPECT_EQ(ini2.getValue("variables", "baz"), "3");
  EXPECT_EQ(ini2.getValue("variables", "foo"), "1");

  // Test move assignment operator
  eld::plugin::INIFile ini3;
  eld::INIReader *ini3oldReader = ini3.getReader();
  ini3 = std::move(ini2);
  EXPECT_TRUE(ini2.getReader() == ini3oldReader);
  EXPECT_EQ(ini3.getValue("variables", "baz"), "3");
  EXPECT_EQ(ini3.getValue("variables", "foo"), "1");
}