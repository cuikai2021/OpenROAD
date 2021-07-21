#include <Clock.h>
#include <itest.h>

#include "utl/Logger.h"

using namespace cts;

///////// test for class "ClockInst" in "Clock.h"///////////////
#if 0
TEST(TritonCTS_clock_inst, constructor_and_basic_functions)
{
  const std::string name = "name";
  const std::string master = "master";
  InstType type = CLOCK_SINK;
  DBU x = 0;
  DBU y = 0;
  auto pos = std::make_shared<Point<DBU>>(x, y);
  auto pinObj = std::make_shared<odb::dbITerm>();
  auto inst = std::make_shared<odb::dbInst>();
  float inputCap = 0.0f;

  auto clock_inst = std::make_shared<ClockInst>(
      name, master, type, x, y, nullptr, inputCap);

  EXPECT_EQ(clock_inst->getName(), name);
  EXPECT_EQ(clock_inst->getMaster(), master);
  EXPECT_EQ(clock_inst->getX(), x);
  EXPECT_EQ(clock_inst->getY(), y);
  EXPECT_EQ(clock_inst->getLocation().computeDist(*pos), 0);

  clock_inst->setInstObj(inst.get());
  EXPECT_EQ(clock_inst->getDbInst(), inst.get());
  clock_inst->setInputPinObj(pinObj.get());
  EXPECT_EQ(clock_inst->getDbInputPin(), pinObj.get());
  clock_inst->setInputCap(1681.156f);
  EXPECT_FLOAT_EQ(clock_inst->getInputCap(), 1681.156f);
  EXPECT_FALSE(clock_inst->isClockBuffer());
}

///////// test for class "Clock" in "Clock.h"///////////////

class TritonCTS_clock : public ::testing::Test
{
 protected:
  std::shared_ptr<Clock> clock = std::make_shared<Clock>(netName,
                                                         clockPin,
                                                         sdcClockName,
                                                         clockPinX,
                                                         clockPinY);

 private:
  const std::string netName = "netName";
  const std::string clockPin = "clockPin";
  const std::string sdcClockName = "sdcClockName";
  DBU clockPinX = 0;
  DBU clockPinY = 0;
};

TEST_F(TritonCTS_clock, clock_basic_functions)
{
  DBU x = 123;
  auto netObj = std::make_shared<odb::dbNet>();

  EXPECT_EQ(clock->getMaxLevel(), 0);
  clock->setMaxLevel(x);
  EXPECT_EQ(clock->getMaxLevel(), x);

  EXPECT_EQ(clock->getNetObj(), nullptr);
  clock->setNetObj(netObj.get());
  EXPECT_EQ(clock->getNetObj(), netObj.get());

  EXPECT_EQ(clock->getDriverPin(), nullptr);
  clock->setDriverPin(netObj.get());
  EXPECT_EQ(clock->getDriverPin(), netObj.get());
}

TEST_F(TritonCTS_clock, clock_add_find_clock)
{
  const std::string name = "clockBuffer";
  const std::string master = "master";
  DBU x = 123;
  DBU y = 456;
  auto pos = std::make_shared<Point<DBU>>(x, y);

  EXPECT_EQ(clock->findClockByName(name + "_" + clock->getName()), nullptr);
  ClockInst* clockInst = &(clock->addClockBuffer(name, master, x, y));
  EXPECT_TRUE(clockInst->isClockBuffer());

  clockInst = clock->findClockByName(name + "_" + clock->getName());
  EXPECT_EQ(clockInst->getLocation().computeDist(*pos), 0);

  // allow to add clock buffer with same name
  clockInst = &(clock->addClockBuffer(name, master, x + 1, y + 1));
  EXPECT_EQ(clockInst->getLocation().computeDist(*pos), 2);
}

TEST_F(TritonCTS_clock, clock_buffer_iteration)
{
  const std::string name = "clockBuffer";
  const std::string master = "master";
  DBU x = 123;
  DBU y = 456;
  std::vector<std::string> bufferName;
  std::deque<bool> isBuffer;

  for (int i = 0; i < 10; ++i) {
    clock->addClockBuffer(name + std::to_string(i), master, x, y);
  }

  clock->forEachClockBuffer([&bufferName](const ClockInst& inst) {
    bufferName.push_back(inst.getName());
  });
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(bufferName[i], name + std::to_string(i) + "_" + clock->getName());
  }

  clock->forEachClockBuffer([&isBuffer](ClockInst& inst) {
    isBuffer.emplace_back(inst.isClockBuffer());
  });
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(isBuffer[i]);
  }
}

TEST_F(TritonCTS_clock, clock_subNet_add_remove_iteration)
{
  const std::string name = "SubNet";
  ClockInst clock_inst("name", "master", CLOCK_SINK, 0, 0, nullptr, 0.0f);
  ClockInst replace("replace", "master", CLOCK_SINK, 1, 1, nullptr, 0.0f);
  std::vector<std::string> sinkName;
  std::set<ClockInst*> sinksToRemove;
  Clock::SubNet subNet(name);

  // allow to add same sub net into clock
  // but only save the largest index in the deque
  for (int i = 0; i < 10; ++i) {
    subNet.addInst(clock_inst);
  }
  EXPECT_EQ(subNet.findIndex(&clock_inst), 9);
  EXPECT_EQ(subNet.getNumSinks(), 9);

  subNet.replaceSink(&clock_inst, &replace);
  EXPECT_EQ(subNet.findIndex(&replace), 9);
  EXPECT_EQ(subNet.getDriver(), &clock_inst);

  subNet.forEachSink([&sinkName](ClockInst* inst) -> void {
    // the iteration of all sink is not including the first element in its map
    // To avoid potential dangers
    // Thus, the first ClockInst element in subNet need to be considered as
    // nonexistent
    sinkName.push_back(inst->getName());
  });
  for (int i = 0; i < 8; ++i) {
    // here the index is the "real" index in the container
    // but if you want to search inst in clock, the index shell plus 1
    EXPECT_EQ(sinkName[i], "name");
  }
  EXPECT_EQ(sinkName[8], "replace");

  sinksToRemove.insert(&clock_inst);
  sinksToRemove.insert(&replace);
  subNet.removeSinks(sinksToRemove);
  EXPECT_EQ(subNet.findIndex(&clock_inst), 0);
  EXPECT_EQ(subNet.getNumSinks(), 0);
}

TEST_F(TritonCTS_clock, sub_net_basic_function)
{
  const std::string name = "SubNet";
  std::vector<std::string> instName;
  std::deque<bool> leafs;
  Clock::SubNet subNet("");

  for (int i = 0; i < 10; ++i) {
    subNet = clock->addSubNet(name);
  }

  clock->forEachSubNet([&instName](const Clock::SubNet& subNet) {
    instName.push_back(subNet.getName());
  });
  EXPECT_EQ(instName.size(), 10);
  for (unsigned int i = 0; i < instName.size(); ++i) {
    EXPECT_EQ(instName[i], name + "_" + clock->getName());
  }

  clock->forEachSubNet([&leafs](Clock::SubNet& subNet) {
    subNet.setLeafLevel(true);
    leafs.emplace_back(subNet.isLeafLevel());
  });
  for (unsigned int i = 0; i < leafs.size(); ++i) {
    EXPECT_TRUE(leafs[i]);
  }
}

template <class T>
void expect_eq_box(Box<T> box,
                   double MaxX,
                   double MaxY,
                   double MinX,
                   double MinY,
                   double factor)
{
  EXPECT_EQ(box.getMaxX(), MaxX * factor);
  EXPECT_EQ(box.getMaxY(), MaxY * factor);
  EXPECT_EQ(box.getMinX(), MinX * factor);
  EXPECT_EQ(box.getMinY(), MinY * factor);
}

TEST_F(TritonCTS_clock, clock_sink_all_function__compute_region_and_iteration)
{
  const std::string name = "name";
  DBU x = 666;
  DBU y = 888;
  auto pinObj = std::make_shared<odb::dbITerm>();
  float inputCap = 8.8f;
  double factor = 6.6f;
  Box<DBU> box;
  Box<double> cluBox;
  auto logger = std::make_shared<utl::Logger>();
  std::vector<std::pair<float, float>> cluster;
  std::vector<double> cap;
  std::vector<odb::dbITerm*> pin;

  // basic functions
  clock->addSink(name, x, y);
  clock->addSink(name, x + 1, y + 1, pinObj.get(), inputCap);
  EXPECT_EQ(clock->getNumSinks(), 2);

  ASSERT_NO_THROW(clock->report(logger.get()));

  // compute region with odb::Box<T>
  box = clock->computeSinkRegion();
  expect_eq_box(box, x + 1, y + 1, x, y, 1);

  cluster.push_back({x, y});
  cluster.push_back({x + 1, y + 1});
  cluBox = clock->computeSinkRegionClustered(cluster);
  expect_eq_box(cluBox, x + 1, y + 1, x, y, 1);

  cluBox = clock->computeNormalizedSinkRegion(factor);
  expect_eq_box(cluBox, x + 1, y + 1, x, y, factor);

  // iterations
  clock->forEachSink([&cap, &pin](const ClockInst& inst) {
    cap.push_back(inst.getInputCap());
    pin.push_back(inst.getDbInputPin());
  });
  EXPECT_EQ(cap[0], 0.0f);
  EXPECT_EQ(cap[1], 8.8f);
  EXPECT_EQ(pin[0], nullptr);
  EXPECT_EQ(pin[1], pinObj.get());

  clock->forEachSink([&cap, &pin](ClockInst& inst) {
    cap.push_back(inst.getInputCap());
    pin.push_back(inst.getDbInputPin());
  });
  EXPECT_EQ(cap[2], 0.0f);
  EXPECT_EQ(cap[3], 8.8f);
  EXPECT_EQ(pin[2], nullptr);
  EXPECT_EQ(pin[3], pinObj.get());
}
#endif 