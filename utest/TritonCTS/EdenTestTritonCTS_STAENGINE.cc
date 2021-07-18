#include <StaEngine.h>
#include <itest.h>
#include <tcl.h>
#include <tritoncts/TritonCTS.h>

#include "db_sta/dbSta.hh"
#include "ord/OpenRoad.hh"
#include "sta/PatternMatch.hh"
#include "sta/Sdc.hh"

using namespace cts;

class TritonCTS_StaEngine : public ::testing::Test
{
 private:
  const char* defName = "src/TritonCTS/test/16sinks.def";
  const char* lefName = "src/TritonCTS/test/Nangate45/Nangate45.lef";
  const char* libName = "src/TritonCTS/test/Nangate45/Nangate45_typ.lib";

 protected:
  ord::OpenRoad* openRoad;
  std::shared_ptr<StaEngine> sta;

  void SetUp() override
  {
    auto* interp = Tcl_CreateInterp();
    Tcl_Init(interp);
    openRoad = ord::OpenRoad::openRoad();
    openRoad->init(interp);
    // read pdk incase runtime error
    openRoad->getSta()->readLiberty(
        libName, openRoad->getSta()->cmdCorner(), sta::MinMaxAll::all(), true);
    openRoad->readLef(lefName, "test", true, true);
    openRoad->readDef(defName, false, false, false);
    sta = std::make_shared<StaEngine>(openRoad->getTritonCts()->getParms());
    // give arguments necessary to tritonCTS
    Tcl_Eval(interp, "create_clock -period 5 clk");
  }

  void TearDown() override { openRoad->getDb()->clear(); }
};

TEST_F(TritonCTS_StaEngine, all_functions)
{
  sta::PatternMatch matcher("*");
  sta::ClockSeq clks;
  std::vector<std::set<odb::dbNet*>> clockNetInfo;

  ASSERT_NO_THROW(sta->init());
  openRoad->getSta()->sdc()->findClocksMatching(&matcher, &clks);
  for (auto clk : clks) {
    std::set<odb::dbNet*> clkNets;
    sta->findClockRoots(clk, clkNets);
    EXPECT_STREQ(clk->name(), "clk");

    for (auto net : clkNets) {
      EXPECT_EQ(net->getName(), "clk");

      for (odb::dbITerm* iterm : net->getITerms()) {
        EXPECT_FLOAT_EQ(sta->getInputPinCap(iterm), 9.49653e-16f);
        EXPECT_TRUE(sta->isSink(iterm));
      }
    }
  }
}