#include "misc_tools.h"

#include <gtest/gtest.h>

namespace {

TEST(MultiByteStrings, TextLength)
{
    wchar_t buf[100];
    EXPECT_EQ(mbs_to_wcs_buf(buf, "Hello, world!", 100), 13);
}

}  // namespace
