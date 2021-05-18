#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>
#include <algorithm>
#include <memory>

namespace testing {

#define EXPECT_FILE_EQ(val1, val2) testing::file_equal(val1, val2, true)
#define EXPECT_FILE_NOT_EQ(val1, val2) testing::file_equal(val1, val2, false)

void file_equal(const std::string file0, const std::string file1, bool need_equal);

}  // namespace testing