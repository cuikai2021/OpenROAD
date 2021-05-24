#include <clustering.h>
#include <itest.h>

#include "CtsOptions.h"
#include "tritoncts/TritonCTS.h"
#include "utl/Logger.h"

#define TEST_OK_FILES_PATH "utest/TritonCTS/test"
#define TEST_FILES_PATH TEST_OK_FILES_PATH "/results"

using namespace CKMeans;
using namespace cts;

TEST(TritonCTS_clustering_flop, constructor)
{
  const float x = 0.0f;
  const float y = 0.0f;
  unsigned idx = 4294967295;

  ASSERT_NO_THROW(auto fl = std::make_shared<flop>(x, y, idx));
}

class TritonCTS_clustering : public ::testing::Test
{
 public:
  inline std::vector<std::pair<float, float>> loadSinks(const std::string& csv)
  {
    std::ifstream csvfile(csv);
    std::string tmp = "";
    bool isFirst = true;
    std::pair<float, float> point;
    std::vector<std::pair<float, float>> sinks;

    // skip first line
    std::getline(csvfile, tmp);
    while (std::getline(csvfile, tmp)) {
      std::istringstream sin(tmp);
      while (std::getline(sin, tmp, ',')) {
        if (isFirst) {
          point.first = std::atof(tmp.c_str());
        } else {
          point.second = std::atof(tmp.c_str());
          sinks.push_back(point);
        }
        isFirst = !isFirst;
      }
    }
    return sinks;
  }

 protected:
  std::vector<std::pair<float, float>> sinks
      = loadSinks(TEST_OK_FILES_PATH "/old_faithful.csv");
  std::shared_ptr<clustering> cluster
      = std::make_shared<clustering>(sinks, xBranch, yBranch, logger.get());

  const unsigned CAP
      = (unsigned) (sinks.size() * 0.6f);  //_options->getClusteringCapacity()
  const unsigned MAX = 100;                // max iteration numbers
  const unsigned power = 10;
  const unsigned IDX = 0;  // index of this clustering
  std::vector<std::pair<float, float>> means;

  void SetUp() override
  {
    means.emplace_back(1.5f, 40.0f);
    means.emplace_back(5.0f, 90.0f);
  }

 private:
  float xBranch = 0.0f;
  float yBranch = 0.0f;
  std::shared_ptr<utl::Logger> logger = std::make_shared<utl::Logger>();
};

TEST_F(TritonCTS_clustering, basic_functions_and_plot)
{
  std::pair<float, float> loc1(0.0f, 0.0f);
  std::pair<float, float> loc2(-0.000001f, 999999.0f);
  auto flop = std::make_shared<CKMeans::flop>(2.5f, 80.0f, 0);
  int iter = 0;
  std::vector<std::vector<CKMeans::flop*>> plotClusters;
  std::vector<std::pair<float, float>> newMeans;
  std::string plotFile = "/plot_basic_ut";
  std::string fileName = plotFile + "__" + std::to_string(iter);

  EXPECT_FLOAT_EQ(cluster->calcDist(loc1, flop.get()), 82.5f);
  EXPECT_FLOAT_EQ(cluster->calcDist(loc2, flop.get()), 999921.500001f);

  EXPECT_FLOAT_EQ(cluster->calcDist(loc1, loc2), 999999.000001f);

  newMeans.emplace_back(4.25f, 80.0f);
  newMeans.emplace_back(2.15f, 55.0f);

  cluster->setPlotFileName(TEST_FILES_PATH + plotFile);
  cluster->plotClusters(plotClusters, newMeans, means, iter);
  EXPECT_FILE_EQ(TEST_FILES_PATH + fileName + ".py",
                 TEST_OK_FILES_PATH + fileName + ".ok");
}

TEST_F(TritonCTS_clustering, calculate_silhoute)
{
  float silh = cluster->calcSilh(means, CAP, 0);
  EXPECT_NEAR(silh, -0.25f, 0.01f);
}

TEST_F(TritonCTS_clustering, fix_segment)
{
  float set[4] = {21.4f, 99.0f, 50.0f, 80.0f};
  float targetDist = 43.666f;
  std::pair<float, float> fixedPoint(set[0], set[1]);
  std::pair<float, float> movablePoint(set[2], set[3]);

  cluster->fixSegment(fixedPoint, movablePoint, targetDist);
  EXPECT_FLOAT_EQ(cluster->calcDist(fixedPoint, movablePoint), targetDist);
  EXPECT_FLOAT_EQ(fixedPoint.first, set[0]);
  EXPECT_FLOAT_EQ(fixedPoint.second, set[1]);
}

TEST_F(TritonCTS_clustering, kmeans)
{
  // function void minCostFlow() is called in Kmeans()
  // minCostFlow()'s side effects are private, it can't be test individually
  float silh = cluster->Kmeans(means.size(), CAP, 0, means, MAX, power);
  EXPECT_GT(silh, 0.5f);
  EXPECT_NEAR(means[0].first, 2.8f, 0.1f);
  EXPECT_NEAR(means[0].second, 62.9, 0.1f);
  EXPECT_NEAR(means[1].first, 4.2f, 0.1f);
  EXPECT_NEAR(means[1].second, 80.0f, 0.1f);
}

TEST_F(TritonCTS_clustering, interative_kmeans)
{
  unsigned index = 0;
  std::vector<std::pair<float, float>> segMeans;
  std::pair<float, float> branchingPoint(4.5f, 4.5f);
  std::vector<std::vector<unsigned>> clustersGet;
  std::vector<std::pair<float, float>> result
      = loadSinks(TEST_OK_FILES_PATH "/cluster_result.csv");

  cluster->iterKmeans(MAX / 5, means.size(), CAP, 0, means, 5, 2);
  EXPECT_NEAR(means[0].first, 1.25f, 0.1f);
  EXPECT_NEAR(means[0].second, 25.5f, 0.1f);
  EXPECT_NEAR(means[1].first, 1.25f, 0.1f);
  EXPECT_NEAR(means[1].second, 25.5f, 0.1f);

  // fixSegmentLengths() can only be used after segmentLength been init
  for (int i = 0; i < 10; ++i) {
    segMeans.emplace_back((float) i + 0.0f, (float) i + 0.0f);
  }
  branchingPoint = std::make_pair(4.5f, 4.5f);
  cluster->fixSegmentLengths(segMeans);
  for (unsigned i = 0; i < 9; ++i) {
    switch (i) {
      case 0:
      case 2:
      case 4:
      case 5:
      case 7:
      case 9:
        EXPECT_FLOAT_EQ(segMeans[i].first, 13.375f);
        EXPECT_FLOAT_EQ(segMeans[i].second, 13.375f);
        break;

      default:
        EXPECT_FLOAT_EQ(segMeans[i].first, -13.375f);
        EXPECT_FLOAT_EQ(segMeans[i].second, -13.375f);
        break;
    }
  }

  cluster->getClusters(clustersGet);
  for (unsigned i = 0; i < clustersGet.size(); ++i) {
    for (unsigned j = 0; j < clustersGet[i].size(); ++j) {
      EXPECT_EQ(i, result[index].first);
      EXPECT_EQ(clustersGet[i][j], result[index].second);
      ++index;
    }
  }
}