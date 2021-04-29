#include <Checker.h>
#include <ppl/IOPlacer.h>

using namespace ppl;

TEST(testcase_Interval_constructor, test0)
{
  Interval* in = new Interval();
  EXPECT_EQ(in->begin,0);
  delete in;
}
TEST(testcase_Interval_constructor, test1)
{
  Interval* in = new Interval();
  EXPECT_EQ(in->begin,1);
  delete in;
}
TEST(testcase_Interval_constructor, test2)
{
  Interval* in = new Interval();
  EXPECT_EQ(in->begin,2);
}
TEST(testcase_Interval_constructor, test3)
{
  Interval* in = new Interval();
  EXPECT_EQ(in->begin,3);
  delete in;
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}