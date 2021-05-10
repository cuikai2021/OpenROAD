#include <itest.h>

TEST(testcase_file_equal, test0)
{
    EXPECT_FILE_NOT_EQ("test_file/regex.cc","test_file/regex_copy.cc");
}
