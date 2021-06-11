#include <itest.h>

TEST(testcase_file_equal, test0)
{
    EXPECT_FILE_NOT_EQ("utest/utt/test_file/regex.txt","utest/utt/test_file/regex_copy.txt");
}
