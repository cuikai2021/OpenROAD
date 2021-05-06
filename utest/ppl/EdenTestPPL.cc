#include <itest.h>
#include <ppl/IOPlacer.h>

using namespace ppl;

TEST(ppl_testcase_Interval_constructor, test0)
{
  Interval* in = new Interval();
  EXPECT_EQ(in->begin,0);
  delete in;
}
TEST(ppl_testcase_Interval_constructor, test1)
{
  Interval* in = new Interval();
  EXPECT_EQ(in->begin,1);
  delete in;
}