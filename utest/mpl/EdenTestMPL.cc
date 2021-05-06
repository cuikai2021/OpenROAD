#include <itest.h>
#include <mpl/MacroPlacer.h>

using namespace mpl;

TEST(mpl_testcase_coreEdgeFromIndex, test0)
{
  EXPECT_EQ(coreEdgeFromIndex(0), CoreEdge::West);
  EXPECT_EQ(coreEdgeFromIndex(1), CoreEdge::East);
  EXPECT_EQ(coreEdgeFromIndex(2), CoreEdge::North);
  EXPECT_EQ(coreEdgeFromIndex(3), CoreEdge::South);
}
