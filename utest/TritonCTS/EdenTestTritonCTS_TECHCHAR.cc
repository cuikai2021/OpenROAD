#include <TechChar.h>
#include <itest.h>
#include <tritoncts/TritonCTS.h>

#include <db_sta/dbSta.hh>

#define TEST_OK_FILES_PATH "utest/TritonCTS/test"
#define TEST_FILES_PATH TEST_OK_FILES_PATH"/results"

using namespace cts;

class TritonCTS_WireSegment : public ::testing::Test
{
 protected:
  uint8_t length = 0;
  uint8_t load = 1;
  uint8_t outputSlew = 2;
  double power = 3.0f;
  unsigned delay = 4;
  uint8_t inputCap = 5;
  uint8_t inputSlew = 6;
  WireSegment segment = WireSegment(length,
                                    load,
                                    outputSlew,
                                    power,
                                    delay,
                                    inputCap,
                                    inputSlew);
};

TEST_F(TritonCTS_WireSegment, basic_functions)
{
  EXPECT_EQ(segment.getPower(), power);
  EXPECT_EQ(segment.getDelay(), delay);
  EXPECT_EQ(segment.getInputCap(), inputCap);
  EXPECT_EQ(segment.getInputSlew(), inputSlew);
  EXPECT_EQ(segment.getLength(), length);
  EXPECT_EQ(segment.getLoad(), load);
  EXPECT_EQ(segment.getOutputSlew(), outputSlew);
}

TEST_F(TritonCTS_WireSegment, buffers)
{
  EXPECT_FALSE(segment.isBuffered());
  for (double i = 0.0f; i < 10.0f; i += 1.0f) {
    segment.addBuffer(i);
    segment.addBufferMaster("wire" + std::to_string(i));
  }
  EXPECT_TRUE(segment.isBuffered());

  EXPECT_EQ(segment.getNumBuffers(), 10);

  for (double i = 0.0f; i < 10.0f; i += 1.0f) {
    EXPECT_FLOAT_EQ(segment.getBufferLocations().at((int) i), i);
    EXPECT_FLOAT_EQ(segment.getBufferLocation((int) i), i);
    EXPECT_EQ(segment.getBufferMasters().at((int) i),
              "wire" + std::to_string(i));
    EXPECT_EQ(segment.getBufferMaster((int) i), "wire" + std::to_string(i));
  }
}

class TritonCTS_TechChar : public ::testing::Test
{
 public:
  constexpr static const char* logName = TEST_FILES_PATH"/tech_char.log";
  constexpr static const char* writeName
      = TEST_FILES_PATH"/tech_char_write.log";
  constexpr static const char* writeSolName
      = TEST_FILES_PATH"/tech_char_write_sol.log";
  constexpr static const char* logOk = TEST_OK_FILES_PATH"/tech_char.ok";
  constexpr static const char* writeOk = TEST_OK_FILES_PATH"//tech_char_write.ok";
  constexpr static const char* writeSolOk = TEST_OK_FILES_PATH"/tech_char_write_sol.ok";
  

 protected:
  TechChar tech = TechChar(&options, &logger);
  CtsOptions options = CtsOptions();
  utl::Logger logger = utl::Logger(logName);

  static void SetUpTestSuite()
  {
    remove(logName);
    remove(writeName);
    remove(writeSolName);
  }
};

TEST_F(TritonCTS_TechChar, wire_segment)
{
  unsigned key = tech.computeKey(0, 0, 0);
  std::vector<unsigned> getInputCap;
  std::vector<unsigned> getDelay;

  ASSERT_EQ(key, 0);

  for (uint8_t i = 0; i < 10; ++i) {
    tech.createWireSegment(i % 2, i % 2, i % 2, 1.0f, (unsigned) i, i, i);
  }
  for (uint8_t i = 0; i < 10; ++i) {
    EXPECT_EQ(tech.getWireSegment(i).getInputCap(), (uint8_t) i);
  }

  tech.forEachWireSegment(
      [&getInputCap](unsigned idx, const WireSegment& wire) {
        getInputCap.push_back(idx - (unsigned) wire.getInputCap());
      });
  for (unsigned i = 0; i < getInputCap.size(); ++i) {
    EXPECT_EQ(getInputCap[i], 0);
  }

  tech.forEachWireSegment(
      0, 0, 0, [&getDelay](unsigned idx, const WireSegment& wire) {
        getDelay.push_back(idx * 2 - (unsigned) wire.getInputCap());
      });
  EXPECT_EQ(getDelay.size(), 5);
  for (unsigned i = 0; i < getDelay.size(); ++i) {
    EXPECT_EQ(getDelay[i], 2 * i);
  }

  // reporters
  ASSERT_NO_THROW(tech.report());
  ASSERT_NO_THROW(tech.reportSegment(key));
  ASSERT_NO_THROW(tech.reportSegments(0, 0, 0));
}

TEST_F(TritonCTS_TechChar, compile_lut)
{
  std::vector<TechChar::ResultData> results;

  for (float i = 0.0f; i < 10.0f; i += 1.0f) {
    TechChar::ResultData data = {i,
                                 i,
                                 i,
                                 i,
                                 i,
                                 i,
                                 i,
                                 i < 5.0f ? true : false,
                                 std::vector<std::string>()};
    results.push_back(data);
  }
  ASSERT_NO_THROW(tech.compileLut(results));
  EXPECT_EQ(tech.getLengthUnit(), 10);
  EXPECT_EQ(tech.getActualMinInputCap(), 0);
  EXPECT_EQ(tech.getMinSegmentLength(), 0);
  EXPECT_EQ(tech.getMaxSegmentLength(), 0);
}

TEST_F(TritonCTS_TechChar, create)
{
  unsigned length = 10;
  unsigned fakeLength = 15;  // must be different from `length`
  unsigned setValue = std::numeric_limits<unsigned>::max();
  const char* defName = "src/TritonCTS/test/check_buffers.def";
  const char* lefName = "src/TritonCTS/test/Nangate45/Nangate45.lef";
  const char* libName = "src/TritonCTS/test/Nangate45/Nangate45_typ.lib";

  EXPECT_EQ(tech.getLogger(), options.getLogger());

  // Setup the app with tcl
  auto* interp = Tcl_CreateInterp();  // no need to free it manually
  Tcl_Init(interp);
  // must init an openRoad object incase segment fault
  ord::OpenRoad* openRoad = ord::OpenRoad::openRoad();
  openRoad->init(interp);
  // read pdk incase runtime error
  openRoad->getSta()->readLiberty(
      libName, openRoad->getSta()->cmdCorner(), sta::MinMaxAll::all(), true);
  openRoad->readLef(lefName, "test", true, true);
  openRoad->readDef(defName, false, false, false);

  // give arguments necessary to tritonCTS
  // from regression source `check_buffer.tcl`
  Tcl_Eval(interp, "create_clock -period 5 clk");
  Tcl_Eval(interp, "set_wire_rc -clock -layer metal5");
  openRoad->getTritonCts()->getParms()->setRootBuffer("CLKBUF_X3");
  openRoad->getTritonCts()->setBufferList("CLKBUF_X3");
  openRoad->getTritonCts()->getParms()->setSinkBuffer("CLKBUF_X3");
  openRoad->getTritonCts()->getParms()->setWireSegmentUnit(20);
  openRoad->getTritonCts()->getParms()->setRunPostCtsOpt(false);
  openRoad->getTritonCts()->getParms()->setSinkClustering(true);
  openRoad->getTritonCts()->getParms()->setSimpleSegmentsEnabled(true);
  openRoad->getTritonCts()->getParms()->setBufferDistance(100);
  openRoad->getTritonCts()->getParms()->setSizeSinkClustering(10);
  openRoad->getTritonCts()->getParms()->setMaxDiameter(60);
  openRoad->getTritonCts()->getParms()->setNumStaticLayers(1);

  // test create() with techChar created in openRoad, but not the one we
  // prepared. It is because too much arguments input that only can be called by
  // openRoad.
  TechChar* otech = openRoad->getTritonCts()->getCharacterization();
  ASSERT_NO_THROW(otech->create());
  ASSERT_NO_THROW(otech->createFakeEntries(length, fakeLength));

  otech->write(writeName);
  otech->writeSol(writeSolName);

  EXPECT_EQ(otech->getMinSegmentLength(), 2);
  EXPECT_EQ(otech->getMaxSegmentLength(), 4);
  EXPECT_EQ(otech->getMaxCapacitance(), 34);
  EXPECT_EQ(otech->getMaxSlew(), 26);
  EXPECT_FLOAT_EQ(otech->getCharMaxCap(), 1.81885e-13f);
  EXPECT_FLOAT_EQ(otech->getCharMaxSlew(), 1.98535e-10f);

  EXPECT_EQ(otech->getActualMinInputCap(), 1);
  otech->setActualMinInputCap(setValue);
  EXPECT_EQ(otech->getActualMinInputCap(), setValue);

  EXPECT_EQ(otech->getLengthUnit(), 7);
  otech->setLenghthUnit(
      setValue);  // modify LENGTH_UNIT_MICRON but not _lengthUnit
  EXPECT_EQ(otech->getLengthUnit(), 7);

    // clear all data 
  openRoad->getDb()->clear();
}

TEST(TritonCTS_Total, file_equal)
{
  // segment wire
  EXPECT_FILE_EQ(TritonCTS_TechChar::logName, TritonCTS_TechChar::logOk);

  // tech char
  EXPECT_FILE_EQ(TritonCTS_TechChar::writeName,
                 TritonCTS_TechChar::writeOk);
  EXPECT_FILE_EQ(TritonCTS_TechChar::writeSolName,
                 TritonCTS_TechChar::writeSolOk);
}