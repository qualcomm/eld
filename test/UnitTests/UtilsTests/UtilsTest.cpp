//===- UtilsTest.cpp ------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/Utils.h"
#include "gtest/gtest.h"

TEST(ResponseFileSerializationTest, GNUValues) {
  // Spaces must remain part of the same response-file argument.
  EXPECT_EQ(eld::utility::serializeGNUResponseFileArgument("a b"), "\"a b\"");

  // GNU tokenization treats backslashes as escapes, so printArg doubles
  // literal backslashes.
  EXPECT_EQ(eld::utility::serializeGNUResponseFileArgument("a\\b"),
            "\"a\\\\b\"");

  // Single quotes are ordinary characters inside printArg's double quotes.
  EXPECT_EQ(eld::utility::serializeGNUResponseFileArgument("a'b"), "\"a'b\"");

  // An empty value still needs a quoted response-file token.
  EXPECT_EQ(eld::utility::serializeGNUResponseFileArgument(""), "\"\"");
}

TEST(ResponseFileSerializationTest, WindowsValues) {
  // Spaces must remain part of the same response-file argument while normal
  // Windows path separators remain unchanged.
  EXPECT_EQ(eld::utility::serializeWindowsResponseFileArgument(
                "C:\\Program Files\\a.o"),
            "\"C:\\Program Files\\a.o\"");

  // An embedded double quote must be escaped.
  EXPECT_EQ(eld::utility::serializeWindowsResponseFileArgument("a\"b"),
            "\"a\\\"b\"");

  // Backslashes immediately before the closing quote must be doubled.
  EXPECT_EQ(eld::utility::serializeWindowsResponseFileArgument("C:\\dir\\"),
            "\"C:\\dir\\\\\"");

  // An empty value still needs a quoted response-file token.
  EXPECT_EQ(eld::utility::serializeWindowsResponseFileArgument(""), "\"\"");
}
