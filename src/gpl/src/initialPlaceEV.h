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

#pragma once

#include <set>

#include "gpl/initialPlace.h"
#include "octave/dMatrix.h"
#include "octave/dSparse.h"

class SparseMatrix;

namespace gpl {

class InitialPlaceEV : public InitialPlace
{
 public:
  InitialPlaceEV() {}
  InitialPlaceEV(InitialPlaceVars ipVars,
                 std::shared_ptr<PlacerBase> pb,
                 utl::Logger* logger);
  ~InitialPlaceEV() {}

  void run();

 private:
  std::vector<Instance*> placeInsts_;
  virtual const std::vector<Instance*>& placeInsts() const
  {
    return placeInsts_;
  }

  void setPlaceInsts(const std::vector<Instance*>& insts);
  void resetPlaceInsts();

  bool resolveUnconnectedGraph(const Triplets& edges);

  void initAdjGraph(Triplets& aList);

  void createSparseMatrix(size_t vertexNum,
                         const Triplets& edges,
                         SparseMatrix& graphL);

  struct Snapshot
  {
    int iter;
    double ratio;
    Matrix eig_vecs;
    SparseMatrix B;
  };
  void tuneVertexWeights(const SparseMatrix& graphL, Snapshot& snap);

  void updateEdgeWeights(const SparseMatrix& vertexWeights, Triplets& edges);

  void updateInstLocs(const Matrix& eig_vecs);

  void adsorbInsts(std::set<size_t>& fixedInstIDs);

  class IdSolver
  {
   public:
    // old IDs are in range [0, N-1];
    // assign new IDs to nodes after 'removed' some insts
    void init(size_t N, const std::set<size_t>& removed)
    {
      size_t newID = 0;
      for (size_t id = 0; id < N; ++id) {
        if (!removed.count(id)) {
          n2o[newID] = id;
          o2n[id] = newID;
          newID++;
        }
      }
    }
    size_t old2new(size_t id) const { return o2n.at(id); }
    size_t new2old(size_t id) const { return n2o.at(id); }

    size_t numNewIDs() const { return n2o.size(); }

   private:
    std::map<size_t, size_t> n2o;
    std::map<size_t, size_t> o2n;
  };

  void bicgInit(IdSolver& idSolver,
                const Triplets& aList,
                SMatrix& L,
                Eigen::VectorXf aB[2],
                Eigen::VectorXf aInstLocs[2]);

  float bicgSolve(const SMatrix& L,
                  const Eigen::VectorXf& b,
                  Eigen::VectorXf& instLocs);

  void updateInstLocs(const IdSolver& idSolver, Eigen::VectorXf aInstLocs[2]);

  void dumpMatrixAB(const Triplets& aList, const std::string& postfix) const;
  void dumpXYLocs(const std::string& filename) const;
};

}  // namespace gpl
