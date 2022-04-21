#include "initialPlaceDirichlet.h"

#include "eigs.h"
#include "graphics.h"
#include "octave/dMatrix.h"
#include "octave/dSparse.h"
#include "placerBase.h"
#include "plot.h"
#include "utl/Logger.h"

namespace gpl {

void InitialPlaceDirichlet::run()
{
  std::unique_ptr<Graphics> graphics;
  if (ipVars_.debug && Graphics::guiActive()) {
    graphics = std::make_unique<Graphics>(log_, pb_);
  }
  for (int iter = 0; iter < 1; ++iter) {
    InitialPlaceBicgstab::run();
    const octave_idx_type placeCnt = placeInstsNum();
    const int K = 4;  // number of eig values to get
    for (auto dim : {X, Y}) {
      auto const& triplets = matrixATriplets_[dim];
      SparseMatrix A(
          placeCnt, placeCnt, octave_idx_type(2 * triplets.size() + placeCnt));
      {  // init matrix A from the B2B modeling net matrix
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
void InitialPlaceDirichlet::updateInstLoc(const Matrix& eig_vecs, int dim)
{
  auto die = pb_->die();
  const double scale = sqrt(double(pb_->placeInstsArea()) / die.coreArea());

  const ColumnVector& evec = eig_vecs.column(eig_vecs.columns() - 1);

  const octave_idx_type placeCnt = placeInstsNum();

  Eigen::VectorXf Xf;
  Xf.resize(placeCnt);
  {  // convert eigen-vector to coordinate on the die area
    auto delta = scale * (dim == X ? die.coreDx() : die.coreDy());
    auto centerLoc = dim == X ? die.coreCx() : die.coreCy();
    const int minLoc = centerLoc - (delta / 2);
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
      Xf(i) = c;
    }
  }

  auto const& X0 = instLocVec_[dim];
  auto const& X1 = Xf;

  // get the ratio to make x0 + x1 * ratio <= maxLoc
  float ratio = 1.0;
  {
    size_t maxLoc = dim == X ? die.coreUx() : die.coreUy();
    for (size_t i = 0; i < placeInstsNum(); ++i) {
      auto const x0 = X0[i];
      auto const x1 = X1[i];
      auto loc = x0 + x1;
      if (loc > maxLoc) {
        float r = (maxLoc - x0) / x1;
        if (r < ratio) {
          ratio = r;
        }
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