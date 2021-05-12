#include <Clock.h>
#include <itest.h>

#include <memory>

using namespace cts;
using namespace std;

TEST(TritonCTS_clock_inst_construct, test0)
{
  const std::string name = "name";
  const std::string master = "master";
  InstType type = CLOCK_SINK;
  DBU x = 0;
  DBU y = 0;
  auto pos = std::make_shared<Point<DBU>>(x, y);
  auto pinObj = std::make_shared<odb::dbITerm>();
  auto inst = std::make_shared<odb::dbInst>();
  float inputCap = 0.0;

  auto clock_inst
      = std::make_shared<ClockInst>(name, master, type, x, y, nullptr, inputCap);

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
