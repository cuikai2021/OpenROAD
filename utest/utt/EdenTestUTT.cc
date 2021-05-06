#include <itest.h>

TEST(testcase_file_equal, test0)
{
    EXPECT_FILE_EQ_CODE("test_file/regex.cc","test_file/regex_copy.cc");
}
