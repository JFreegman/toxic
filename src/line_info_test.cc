#include "line_info.h"

#include <gtest/gtest.h>

namespace {

TEST(LineInfo, TextWidth)
{
    wchar_t buf[100];
    EXPECT_EQ(line_info_add_msg(buf, 100, "Hello, world!"), 13);
}

}  // namespace
