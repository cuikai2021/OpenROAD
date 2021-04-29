#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <fstream>
#include <regex>
#include <string>

namespace utt {

#define EXPECT_FILE_EQ(val1, val2, flag) file_equal(val1, val2, flag)
#define EXPECT_FILE_EQ_CODE(val1, val2) \
  file_equal(val1, val2, utt::ignore_commit_code)
#define EXPECT_FILE_EQ_SCRIPT(val1, val2) \
  file_equal(val1, val2, utt::ignore_commit_script)
#define EXPECT_FILE_EQ_NO_IGNORE(val1, val2) file_equal(val1, val2, utt::no_ignore)

typedef enum
{
  no_ignore,
  ignore_commit_code,
  ignore_commit_script
} ignore;

void file_equal(std::string file0, std::string file1, ignore ig);

}  // namespace utt