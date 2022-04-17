///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2018-2020, The Regents of the University of California
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#include "initialPlaceEV.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <queue>
#include <set>

#include "graphics.h"
#include "maxDensity.h"
#include "octave/dColVector.h"
#include "octave/eigs-base.h"
#include "odb/dbTypes.h"
#include "placerBase.h"
#include "utl/Logger.h"

using namespace maxDensity;

namespace gpl {
using namespace std;

namespace {
/*
the @var{k} eigenvalues closest to @var{sigma} are returned.
return:
  eig_vec: eigen vectors
  eig_val: eigen values
  nconv: number of Ritz values that satisfy convergence
*/
int eigs(Matrix& eig_vec,
         ColumnVector& eig_val,
         const SparseMatrix& A,
         const SparseMatrix& B,
         octave_idx_type k,
         double sigma = 0.0)
{
  assert(A.issymmetric());

  /*
  The number of @nospell{Lanczos} basis vectors to use.  More vectors will
  result in faster convergence, but a greater use of memory.  The optimal
  value of @code{p} is problem dependent and should be in the range
  @code{@var{k} + 1} to @var{n}.  The default value is @code{2 * @var{k}}.
  */
  octave_idx_type p = 2 * k;

  // return value, 0 means success
  octave_idx_type info = 0;

  /*
  ## @item permB
  ## The permutation vector of the Cholesky@tie{}factorization for @var{B} if
  ## @code{cholB} is true.  It is obtained by
  ## @code{[R, ~, permB] = chol (@var{B}, @qcode{"vector"})}.  The default is
  ## @code{1:@var{n}}.
  */
  ColumnVector permB;

  // ResidualVectors = A * Evectors - B * Evectors * Evalues
  ColumnVector resid;

  std::ostream& os = std::cout;
  // Defines the required convergence tolerance,
  // calculated as tol * norm (A)}.  The default is @code{eps}.
  double tol = 0;

  bool rvec = true;

  // If the generalized eigenvalue problem is being calculated, this flag
  // (true/false) specifies whether the @var{B} input represents
  // @code{chol (@var{B})} or simply the matrix @var{B}.  The default is false.
  bool cholB = false;

  // The level of diagnostic printout (0|1|2).  If @code{disp} is 0 then
  // diagnostics are disabled.  The default value is 0.
  int disp = 0;

  // The maximum number of iterations.  The default is 300.
  int maxit = 300;

  auto nconv = EigsRealSymmetricMatrixShift(A,
                                            sigma,
                                            k,
                                            p,
                                            info,
                                            eig_vec,
                                            eig_val,
                                            B,
                                            permB,
                                            resid,
                                            os,
                                            tol,
                                            rvec,
                                            cholB,
                                            disp,
                                            maxit);
  std::cout << "eig_val:\n" << eig_val << std::endl;
  return nconv;
}

bool isSourcePin(Pin* pin)
{
  if (pin->isITerm()) {
    return pin->dbITerm()->getIoType() != odb::dbIoType::INPUT;
  } else {
    return pin->dbBTerm()->getIoType() != odb::dbIoType::OUTPUT;
  }
}

Pin* getSourcePin(Net* net)
{
  auto pins = net->pins();
  for (auto& pin : pins) {
    if (isSourcePin(pin)) {
      return pin;
    }
  }
  assert(0 && "net must have a source pin");
  return nullptr;
}

void reposition(std::vector<double>& X)
{
  double sum_x = 0;
  for (auto x : X) {
    sum_x += x;
  }
  auto avg_x = sum_x / X.size();
  for (auto& x : X) {
    x -= avg_x;
  }
}

void normalize(SparseMatrix& B, octave_idx_type placeCnt)
{
  double sum = 0;
  for (octave_idx_type i = 0; i < placeCnt; ++i) {
    sum += B(i, i);
  }
  auto r = placeCnt / sum;
  for (octave_idx_type i = 0; i < placeCnt; ++i) {
    B(i, i) *= r;
  }
}

// update inst(cell) locations with eig_vec
void updateInstLoc(const Matrix& eig_vecs,
                   int dim,
                   int delta,
                   int centerLoc,
                   const octave_idx_type placeCnt,
                   Eigen::VectorXf& instLocVec)
{
  const auto K = eig_vecs.columns();
  assert(K == 4);
  const auto ev_idx = K - 2 - dim;
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

// find the connected components of a graph
std::vector<std::vector<size_t>> components(const InitialPlace::Triplets& edges,
                                            const size_t VERTEX_NUM)
{
  struct Node
  {
    size_t id;
    size_t groupId;
    Node* next;
    Node(size_t id_, size_t groupId_)
        : id(id_), groupId(groupId_), next(nullptr)
    {
    }
  };

  struct List
  {
    Node* head;
    Node* tail;
    size_t size;
    size_t id;
    List(Node* head_, Node* tail_, size_t size_, size_t id_)
        : head(head_), tail(tail_), size(size_), id(id_)
    {
    }
    void merge(List& ano)
    {
      assert(tail && tail->next == nullptr);
      tail->next = ano.head;
      tail = ano.tail;
      size += ano.size;
      for (auto p = ano.head; p != nullptr; p = p->next) {
        p->groupId = id;
      }
      ano.clear();
    }
    void clear()
    {
      head = tail = nullptr;
      size = 0;
      id = LONG_MAX;
    }
    void release()
    {
      for (auto p = head; p != nullptr;) {
        auto next = p->next;
        delete p;
        p = next;
      }
    }
  };

  std::vector<List> groups;
  groups.reserve(VERTEX_NUM);
  for (size_t i = 0; i < VERTEX_NUM; ++i) {
    size_t nodeId = i;
    size_t groupId = i;
    auto node = new Node(nodeId, groupId);
    groups.emplace_back(List(node, node, 1, groupId));
  }

  typedef std::pair<Node*, Node*> Edge;
  std::vector<Edge> nodeEdges;
  nodeEdges.reserve(edges.size());
  for (auto& e : edges) {
    auto n0 = groups[e.row()].head;
    auto n1 = groups[e.col()].head;
    nodeEdges.emplace_back(std::make_pair(n0, n1));
  }

  for (auto& e : nodeEdges) {
    auto n0 = e.first;
    auto n1 = e.second;
    if (n0->groupId != n1->groupId) {
      auto& lst0 = groups[n0->groupId];
      auto& lst1 = groups[n1->groupId];
      if (lst0.size > lst1.size) {
        lst0.merge(lst1);
      } else {
        lst1.merge(lst0);
      }
    }
  }

  std::vector<std::vector<size_t>> res;
  for (auto const& g : groups) {
    if (g.size > 0) {
      std::vector<size_t> vec;
      vec.reserve(g.size);
      for (auto p = g.head; p != nullptr; p = p->next) {
        vec.push_back(p->id);
      }
      res.emplace_back(vec);
    }
  }
  
  for (auto& g : groups) {
    g.release();
  }
  return res;
}

}  // namespace

InitialPlaceEV::InitialPlaceEV(InitialPlaceVars ipVars,
                               std::shared_ptr<PlacerBase> pb,
                               utl::Logger* logger)
    : InitialPlace(ipVars, pb, logger)
{
  setPlaceInsts(pb_->placeInsts());
}

void InitialPlaceEV::run()
{
  Triplets aList;
  initAdjGraph(aList);

  if (resolveUnconnectedGraph(aList)) {
    initAdjGraph(aList);
  }

  SparseMatrix graphL;
  initLaplaceMatrix(graphL, placeInstsNum(), aList);

#ifndef NDEBUG
  dumpMatrixAB(aList, "initial.mtx");
#endif
  placeInstsCenter();

  Snapshot snap;
  tuneVertexWeights(graphL, snap);

#ifndef NDEBUG
  dumpMatrixAB(aList, "final.mtx");
  dumpXYLocs("XY.locs.mtx");
#endif

  {  // placement with some cells fixed to RULD borders

    updateEdgeWeights(snap.B, aList);

    /*
    Our objective function is the quadratic wirelenth:
    QWL == xLx
    Assume x=(y,f) where f are fixed cells
    QWL == yL’y + fBy + fDf
    L’ can be got by deleting all the fixed cell rows and columns.
    b = fB is a vector, where for each y_i, b_i = -\sum w_{ij}*f_j
    */
    SMatrix L;
    Eigen::VectorXf aB[DIMENSION_CNT];
    Eigen::VectorXf aInstLocs[DIMENSION_CNT];
    IdSolver idSolver;
    bicgInit(idSolver, aList, L, aB, aInstLocs);

    float errors[DIMENSION_CNT] = {0.0f, 0.0f};
    std::unique_ptr<Graphics> graphics;
    if (ipVars_.debug && Graphics::guiActive()) {
      graphics = make_unique<Graphics>(log_, pb_);
    }
    for (size_t i = 1; i <= ipVars_.maxIter; i++) {
      for (auto dim : {X, Y}) {
        errors[dim] = bicgSolve(L, aB[dim], aInstLocs[dim]);
      }
      updateInstLocs(idSolver, aInstLocs);
      if (graphics) {
        graphics->cellPlot(true);
      }
      log_->report("[InitialPlaceEVBicg] Iter: {} CG Error: {:0.8f} HPWL: {}",
                   i,
                   max(errors[X], errors[Y]),
                   pb_->hpwl());
      if (max(errors[X], errors[Y]) <= 1e-5 && i >= 5) {
        break;
      }
    }
  }

  resetPlaceInsts();

#ifndef NDEBUG
  dumpMatrixAB(aList, "final.bicg.mtx");
  dumpXYLocs("XY.locs.bicg.mtx");
#endif
}

bool InitialPlaceEV::resolveUnconnectedGraph(const Triplets& edges)
{
  std::vector<std::vector<size_t>> comps = components(edges, placeInstsNum());
  if (comps.size() > 1) {  // unconnected graph
    const std::vector<size_t>* pMaxComp = nullptr;
    // get the max connected sub-graph
    for (auto const& g : comps) {
      if (pMaxComp == nullptr || g.size() > pMaxComp->size()) {
        pMaxComp = &g;
      }
    }
    std::vector<Instance*> insts;
    insts.reserve(pMaxComp->size());
    for (auto i : *pMaxComp) {
      insts.push_back(placeInsts_[i]);
    }
    setPlaceInsts(insts);
    return true;
  }
  return false;
}

void InitialPlaceEV::setPlaceInsts(const std::vector<Instance*>& insts)
{
  placeInsts_ = insts;
  setPlaceInstExtId();
}

void InitialPlaceEV::resetPlaceInsts()
{
  setPlaceInsts(pb_->placeInsts());
}

// matrix of the connectivity graph
void InitialPlaceEV::initAdjGraph(Triplets& aList)
{
  const size_t placeCnt = placeInstsNum();
  aList.clear();
  aList.reserve(2 * placeCnt);
  std::map<size_t, float> m;
  for (auto& net : pb_->nets()) {
    auto sigT = net->dbNet()->getSigType();
    if (sigT != odb::dbSigType::SIGNAL && sigT != odb::dbSigType::CLOCK
        && sigT != odb::dbSigType::RESET) {
      continue;
    }
    // skip for small nets.
    if (net->pins().size() <= 1) {
      continue;
    }

    // escape long time cals on huge fanout.
    if (net->pins().size() >= ipVars_.maxFanout) {
      // continue;
    }

    // foreach two pins in single nets.
    auto pin_from = getSourcePin(net);
    if (!pin_from->isPlaceInstConnected()) {
      continue;
    }

    for (auto& pin_to : net->pins()) {
      if (pin_to != pin_from && pin_to->isPlaceInstConnected()) {
        auto row = pin_from->instance()->extId();
        auto col = pin_to->instance()->extId();
        if (row > col) {
          std::swap(row, col);
        }
        auto k = row * placeCnt + col;
        ++m[k];
      }
    }
  }

  for (auto const& [k, v] : m) {
    auto row = k / placeCnt;
    auto col = k % placeCnt;
    aList.push_back(T(row, col, v));
  }
}

// construct the laplace matrix of the connectivity graph
void InitialPlaceEV::initLaplaceMatrix(SparseMatrix& graphL,
                                       size_t vertexNum,
                                       Triplets& edges)
{
  graphL = SparseMatrix(
      vertexNum, vertexNum, octave_idx_type(2 * edges.size() + vertexNum));
  for (size_t i = 0; i < vertexNum; ++i) {
    graphL(i, i) = 0;
  }
  for (auto& t : edges) {
    graphL(t.row(), t.col()) = -t.value();
    graphL(t.col(), t.row()) = -t.value();
    graphL(t.row(), t.row()) += t.value();
    graphL(t.col(), t.col()) += t.value();
  }
}

void InitialPlaceEV::tuneVertexWeights(const SparseMatrix& graphL,
                                       Snapshot& snap)
{
  const octave_idx_type placeCnt = placeInstsNum();
  SparseMatrix B(placeCnt, placeCnt, placeCnt);
  for (size_t i = 0; i < placeCnt; ++i) {
    B(i, i) = 1;
  }

  snap.ratio = placeCnt;

  std::unique_ptr<Graphics> graphics;
  if (ipVars_.debug && Graphics::guiActive()) {
    graphics = make_unique<Graphics>(log_, pb_);
  }

  const int MAX_ITER = 100;  // todo: make it configurable in runtime
  int iter = 1;
  const int K = 4;  // number of eig values to get
  do {
    Matrix eig_vecs;
    ColumnVector eig_vals;
    eigs(eig_vecs, eig_vals, graphL, B, K);

    int ev_min_idx = K - 2;  // the 1st smallest eigen value who is not 0 or nan
    for (; ev_min_idx >= 0; --ev_min_idx) {
      auto eval_min = eig_vals.elem(ev_min_idx);
      if (!std::isnan(eval_min) && std::abs(eval_min) > 1e-7) {
        break;
      }
    }
    if (ev_min_idx < 0) {
      log_->report("[InitialPlace/EVPlace] eigen value is too small: {}",
                   eig_vals.elem(0));
      break;
    }

    std::vector<Id2Val> vId2Val[2];
    double ratios[2];
    std::pair<size_t, size_t> results[2];
    for (int dim = 0; dim < gpl::DIMENSION_CNT; ++dim) {
      std::vector<Id2Val>& X = vId2Val[dim];
      auto ev_idx = ev_min_idx - dim;
      if (ev_idx < 0) {
        ev_idx = 0;
      }
      {  // init X with eig_vec
        auto ev = eig_vecs.column(ev_idx);
        X.reserve(ev.numel());
        for (size_t i = 0; i < ev.numel(); ++i) {
          X.push_back(Id2Val(i, ev(i)));
        }
        sort(X.begin(), X.end(), [](const Id2Val& lhs, const Id2Val& rhs) {
          return lhs.second < rhs.second;
        });
        {  // scale the values
          auto scale
              = std::sqrt(X.size()) / (getVal(X[X.size() - 1]) - getVal(X[0]));
          for (size_t i = 1; i < X.size(); ++i) {
            auto c = (getVal(X[i]) - getVal(X[0])) * scale;
            X[i].second = c;
          }
          X[0].second = 0;
        }
      }

      std::pair<size_t, size_t>& result = results[dim];
      max_density(result, X, 2 * std::sqrt(X.size()), X.size());

      auto avgDensity = slope(X, 0, X.size() - 1);
      auto maxDensity = slope(X, result.first, result.second);
      auto& r = ratios[dim];
      r = maxDensity / avgDensity;

      log_->report(
          "[InitialPlace/EVPlace] Iter: {}, dim={}, eig_val={:0.8}, "
          "d.max={:0.4f}, "
          "d.avg={:0.4f}, r(max/avg)={:0.4f},snap.ratio={:0.4f}, subset#={}, "
          "result=({}, {})",
          iter,
          dim == 0 ? 'X' : 'Y',
          eig_vals.elem(ev_idx),
          maxDensity,
          avgDensity,
          r,
          snap.ratio,
          result.second - result.first + 1,
          result.first,
          result.second);
    }

    auto target = ratios[0] > ratios[1] ? 0 : 1;
    auto r = ratios[target];
    if (r < snap.ratio) {
      snap.ratio = r;
      snap.iter = iter;
      snap.eig_vecs = eig_vecs;
      snap.B = B;
      updateInstLocs(snap.eig_vecs);
      log_->report(
          "[InitialPlace/EVPlace] Iter: {}, HPWL: {}", iter, pb_->hpwl());
      if (graphics) {
        graphics->cellPlot(true);
      }
    }
    if (snap.ratio < 1.2) {
      log_->report(
          "[InitialPlace/EVPlace] reach the goal ratio with max/avg={}",
          snap.ratio);
      break;
    }

    if (iter - snap.iter > 50) {
      log_->report(
          "[InitialPlace/EVPlace] no convergence in the latest 50 iterations");
      break;
    }

    auto const& result = results[target];
    for (size_t i = result.first; i < result.second; ++i) {
      auto const& X = vId2Val[target];
      size_t id = getId(X[i]);
      // B(id, id) *= std::sqrt(r);
      // B(id, id) *= 2;
      B(id, id) *= 1.618;
    }
    normalize(B, placeCnt);
  } while (iter++ < MAX_ITER);

  log_->report(
      "[InitialPlace/EVPlace] maxDensity: snapRatio={:0.4f}, snapIter={}, "
      "HPWL: {}",
      snap.ratio,
      snap.iter,
      pb_->hpwl());

  // dump matrix B for debug
  // std::ofstream oss("debug.vertex.weights.mtx");
  // oss << placeCnt << " " << placeCnt << " " << placeCnt << std::endl;
  // oss << snap.B << std::endl;
}

void InitialPlaceEV::updateEdgeWeights(const SparseMatrix& vertexWeights,
                                       Triplets& edges)
{
  auto const placeCnt = placeInstsNum();
  std::vector<double> b(placeCnt, 0);
  for (size_t i = 0; i < placeCnt; ++i) {
    b[i] = vertexWeights(i, i);
  }
  for (auto& e : edges) {
    auto val = e.value() / std::sqrt(b[e.row()] * b[e.col()]);
    e = T(e.row(), e.col(), val);
  }
}

void InitialPlaceEV::updateInstLocs(const Matrix& eig_vecs)
{
  const double scale
      = sqrt(double(pb_->placeInstsArea()) / pb_->die().coreArea());

  updateInstLoc(eig_vecs,
                X,
                scale * pb_->die().coreDx(),
                pb_->die().coreCx(),
                placeInstsNum(),
                instLocVec_[X]);
  updateInstLoc(eig_vecs,
                Y,
                scale * pb_->die().coreDy(),
                pb_->die().coreCy(),
                placeInstsNum(),
                instLocVec_[Y]);

  updateCoordi();
}

// dump the A|B matrix
void InitialPlaceEV::dumpMatrixAB(const Triplets& aList,
                                  const std::string& postfix) const
{
  const octave_idx_type placeCnt = placeInstsNum();
  std::string files[2] = {"./LB.X." + postfix, "./LB.Y." + postfix};
  for (auto dim : {gpl::X, gpl::Y}) {
    std::cout << "dumping Matrix " << files[dim] << std::endl;
    std::ofstream oss(files[dim]);
    oss << placeCnt << " " << placeCnt + 1 << " " << aList.size() * 2 + placeCnt
        << std::endl;
    {
      std::vector<double> vec(placeCnt);
      auto const& insts = placeInsts();
      for (size_t i = 0; i < placeCnt; ++i) {
        vec[i] = insts[i]->c((gpl::DIMENSION) dim);
      }
      reposition(vec);

      // B[i] = Σ(Cij * (Xi - Xj) / Xi,
      // where j are the nodes connected with node i
      // Xi(Xj) is the coordindate value of node i(j)
      std::vector<double> B(placeCnt, 0);
      for (auto const& t : aList) {
        auto i = t.row();
        auto j = t.col();
        auto c = t.value();
        B[i] += c * (vec[i] - vec[j]) / vec[i];
        B[j] += c * (vec[j] - vec[i]) / vec[j];
      }

#if 0
      // To tune B[i] to make it gt 0.
      // Given B[i] = Σ(Cij * (Xi - Xj) / Xi,
      // Assume Xi < 0, then we have two options to tune B[i] from negative to
      // be positive:
      //   option1: change Xi' = Xi + delta to be greater than 0, like 1>0
      //   option2: change Σ(Cij * (Xi - Xj) to be less than 0, like -1 < 0
      //            Xi' = Xi + delta = (-1 + Σ(Cij * Xj)) / Σ(Cij)

      std::vector<double> sum_c(placeCnt, 0);
      std::vector<double> sum_cx(placeCnt, 0);
      for (auto const& t : aList) {
        auto i = t.row();
        auto j = t.col();
        auto c = t.value();
        sum_c[i] += c;
        sum_c[j] += c;
        sum_cx[i] += c * vec[j];
        sum_cx[j] += c * vec[i];
      }

      std::vector<double> Deltas(placeCnt);
      for (size_t i = 0; i < placeCnt; ++i) {
        if (B[i] > 0) {
          continue;
        }
        double optTargets[2] = {1, -1};
        if (vec[i] > 0) {
          optTargets[0] = -optTargets[0];
          optTargets[1] = -optTargets[1];
        }
        double deltas[2] = {0, 0};
        deltas[0] = optTargets[0] - vec[i];
        deltas[1] = ((optTargets[1] + sum_cx[i]) / sum_c[i]) - vec[i];

        auto delta = 0.0;
        if (std::abs(deltas[0]) < std::abs(deltas[1])) {
          delta = deltas[0];
        } else {
          delta = deltas[1];
        }
        Deltas[i] = std::abs(delta);
        printf("i:%-5d, X=%-8.2f, delta=%-8.2f, delta0=%-8.2f, delta1=%-8.2f, sum_cxj=%-8.2f, sum_c=%-4.0f\n", 
                i, vec[i], delta, deltas[0], deltas[1], sum_cx[i], sum_c[i]);
        auto xi = vec[i] + delta;
        B[i] = (sum_c[i] * xi - sum_cx[i]) / xi;
        assert(B[i] > 0 && "B[i] should be >=0");
      }
      {
        auto minmax = std::minmax_element(Deltas.begin(), Deltas.end());
        double sum = std::accumulate(Deltas.begin(), Deltas.end(), 0);
        double avg = sum / Deltas.size();
        std::cout << "Delta: min=" << *minmax.first << ", max=" << *minmax.second << ", avg=" << avg << std::endl;
      }
      {
        auto minmax = std::minmax_element(vec.begin(), vec.end());
        double sum = std::accumulate(vec.begin(), vec.end(), 0);
        double avg = sum / vec.size();
        std::cout << "X: min=" << *minmax.first << ", max=" << *minmax.second  << ", avg=" << avg << std::endl;
      }
#endif

      for (size_t i = 0; i < placeCnt; ++i) {
        oss << i + 1 << " " << placeCnt + 1 << " " << B[i] << std::endl;
      }
    }
    for (auto const& t : aList) {
      auto i = t.row();
      auto j = t.col();
      auto c = t.value();
      oss << i + 1 << " " << j + 1 << " " << c << std::endl;
      oss << j + 1 << " " << i + 1 << " " << c << std::endl;
    }
  }
}

void InitialPlaceEV::dumpXYLocs(const std::string& filename) const
{
  const octave_idx_type placeCnt = placeInstsNum();
  std::ofstream oss(filename);
  oss << placeCnt << " " << 2 << " " << 2 * placeCnt << std::endl;
  for (auto dim : {gpl::X, gpl::Y}) {
    std::vector<double> vec(placeCnt);
    auto const& insts = placeInsts();
    for (size_t i = 0; i < placeCnt; ++i) {
      vec[i] = insts[i]->c((gpl::DIMENSION) dim);
    }
    reposition(vec);
    for (size_t i = 0; i < placeCnt; ++i) {
      oss << i + 1 << " " << dim + 1 << " " << vec[i] << std::endl;
    }
  }
}

}  // namespace gpl
