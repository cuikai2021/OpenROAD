#include <HTreeBuilder.h>
#include <TechChar.h>
#include <itest.h>
#include <tritoncts/TritonCTS.h>

#include <db_sta/dbSta.hh>

#define TEST_OK_FILES_PATH "utest/TritonCTS/test"
#define TEST_FILES_PATH TEST_OK_FILES_PATH "/results"

using namespace cts;

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
 protected:
  const char* logFile = TEST_FILES_PATH "/h_tree_builder.log";
  const char* okFile = TEST_OK_FILES_PATH "/h_tree_builder.ok";
};

TEST_F(TritonCTS_HTreeBuilder, run_and_plot)
{
  std::shared_ptr<HTreeBuilder> builder;
  ord::OpenRoad* openRoad = ord::OpenRoad::openRoad();

  unsigned outCap = 0;
  unsigned outSlew = 0;

  const char* defName = "utest/TritonCTS/test/clustered_test.def";
  const char* lefName = "src/TritonCTS/test/Nangate45/Nangate45.lef";
  const char* libName = "src/TritonCTS/test/Nangate45/Nangate45_typ.lib";

  remove(logFile);

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
  // from regression source `check_buffer.tcl`
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

  TechChar* tech = openRoad->getTritonCts()->getCharacterization();
  CtsOptions* options = openRoad->getTritonCts()->getParms();
  tech->create();
  Clock clock = Clock("netName", "clockPin", "sdcClockName", 0, 0);
  for (int i = 0; i < 10; ++i) {
    clock.addSink("sinks_" + std::to_string(i), i, i);
  }
  auto logger = std::make_shared<Logger>(logFile);
  builder = std::make_shared<HTreeBuilder>(
      openRoad->getTritonCts()->getParms(), clock, nullptr, logger.get());
  // Function from `Treebuilder`
  builder->setTechChar(*tech);
  builder->run();

  builder->plotSolution();
  system("mv plot0.py " TEST_FILES_PATH "/plot0.py");

  EXPECT_EQ(builder->computeMinDelaySegment(tech->getMaxSegmentLength(),
                                            1,
                                            1,
                                            options->getMaxSlew(),
                                            1,
                                            outCap,
                                            outSlew),
            24);

  // clear all data
  openRoad->getDb()->clear();
}

TEST_F(TritonCTS_HTreeBuilder, file_equal)
{
  EXPECT_FILE_EQ(logFile, okFile);
  EXPECT_FILE_EQ(TEST_FILES_PATH "/plot0.py", TEST_OK_FILES_PATH "/plot0.ok");
}
