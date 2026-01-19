#include "misc_tools.h"

#include <gtest/gtest.h>

namespace {

TEST(MultiByteStrings, TextLength)
{
    wchar_t buf[100];
    EXPECT_EQ(mbs_to_wcs_buf(buf, "Hello, world!", 100), 13);
}

TEST(GetFileName, Basic)
{
    char buf[100];
    EXPECT_EQ(get_file_name(buf, sizeof(buf), "/path/to/file.txt"), 8);
    EXPECT_STREQ(buf, "file.txt");
}

TEST(GetFileName, TrailingSlash)
{
    char buf[100];
    EXPECT_EQ(get_file_name(buf, sizeof(buf), "/path/to/dir/"), 3);
    EXPECT_STREQ(buf, "dir");
}

TEST(GetFileName, NoPath)
{
    char buf[100];
    EXPECT_EQ(get_file_name(buf, sizeof(buf), "file.txt"), 8);
    EXPECT_STREQ(buf, "file.txt");
}

TEST(GetFileName, JustSlash)
{
    char buf[100];
    EXPECT_EQ(get_file_name(buf, sizeof(buf), "/"), 0);
    EXPECT_STREQ(buf, "");
}

TEST(GetFileName, Empty)
{
    char buf[100];
    EXPECT_EQ(get_file_name(buf, sizeof(buf), ""), 0);
    EXPECT_STREQ(buf, "");
}

}  // namespace
