#include <Checker.h>

TEST(testcase_file_equal, test0)
{
    EXPECT_FILE_EQ_CODE("regex.cc","regex_copy.cc");
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}