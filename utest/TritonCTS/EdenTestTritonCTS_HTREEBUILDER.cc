#include <HTreeBuilder.h>
#include <LevelBalancer.h>
#include <SinkClustering.h>
#include <itest.h>
#include <cts/TritonCTS.h>

#include "Clock.h"
#include "CtsOptions.h"
#include "LevelBalancer.h"
#include "PostCtsOpt.h"
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
#if 0
class TritonCTS_SegmentBuilder : public ::testing::Test
{
 protected:
  const std::string instPrefix = "instPrefix";
  const std::string netPrefix = "netPrefix";
  Point<double> target = Point<double>(123.456f, 456.789f);
  unsigned techCharDistUnit = techChar->getLengthUnit() * options.getDbUnits();

  std::shared_ptr<SegmentBuilder> segment;
  CtsOptions options = CtsOptions();
  std::shared_ptr<TechChar> techChar
      = std::make_shared<TechChar>(&options, logger.get());
  std::shared_ptr<Clock> clock
      = std::make_shared<Clock>("netName", "clockPin", "sdcClockName", 0, 0);

  void SetUp() override
  {
    for (unsigned i = 0; i < 10; ++i) {
      techCharWires.emplace_back(i);
      techChar->createWireSegment(i, i, i, 1.0f, (unsigned) i, i, i);
    }
    segment = std::make_shared<SegmentBuilder>(instPrefix,
                                               netPrefix,
                                               root,
                                               target,
                                               techCharWires,
                                               *clock,
                                               drivingSubNet,
                                               *techChar,
                                               techCharDistUnit);
  }

 private:
  Point<double> root = Point<double>(0.0f, 0.0f);

  std::vector<unsigned> techCharWires;
  std::shared_ptr<utl::Logger> logger = std::make_shared<utl::Logger>();
  Clock::SubNet drivingSubNet = Clock::SubNet("SubNet");
};

TEST_F(TritonCTS_SegmentBuilder, sub_net_getter)
{
  ClockInst clock_inst("name", "master", CLOCK_SINK, 0, 0, nullptr, 0.0f);
  ClockInst replace("replace", "master", CLOCK_SINK, 1, 1, nullptr, 0.0f);

  Clock::SubNet* subNet = segment->getDrivingSubNet();

  for (int i = 0; i < 10; ++i) {
    subNet->addInst(clock_inst);
  }
  EXPECT_EQ(subNet->findIndex(&clock_inst), 9);
  EXPECT_EQ(segment->getDrivingSubNet()->getNumSinks(), 9);
}

TEST_F(TritonCTS_SegmentBuilder, builder)
{
  // no public functions to set buffers in wiresegment directly in TechChar
  // can not test its formal functions individually in class
  // it will be test at global test in child class `HTreeBuilder`
  ASSERT_NO_THROW(segment->build());
}

TEST_F(TritonCTS_SegmentBuilder,
       force_buffer_in_Segment_and_buffer_level_getter)
{
  std::vector<std::string> bufferName;
  std::vector<std::string> bufferMaster;
  std::vector<std::string> subName;
  std::vector<Point<DBU>> bufferLocation;
  std::vector<ClockInst> newBuffer;
  std::vector<Clock::SubNet> newSubNet;

  EXPECT_EQ(segment->getNumBufferLevels(), 0);

  for (unsigned i = 0; i < 10; ++i) {
    segment->forceBufferInSegment(std::to_string(i));
  }

  clock->forEachClockBuffer(
      [&bufferName, &bufferLocation, &bufferMaster, &newBuffer](
          ClockInst inst) {
        if (inst.isClockBuffer()) {
          bufferName.push_back(inst.getName());
          bufferLocation.push_back(inst.getLocation());
          bufferMaster.push_back(inst.getMaster());
          newBuffer.push_back(inst);
        }
      });
  EXPECT_EQ(bufferName.size(), bufferLocation.size());
  EXPECT_EQ(bufferName.size(), bufferMaster.size());

  for (unsigned i = 0; i < bufferName.size(); ++i) {
    EXPECT_EQ(bufferName[i], instPrefix + "_f_" + clock->getName());
    EXPECT_EQ(bufferMaster[i], std::to_string(i));
    EXPECT_EQ(bufferLocation[i].getX(), target.getX() * techCharDistUnit);
    EXPECT_EQ(bufferLocation[i].getY(), target.getY() * techCharDistUnit);
  }

  clock->forEachSubNet([&newSubNet, &subName](Clock::SubNet sub) {
    newSubNet.push_back(sub);
    subName.push_back(sub.getName());
  });

  EXPECT_EQ(segment->getDrivingSubNet()->getName(), newSubNet.back().getName());
  for (unsigned i = 1; i < newSubNet.size(); ++i) {
    EXPECT_EQ(subName[i], netPrefix + "_leaf_" + clock->getName());
    EXPECT_EQ(newSubNet[i].findIndex(&newBuffer[i]),
              newSubNet[i].getNumSinks());
  }

  EXPECT_EQ(segment->getNumBufferLevels(), 1);
}

class TritonCTS_HTreeBuilder : public ::testing::Test
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
  std::set<odb::dbNet*> visitedClockNets;
  unsigned numberOfClocks = 0;
  std::set<odb::dbNet*> staClockNets;
  std::vector<HTreeBuilder*> _builder;

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

TEST_F(TritonCTS_HTreeBuilder, run_and_plot)
{
  std::shared_ptr<StaEngine> staEngine;
  TechChar* tech = nullptr;
  CtsOptions* options = nullptr;
  sta::dbSta* openSta = nullptr;
  odb::dbBlock* block = nullptr;
  ord::OpenRoad* openRoad = ord::OpenRoad::openRoad();
  auto logger = std::make_shared<Logger>();

  unsigned outCap = 0;
  unsigned outSlew = 0;

  const char* defName = "utest/TritonCTS/test/clustered_test.def";
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

  // test SinkClustering and its plot by the way
  openRoad->getLogger()->setDebugLevel(utl::CTS, "Stree", 1);

  // build tree
  for (auto builder : _builder) {
    builder->setTechChar(*tech);
    builder->run();
  }
  _builder[0]->plotSolution();
  system("mv plot0.py " TEST_FILES_PATH "/plot0.py");
  system("mv plot_clustering.py " TEST_FILES_PATH "/plot_sink_clustering.py");

  EXPECT_EQ(_builder[0]->computeMinDelaySegment(tech->getMaxSegmentLength(),
                                                1,
                                                1,
                                                options->getMaxSlew(),
                                                1,
                                                outCap,
                                                outSlew),
            24);

  // test no argument sink clustering by the way
  // it will match points into pair of lowest cost
  SinkClustering matching(options, tech);

  for (float i = 0.0f; i < 10; ++i) {
    for (float j = 0.0f; j < 10; ++j) {
      matching.addPoint(i, j);
      if ((i + j) > 10)
        matching.addCap(options->getSinkBufferInputCap());
      else
        matching.addCap(1.0f);
    }
  }
  matching.run();
  system("mv plot.py " TEST_FILES_PATH "/plot_sink_clustering_no_arg.py");

  EXPECT_FILE_EQ(TEST_FILES_PATH "/plot0.py", TEST_OK_FILES_PATH "/plot0.ok");
  EXPECT_FILE_EQ(TEST_FILES_PATH "/plot_sink_clustering.py",
                 TEST_OK_FILES_PATH "/plot_sink_clustering.ok");
  EXPECT_FILE_EQ(TEST_FILES_PATH "/plot_sink_clustering_no_arg.py",
                 TEST_OK_FILES_PATH "/plot_sink_clustering_no_arg.ok");

  // clear all data
  openRoad->getDb()->clear();
}
#endif 