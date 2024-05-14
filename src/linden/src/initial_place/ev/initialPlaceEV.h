#pragma once

#include "eigs.h"
#include "dimension.hpp"
#include "linden/db/db.hpp"
#include <octave/dMatrix.h>
#include <octave/dSparse.h>
#include <set>
#include <vector>
#include <unordered_map>
#include <utility>

class SparseMatrix;

namespace linden::ip::ev {

class InitialPlaceEV
{
public:
    explicit InitialPlaceEV(linden::db::Database& ldb);
    ~InitialPlaceEV() =default;

    void run();

private:
    linden::db::Database& ldb_;
    std::vector<linden::db::Cell*> placeable_insts_;
    double placeable_insts_area_;
    std::unordered_map<const linden::db::Cell*, size_t> placeable_insts_indices_;
    Eigen::VectorXf inst_loc_vec_[DIMENSION_CNT];

    void build_placeable_insts();

    void place_insts_to_core_center();
    void refine_placeable_insts_to_max_connected_component() noexcept;

    void initAdjGraph(Triplets& aList);
    void createSparseMatrix(
        size_t vertexNum,
        const Triplets& edges,
        SparseMatrix& graphL);

    struct Snapshot {
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

    void bicgInit(
        IdSolver& idSolver,
        const Triplets& aList,
        SMatrix& L,
        Eigen::VectorXf aB[2],
        Eigen::VectorXf aInstLocs[2]);

    float bicgSolve(
        const SMatrix& L,
        const Eigen::VectorXf& b,
        Eigen::VectorXf& instLocs);

    void updateInstLocs(const IdSolver& idSolver, Eigen::VectorXf aInstLocs[2]);

    std::pair<int, int> minmaxLoc(DIMENSION dim) const noexcept;
    float abs2normLoc(float loc, DIMENSION dim) const noexcept;
    float norm2absLoc(float loc, DIMENSION dim) const noexcept;
    void updateCoordi();

    void dumpMatrixAB(const Triplets& aList, const std::string& suffix) const;
    void dumpXYLocs(const std::string& filename) const;
};

}  // namespace linden::ip::ev
