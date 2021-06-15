#include <HTreeBuilder.h>
#include <LevelBalancer.h>
#include <SinkClustering.h>
#include <itest.h>
#include <tritoncts/TritonCTS.h>

#include "Clock.h"
#include "CtsOptions.h"
#include "LevelBalancer.h"
#include "PostCtsOpt.h"
#include "StaEngine.h"
#include "TechChar.h"
#include "TreeBuilder.h"
#include "db_sta/dbSta.hh"
#include "opendb/db.h"
#include "opendb/dbShape.h"
#include "sta/PatternMatch.hh"
#include "sta/Sdc.hh"
#include "utl/Logger.h"

#define TEST_OK_FILES_PATH "utest/TritonCTS/test"
#define TEST_FILES_PATH TEST_OK_FILES_PATH "/results"

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
  // these two functions has been test in global test
  ASSERT_NO_THROW(levelBalancer->run());
  ASSERT_NO_THROW(levelBalancer->fixTreeLevels(ttbChild.get(), 10, 26));
}

class TritonCTS_LevelBalancerGlobal : public ::testing::Test
{
 private:
  void computeITermPosition(odb::dbITerm* term, int& x, int& y) const
  {
    odb::dbITermShapeItr itr;

    odb::dbShape shape;
    x = 0;
    y = 0;
    unsigned numShapes = 0;
    for (itr.begin(term); itr.next(shape);) {
      if (!shape.isVia()) {
        x += shape.xMin() + (shape.xMax() - shape.xMin()) / 2;
        y += shape.yMin() + (shape.yMax() - shape.yMin()) / 2;
        ++numShapes;
      }
    }
    if (numShapes > 0) {
      x /= numShapes;
      y /= numShapes;
    }
  };
  void incrementNumClocks() { numberOfClocks = numberOfClocks + 1; }

 protected:
  constexpr static const char* logFile = TEST_FILES_PATH "/level_balancer.log";
  const char* okFile = TEST_OK_FILES_PATH "/level_balancer.ok";
  std::set<odb::dbNet*> visitedClockNets;
  unsigned numberOfClocks = 0;
  std::set<odb::dbNet*> staClockNets;
  std::vector<HTreeBuilder*> _builder;

  static void SetUpTestSuite() { remove(logFile); }

  TreeBuilder* initClock(odb::dbNet* net,
                         std::string sdcClock,
                         TreeBuilder* parentBuilder,
                         CtsOptions* options,
                         Logger* logger,
                         StaEngine* staEngine)
  {
    std::string driver = "";
    odb::dbITerm* iterm = net->getFirstOutput();
    int xPin, yPin;
    if (iterm == nullptr) {
      odb::dbBTerm* bterm = net->get1stBTerm();  // Clock pin
      driver = bterm->getConstName();
      bterm->getFirstPinLocation(xPin, yPin);
    } else {
      odb::dbInst* inst = iterm->getInst();
      odb::dbMTerm* mterm = iterm->getMTerm();
      std::string driver = std::string(inst->getConstName()) + "/"
                           + std::string(mterm->getConstName());
      int xTmp, yTmp;
      computeITermPosition(iterm, xTmp, yTmp);
      xPin = xTmp;
      yPin = yTmp;
    }

    // Initialize clock net

    Clock clockNet(net->getConstName(), driver, sdcClock, xPin, yPin);
    clockNet.setDriverPin(iterm);

    for (odb::dbITerm* iterm : net->getITerms()) {
      odb::dbInst* inst = iterm->getInst();

      if (iterm->isInputSignal() && inst->isPlaced()) {
        odb::dbMTerm* mterm = iterm->getMTerm();
        std::string name = std::string(inst->getConstName()) + "/"
                           + std::string(mterm->getConstName());
        int x, y;
        computeITermPosition(iterm, x, y);
        clockNet.addSink(name, x, y, iterm, staEngine->getInputPinCap(iterm));
      }
    }

    if (clockNet.getNumSinks() < 2) {
      return nullptr;
    } else {
      if (clockNet.getNumSinks() == 0) {
        return nullptr;
      }
    }

    long int totalSinks = options->getNumSinks() + clockNet.getNumSinks();
    options->setNumSinks(totalSinks);

    incrementNumClocks();

    clockNet.setNetObj(net);
    HTreeBuilder* builder
        = new HTreeBuilder(options, clockNet, parentBuilder, logger);
    // addBuilder(builder);
    _builder.push_back(builder);
    return builder;
  }

  odb::dbITerm* getSingleOutput(odb::dbInst* inst, odb::dbITerm* input) const
  {
    odb::dbSet<odb::dbITerm> iterms = inst->getITerms();
    odb::dbITerm* output = nullptr;
    for (odb::dbITerm* iterm : iterms) {
      if (iterm != input && iterm->isOutputSignal()) {
        odb::dbNet* net = iterm->getNet();
        if (net) {
          if (staClockNets.find(net) != staClockNets.end()) {
            output = iterm;
            break;
          }
        }
      }
    }
    return output;
  }
  void initOneClockTree(odb::dbNet* driverNet,
                        std::string sdcClockName,
                        TreeBuilder* parent,
                        CtsOptions* options,
                        Logger* logger,
                        StaEngine* staEngine)
  {
    TreeBuilder* clockBuilder = initClock(
        driverNet, sdcClockName, parent, options, logger, staEngine);
    visitedClockNets.insert(driverNet);
    odb::dbITerm* driver = driverNet->getFirstOutput();
    odb::dbSet<odb::dbITerm> iterms = driverNet->getITerms();
    for (odb::dbITerm* iterm : iterms) {
      if (iterm != driver && iterm->isInputSignal()) {
        if (!staEngine->isSink(iterm)
            && (iterm->getInst()->isCore()
                || iterm->getInst()->isPad())) {  // Clock Gate
          odb::dbITerm* outputPin = getSingleOutput(iterm->getInst(), iterm);
          if (outputPin && outputPin->getNet()) {
            odb::dbNet* outputNet = outputPin->getNet();
            if (visitedClockNets.find(outputNet) == visitedClockNets.end())
              initOneClockTree(outputNet,
                               sdcClockName,
                               clockBuilder,
                               options,
                               logger,
                               staEngine);
          }
        }
      }
    }
  }
};

TEST_F(TritonCTS_LevelBalancerGlobal, global_test_level_balancer)
{
  std::shared_ptr<StaEngine> staEngine;
  TechChar* tech = nullptr;
  CtsOptions* options = nullptr;
  sta::dbSta* openSta = nullptr;
  odb::dbBlock* block = nullptr;
  ord::OpenRoad* openRoad = ord::OpenRoad::openRoad();
  auto logger = std::make_shared<Logger>(logFile);

  const char* defName = "utest/TritonCTS/test/level_balance.def";
  const char* lefName = "src/TritonCTS/test/Nangate45/Nangate45.lef";
  const char* libName = "src/TritonCTS/test/Nangate45/Nangate45_typ.lib";

  // Setup the app with tcl
  auto* interp = Tcl_CreateInterp();  // no need to free it manually
  Tcl_Init(interp);
  // must init an openRoad object incase segment fault

  openRoad->init(interp);
  // read pdk incase runtime error
  openRoad->getSta()->readLiberty(
      libName, openRoad->getSta()->cmdCorner(), sta::MinMaxAll::all(), true);
  openRoad->readLef(lefName, "test", true, true);
  openRoad->readDef(defName, false, false, false);

  // give arguments necessary to tritonCTS
  Tcl_Eval(interp, "create_clock -period 5 clk");
  openRoad->getTritonCts()->getParms()->setRootBuffer("CLKBUF_X3");
  openRoad->getTritonCts()->setBufferList("CLKBUF_X3");
  openRoad->getTritonCts()->getParms()->setSinkBuffer("CLKBUF_X3");
  openRoad->getTritonCts()->getParms()->setWireSegmentUnit(20);
  openRoad->getTritonCts()->getParms()->setSinkClustering(true);
  openRoad->getTritonCts()->getParms()->setSimpleSegmentsEnabled(true);
  openRoad->getTritonCts()->getParms()->setBufferDistance(100);
  openRoad->getTritonCts()->getParms()->setSizeSinkClustering(10);
  openRoad->getTritonCts()->getParms()->setMaxDiameter(60);
  openRoad->getTritonCts()->getParms()->setNumStaticLayers(1);

  // TritonCTS::setupCharacterization();
  tech = openRoad->getTritonCts()->getCharacterization();
  options = openRoad->getTritonCts()->getParms();
  tech->create();

  // TritonCTS::findClockRoots();
  staEngine = std::make_shared<StaEngine>(options);
  staEngine->init();

  // TritonCTS::populateTritonCts()
  // TritonCTS::initDB()
  openSta = openRoad->getSta();
  block = openRoad->getDb()->getChip()->getBlock();
  options->setDbUnits(block->getDbUnitsPerMicron());
  // TritonCTS::initAllClocks()
  // Uses dbSta to find all clock nets in the design.
  std::vector<std::pair<std::set<odb::dbNet*>, std::string>> clockNetsInfo;
  // Checks the user input in case there are other nets that need to be added to
  // the set.
  std::vector<odb::dbNet*> inputClkNets = options->getClockNetsObjs();

  if (!inputClkNets.empty()) {
    std::set<odb::dbNet*> clockNets;
    for (odb::dbNet* net : inputClkNets) {
      // Since a set is unique, only the nets not found by dbSta are added.
      clockNets.insert(net);
    }
    clockNetsInfo.emplace_back(std::make_pair(clockNets, std::string("")));
  } else {
    staClockNets = openSta->findClkNets();
    sta::Sdc* sdc = openSta->sdc();
    sta::PatternMatch matcher("*");
    sta::ClockSeq clks;
    sdc->findClocksMatching(&matcher, &clks);
    for (auto clk : clks) {
      std::string clkName = clk->name();
      std::set<odb::dbNet*> clkNets;
      staEngine->findClockRoots(clk, clkNets);
      clockNetsInfo.emplace_back(make_pair(clkNets, clkName));
    }
  }
  // Iterate over all the nets found by the user-input and dbSta
  for (auto clockInfo : clockNetsInfo) {
    std::set<odb::dbNet*> clockNets = clockInfo.first;
    std::string clkName = clockInfo.second;
    for (odb::dbNet* net : clockNets) {
      if (net != nullptr) {
        // Initializes the net in TritonCTS. If the number of sinks is less than
        // 2, the net is discarded.
        initOneClockTree(
            net, clkName, nullptr, options, logger.get(), staEngine.get());
      }
    }
  }
  options->setNumClockRoots(numberOfClocks);

  // build tree
  for (auto builder : _builder) {
    builder->setTechChar(*tech);
    builder->run();
  }
  // run level balancer
  for (TreeBuilder* builder : _builder) {
    if (!builder->getParent() && builder->getChildren().size()) {
      LevelBalancer balancer(builder, options, logger.get());
      ASSERT_NO_THROW(balancer.run());
    }
  }

  // clear all data
  openRoad->getDb()->clear();
}

TEST_F(TritonCTS_LevelBalancerGlobal, file_equal)
{
  EXPECT_FILE_EQ(okFile, logFile);
}