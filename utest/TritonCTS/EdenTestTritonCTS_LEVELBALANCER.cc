#include <LevelBalancer.h>
#include <itest.h>

#include "Clock.h"
#include "CtsOptions.h"
#include "TechChar.h"
#include "Util.h"

using namespace cts;

class TritonCTS_LevelBalancer : public ::testing::Test
{
 protected:
  class TestTreeBuilder : public TreeBuilder
  {
   public:
    TestTreeBuilder(CtsOptions* options, Clock& clk, TreeBuilder* parent)
        : TreeBuilder(options, clk, parent)
    {
      _treeBufLevels = 10;
    }
    void run() { _treeBufLevels++; };
  };
  Clock clock = Clock("clock", "clockPin", "sdcClockName", 0, 0);
  std::shared_ptr<TestTreeBuilder> ttbParent
      = std::make_shared<TestTreeBuilder>(&options, clock, nullptr);
  std::shared_ptr<TestTreeBuilder> ttbChild
      = std::make_shared<TestTreeBuilder>(&options, clock, ttbParent.get());
  std::shared_ptr<TestTreeBuilder> ttbBrother
      = std::make_shared<TestTreeBuilder>(&options, clock, ttbParent.get());
  std::shared_ptr<LevelBalancer> levelBalancer
      = std::make_shared<LevelBalancer>(ttbParent.get(), &options, &logger);
  utl::Logger logger = utl::Logger();

 private:
  CtsOptions options = CtsOptions();

  TechChar techChar = TechChar(&options, &logger);
};

TEST_F(TritonCTS_LevelBalancer, add_buffer_levels)
{
  unsigned buffLevels = 5;
  unsigned index = 0;
  Clock::SubNet driverNet = Clock::SubNet("driverNet");
  const std::string nameSuffix = "suffix";
  std::vector<ClockInst*> cluster;

  for (int i = 0; i < 10; ++i) {
    std::shared_ptr<ClockInst> inst
        = std::make_shared<ClockInst>("buf", "master", CLOCK_BUFFER, i, i);
    cluster.push_back(inst.get());
    driverNet.addInst(*(inst.get()));
  }

  levelBalancer->addBufferLevels(
      ttbChild.get(), cluster, &driverNet, buffLevels, nameSuffix);

  ttbChild->getClock().forEachSubNet([&](Clock::SubNet sub) {
    if (index != buffLevels - 1)
      EXPECT_FALSE(sub.isLeafLevel());
    else
      EXPECT_TRUE(sub.isLeafLevel());
    EXPECT_EQ(sub.getName(),
              "clknet_level_" + std::to_string(index) + "_" + nameSuffix
                  + std::to_string(index) + "_clock");
    ++index;
  });
}

TEST_F(TritonCTS_LevelBalancer, run_fix_compute_max_level)
{
  for (int i = 0; i < 5; ++i) {
    ttbBrother->run();
  }
  EXPECT_EQ(levelBalancer->computeMaxTreeDepth(ttbParent.get()),
            10 + 10 + 5 + 1);
  EXPECT_EQ(levelBalancer->computeMaxTreeDepth(ttbChild.get()), 10);
  EXPECT_EQ(levelBalancer->computeMaxTreeDepth(ttbBrother.get()), 10 + 5);

  // There is no sense to test fix and run without a real test case
  // these two functions has been test in ut of `HTreeBuilder`
  ASSERT_NO_THROW(levelBalancer->run());
  ASSERT_NO_THROW(levelBalancer->fixTreeLevels(ttbChild.get(), 10, 26));
}
