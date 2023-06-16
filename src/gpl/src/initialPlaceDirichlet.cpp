#include "initialPlaceDirichlet.h"

#include <iostream>

#include "eigs.h"
#include "graphics.h"
#include "netModelB2B.h"
#include "octave/dMatrix.h"
#include "octave/dSparse.h"
#include "placerBase.h"
#include "plot.h"
#include "utl/Logger.h"

namespace gpl {

/*
minimize QW(X) = QW(X0) + QW(X1)
  - step1: solve AX=b to get vector X0
  - step2: solve Dirichlet Eigenvector A to get vector X1
  - step3: update location: X=X0+X1
*/
void InitialPlaceDirichlet::run()
{
  std::unique_ptr<Graphics> graphics;
  if (ipVars_.debug && Graphics::guiActive()) {
    graphics = std::make_unique<Graphics>(log_, pb_);
  }
  for (int iter = 0; iter < 1; ++iter) {
    InitialPlaceBicgstab::run();

    IsScanChainsIgnoredPin is_scan_chains_ignored_pin(pb_->db()->getChip()->getBlock()->getScanChains());
    IgnoredNets ignored_nets;
    NetModelB2B model(ipVars_.minDiffLength,
                      ipVars_.netWeightScale,
                      pb_->nets(),
                      placeInsts(),
                      is_scan_chains_ignored_pin,
                      ignored_nets);

    const octave_idx_type placeCnt = placeInstsNum();
    const int K = 4;  // number of eig values to get
    for (auto dim : {X, Y}) {
      auto const& triplets = model.getMatrixATriplets(dim);
      SparseMatrix A(
          placeCnt, placeCnt, octave_idx_type(2 * triplets.size() + placeCnt));
      {  // init matrix A from the net modeling result
        for (auto& t : triplets) {
          A(t.row(), t.col()) = 0;
        }
        for (auto& t : triplets) {
          A(t.row(), t.col()) += t.value();
        }
      }

      SparseMatrix B(placeCnt, placeCnt, placeCnt);
      for (size_t i = 0; i < placeCnt; ++i) {
        B(i, i) = 1;
      }
      Matrix eig_vecs;
      ColumnVector eig_vals;
      eigs(eig_vecs, eig_vals, A, B, K);
      updateInstLoc(eig_vecs, dim);
    }
    updateCoordi();

    log_->report(
        "[InitialPlace/Dirichlet] iter: {}, HPWL: {}", iter, pb_->hpwl());
    if (graphics) {
      graphics->cellPlot(true);
    }
  }
}

// update inst(cell) locations with eig_vec
void InitialPlaceDirichlet::updateInstLoc(const Matrix& eig_vecs, DIMENSION dim)
{
  auto die = pb_->die();

  const ColumnVector& evec = eig_vecs.column(eig_vecs.columns() - 1);

  auto minmax = minmaxLoc(dim);
  auto minLoc = minmax.first;
  auto maxLoc = minmax.second;
  Eigen::VectorXf Xf = gpl::eigvec2locs(evec, minLoc, maxLoc);

  auto const& X0 = instLocVec_[dim];
  auto const& X1 = Xf;

  // get the ratio to make  minLoc <= x0 + x1 * ratio <= maxLoc
  float ratio = 1.0;
  {
    for (size_t i = 0; i < placeInstsNum(); ++i) {
      auto const x0 = X0[i];
      auto const x1 = X1[i];
      auto loc = x0 + x1;
      if (loc > maxLoc || loc < minLoc) {
        assert(x1 != 0);
        auto minmax = loc > maxLoc ? maxLoc : minLoc;
        float r = (minmax - x0) / x1;
        if (r < ratio) {
          ratio = r;
        }
        assert(r > 0 && r < 1);
      }
    }
  }

  // x = x0 + x1 * ratio
  auto& X = instLocVec_[dim];
  for (size_t i = 0; i < placeInstsNum(); ++i) {
    auto const x0 = X0[i];
    auto const x1 = X1[i];
    X(i) = x0 + x1 * ratio;
  }
}

}  // namespace gpl