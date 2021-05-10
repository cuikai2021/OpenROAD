#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <fstream>
#include <string>

namespace testing {

#define EXPECT_FILE_EQ(val1, val2, flag) file_equal(val1, val2, flag)
#define EXPECT_FILE_EQ_CODE(val1, val2) \
  file_equal(val1, val2, testing::kIgnoreCommitCode)
#define EXPECT_FILE_EQ_SCRIPT(val1, val2) \
  file_equal(val1, val2, testing::kIgnoreCommitScript)
#define EXPECT_FILE_EQ_NO_IGNORE(val1, val2) file_equal(val1, val2, testing::kNoIgnore)

typedef enum
{
  kNoIgnore,
  kIgnoreCommitCode,
  kIgnoreCommitScript
} ignore;

void file_equal(std::string file0, std::string file1, ignore ig);

}  // namespace testing