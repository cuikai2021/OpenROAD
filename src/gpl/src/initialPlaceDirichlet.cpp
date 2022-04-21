#include "initialPlaceDirichlet.h"

#include "eigs.h"
#include "graphics.h"
#include "octave/dMatrix.h"
#include "octave/dSparse.h"
#include "placerBase.h"
#include "plot.h"
#include "utl/Logger.h"

namespace gpl {

namespace {
// update inst(cell) locations with eig_vec
void updateInstLoc(const Matrix& eig_vecs,
                   int delta,
                   int centerLoc,
                   const octave_idx_type placeCnt,
                   Eigen::VectorXf& instLocVec)
{
  const auto K = eig_vecs.columns();
  assert(K == 4);
  const auto ev_idx = K - 1;
  const auto& evec = eig_vecs.column(ev_idx);
  const int minLoc = centerLoc - (delta / 2);
  instLocVec.resize(placeCnt);
  {
    std::vector<double> vecX;
    vecX.reserve(evec.numel());
    for (octave_idx_type i = 0; i < evec.numel(); ++i) {
      vecX.push_back(evec(i));
    }
    auto minmax = std::minmax_element(vecX.begin(), vecX.end());
    auto min_val = *minmax.first;
    auto max_val = *minmax.second;
    auto scale = delta / (max_val - min_val);
    for (size_t i = 0; i < vecX.size(); ++i) {
      auto c = (vecX[i] - min_val) * scale + minLoc;
      instLocVec(i) = c;
    }
  }
}

}  // namespace

void InitialPlaceDirichlet::run()
{
  InitialPlaceBicgstab::run();

  const octave_idx_type placeCnt = placeInstsNum();
  Matrix eig_vecs[DIMENSION_CNT];
  ColumnVector eig_vals[DIMENSION_CNT];
  const int K = 4;  // number of eig values to get
  for (auto dim : {X, Y}) {
    auto const& triplets = matrixATriplets_[dim];
    SparseMatrix A(
        placeCnt, placeCnt, octave_idx_type(2 * triplets.size() + placeCnt));
    for (auto& t : triplets) {
      A(t.row(), t.col()) = 0;
    }
    for (auto& t : triplets) {
      A(t.row(), t.col()) += t.value();
    }

    SparseMatrix B(placeCnt, placeCnt, placeCnt);
    for (size_t i = 0; i < placeCnt; ++i) {
      B(i, i) = 1;
    }
    eigs(eig_vecs[dim], eig_vals[dim], A, B, K);
  }
  updateInstLocs(eig_vecs);

  log_->report("[InitialPlace/Dirichlet] HPWL: {}", pb_->hpwl());

  std::unique_ptr<Graphics> graphics;
  if (ipVars_.debug && Graphics::guiActive()) {
    graphics = std::make_unique<Graphics>(log_, pb_);
  }
  if (graphics) {
    graphics->cellPlot(true);
  }
}

void InitialPlaceDirichlet::updateInstLocs(Matrix eig_vecs[2])
{
  const double scale
      = sqrt(double(pb_->placeInstsArea()) / pb_->die().coreArea());

  updateInstLoc(eig_vecs[X],
                scale * pb_->die().coreDx(),
                pb_->die().coreCx(),
                placeInstsNum(),
                instLocVec_[X]);
  updateInstLoc(eig_vecs[Y],
                scale * pb_->die().coreDy(),
                pb_->die().coreCy(),
                placeInstsNum(),
                instLocVec_[Y]);

  updateCoordi();
}

}  // namespace gpl