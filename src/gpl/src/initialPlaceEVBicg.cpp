#include <Eigen/IterativeLinearSolvers>

#include "gpl/graphics.h"
#include "initialPlaceEV.h"
#include "gpl/placerBase.h"
#include "plot.h"
#include "utl/Logger.h"

namespace gpl {
using namespace std;

using Eigen::BiCGSTAB;
using Eigen::IdentityPreconditioner;
using utl::GPL;

void InitialPlaceEV::bicgInit(IdSolver& idSolver,
                              const std::vector<T>& aList,
                              SMatrix& L,
                              Eigen::VectorXf aB[2],
                              Eigen::VectorXf aInstLocs[2])
{
  std::set<size_t> fixed;
  adsorbInsts(fixed);

  idSolver.init(placeInstsNum(), fixed);

  // init matrix L
  auto const N = idSolver.numNewIDs();
  L.resize(N, N);
  std::vector<T> vecT;
  vecT.reserve(4 * aList.size());
  for (auto e : aList) {
    auto i = e.row();
    auto j = e.col();
    auto val = e.value();
    if (!fixed.count(i) && !fixed.count(j)) {
      i = idSolver.old2new(i);
      j = idSolver.old2new(j);
      vecT.push_back(T(i, j, -val));
      vecT.push_back(T(j, i, -val));
      vecT.push_back(T(i, i, val));
      vecT.push_back(T(j, j, val));
    }
  }
  for (auto e : aList) {
    for (auto j : {e.row(), e.col()}) {
      if (fixed.count(j)) {
        auto i = j == e.row() ? e.col() : e.row();
        if (!fixed.count(i)) {
          i = idSolver.old2new(i);
          vecT.push_back(T(i, i, e.value()));
        }
      }
    }
  }
  L.setFromTriplets(vecT.begin(), vecT.end());

  // init vector b
  for (auto const dim : {X, Y}) {
    auto& b = aB[dim];
    b.resize(N);
    for (size_t i = 0; i < N; ++i) {
      b(i) = 0;
    }
    for (auto e : aList) {
      auto w_ij = e.value();
      for (auto j : {e.row(), e.col()}) {
        if (fixed.count(j)) {
          auto f_j = placeInsts()[j]->c(dim);
          auto i = j == e.row() ? e.col() : e.row();
          if (!fixed.count(i)) {
            i = idSolver.old2new(i);
            b(i) = w_ij * f_j;
          }
        }
      }
    }
  }

  // init instLocs
  for (auto const dim : {X, Y}) {
    auto& instLocs = aInstLocs[dim];
    instLocs.resize(N);
    for (size_t id = 0; id < N; ++id) {
      instLocs(id) = placeInsts()[idSolver.new2old(id)]->c(dim);
    }
  }
}

float InitialPlaceEV::bicgSolve(const SMatrix& L,
                                const Eigen::VectorXf& b,
                                Eigen::VectorXf& instLocs)
{
  BiCGSTAB<SMatrix, IdentityPreconditioner> solver;
  solver.setMaxIterations(ipVars_.maxSolverIter);
  solver.compute(L);
  instLocs = solver.solveWithGuess(b, instLocs);
  return solver.error();
}

void InitialPlaceEV::updateInstLocs(const IdSolver& idSolver,
                                    Eigen::VectorXf aInstLocs[2])
{
  auto const N = idSolver.numNewIDs();
  for (size_t id = 0; id < N; ++id) {
    auto oid = idSolver.new2old(id);
    auto inst = placeInsts()[oid];
    inst->dbSetCenterLocation(aInstLocs[X](id), aInstLocs[Y](id));
  }
}

void InitialPlaceEV::adsorbInsts(std::set<size_t>& fixedInstIDs)
{
  class InstCmp
  {
   public:
    InstCmp(DIMENSION d0, bool reverse_order_on_d1)
        : d0_(d0), reverse_order_on_d1_(reverse_order_on_d1)
    {
    }

    static bool ne(size_t v0, size_t v1)
    {
      const int N = 1000;
      return (v0 / N) != (v1 / N);
    }

    bool operator()(Instance* inst0, Instance* inst1)
    {
      const DIMENSION d0 = d0_;
      const DIMENSION d1 = (DIMENSION) ((d0_ + 1) % 2);
      if (ne(inst0->c(d0), inst1->c(d0))) {
        return inst0->c(d0) < inst1->c(d0);
      } else {
        if (reverse_order_on_d1_) {
          return inst0->c(d1) > inst1->c(d1);
        } else {
          return inst0->c(d1) < inst1->c(d1);
        }
      }
    }

   private:
    DIMENSION d0_;
    bool reverse_order_on_d1_;
  };

  typedef std::vector<Instance*> InstVec;
  InstVec insts = placeInsts();
  InstVec rect[2][2];

  for (auto d0 : {X, Y}) {
    for (auto reverse_order_on_d1 : {0, 1}) {
      std::sort(insts.begin(), insts.end(), InstCmp(d0, reverse_order_on_d1));
      auto& res = rect[d0][reverse_order_on_d1];
      res.push_back(insts[0]);
      for (size_t i = 1; i < insts.size(); ++i) {
        if (InstCmp::ne(insts[i]->c(d0), insts[i - 1]->c(d0))) {
          res.push_back(insts[i]);
        }
      }

      const DIMENSION d1 = (DIMENSION) ((d0 + 1) % 2);
      int minmax = insts[0]->c(d1);
      for (auto inst : res) {
        auto v = inst->c(d1);
        if (reverse_order_on_d1) {
          if (v > minmax) {
            minmax = v;
          }
        } else {
          if (v < minmax) {
            minmax = v;
          }
        }
      }

      for (auto inst : res) {
        auto cx = d1 == X ? minmax : inst->c(X);
        auto cy = d1 == Y ? minmax : inst->c(Y);
        inst->setCenterLocation(cx, cy);
      }
#if 0
      std::cout << "/// dim=" << d0
                << ", reverse_order_on_d1=" << reverse_order_on_d1 << std::endl;
      for (auto inst : res) {
        std::cout << inst->extId() << ":(" << inst->c(X) << "," << inst->c(Y)
                  << ")" << std::endl;
      }
#endif
    }
  }

  for (auto i : {0, 1}) {
    for (auto j : {0, 1}) {
      auto const& insts = rect[i][j];
      for (auto inst : insts) {
        fixedInstIDs.insert(inst->extId());
      }
    }
  }
}

}  // namespace gpl