#include "initialPlaceEV.h"
#include "netModelStar.h"
#include "eigs.h"
#include "maxDensity.h"
#include "../../util/odb_util.hpp"
#include "linden/util/name.hpp"
#include "odb/dbTypes.h"
#include "fassert.hpp"
#include "envflag/envflag.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <limits>
#include <filesystem>

IMPORT_STR_FLAG(LINDEN_IP_DEBUG_DIR);

using namespace std;

namespace linden::ip::ev {

namespace {

class UnionFindSet {
public:
    explicit UnionFindSet(const vector<linden::db::Cell*>& insts) noexcept;

    void connect(const NameView& cell0_name, const NameView& cell1_name) noexcept;
    vector<linden::db::Cell*> max_connected_component() noexcept;

private:
    struct Node {
        struct Root {
            size_t size;
        };
        struct Nonroot {
            Node* parent;
        };

        explicit Node(linden::db::Cell*) noexcept;

        linden::db::Cell* cell_;
        optional<Root> root_;
        optional<Nonroot> nonroot_;
    };

    Node* find_root(size_t idx) noexcept;

private:
    vector<Node> nodes;
    unordered_map<Name, size_t, NameHash, std::equal_to<>> name_to_indices_;
};

UnionFindSet::UnionFindSet(const vector<linden::db::Cell*>& insts) noexcept
{
    nodes.reserve(insts.size());
    for(auto& cell: insts) {
        size_t idx = nodes.size();
        nodes.emplace_back(cell);
        name_to_indices_[cell->name()] = idx;
    }
}

UnionFindSet::Node::Node(linden::db::Cell* cell) noexcept
:   cell_(cell)
{
    root_ = Root{size: 1};
}

void UnionFindSet::connect(
    const NameView& cell0_name,
    const NameView& cell1_name) noexcept
{
    auto it0 = name_to_indices_.find(cell0_name);
    if (it0 == name_to_indices_.end()) {
        return;
    }
    auto it1 = name_to_indices_.find(cell1_name);
    if (it1 == name_to_indices_.end()) {
        return;
    }
    auto nd0 = find_root(it0->second);
    auto nd1 = find_root(it1->second);
    if (nd0 == nd1) {
        return;
    }
    FASSERT(nd0->root_);
    FASSERT(nd1->root_);
    if (nd0->root_->size < nd1->root_->size) {
        std::swap(nd0, nd1);
    }
    nd0->root_->size += nd1->root_->size;
    nd1->root_.reset();
    nd1->nonroot_ = Node::Nonroot{parent: nd0};
}

vector<linden::db::Cell*> UnionFindSet::max_connected_component() noexcept
{
    vector<linden::db::Cell*> res;
    res.reserve(nodes.size());
    size_t max_size_so_far = 0;
    Node* root_so_far = nullptr;
    for(size_t i = 0, sz = nodes.size(); i < sz; ++i) {
        auto root = find_root(i);
        FASSERT(root->root_);
        if (root == root_so_far) {
            res.push_back(nodes[i].cell_);
        } else if (root->root_->size > max_size_so_far) {
            res.clear();
            res.push_back(nodes[i].cell_);
            max_size_so_far = root->root_->size;
            root_so_far = root;
        } else {
            // intend to ignore
        }
    }
    return res;
}

UnionFindSet::Node* UnionFindSet::find_root(size_t idx) noexcept {
    deque<Node*> path;
    path.push_back(&nodes[idx]);
    for(;;) {
        Node* top = path.back();
        if (top->root_) {
            break;
        }
        FASSERT(top->nonroot_);
        path.push_back(top->nonroot_->parent);
    }
    Node* root = path.back();
    FASSERT(root->root_);
    path.pop_back();
    for(auto nd: path) {
        FASSERT(nd->nonroot_);
        nd->nonroot_->parent = root;
    }
    return root;
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
Eigen::VectorXf updateInstLoc(
    const Matrix& eig_vecs,
    int dim,
    const std::pair<int, int>& minmax)
{
    const auto K = eig_vecs.columns();
    FASSERT(K == 4)(K);
    const auto ev_idx = K - 1 - dim;
    const auto& evec = eig_vecs.column(ev_idx);
    return eigvec2locs(evec, minmax.first, minmax.second);
}

}  // namespace

InitialPlaceEV::InitialPlaceEV(linden::db::Database& ldb)
:   ldb_(ldb),
    placeable_insts_area_(0)
{
    build_placeable_insts();
}

void InitialPlaceEV::build_placeable_insts() {
    for (auto& x: ldb_.mut_moveable_insts()) {
        auto& cell = x.get();
        if (!cell.macro()) {
            placeable_insts_area_ += cell.rect().area();
            placeable_insts_.push_back(&cell);
        }
    }
    spdlog::debug("# of placeable insts: {}", placeable_insts_.size());
}

void InitialPlaceEV::run()
{
    refine_placeable_insts_to_max_connected_component();
    if (placeable_insts_.size() <= 1) {
        return;
    }
    FASSERT(placeable_insts_indices_.empty());
    for(size_t i = 0, sz = placeable_insts_.size(); i < sz; ++i) {
        placeable_insts_indices_[placeable_insts_[i]] = i;
    }

    Triplets aList;
    initAdjGraph(aList);
    SparseMatrix graphL;
    createSparseMatrix(placeable_insts_.size(), aList, graphL);
    dumpMatrixAB(aList, "initial.mtx");

    place_insts_to_core_center();

    Snapshot snap;
    tuneVertexWeights(graphL, snap);

    {
        // placement with some cells fixed to RULD borders
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
        const size_t MAX_ITER = 20;
        for (size_t i = 1; i <= MAX_ITER; i++) {
            for (auto dim : {X, Y}) {
                errors[dim] = bicgSolve(L, aB[dim], aInstLocs[dim]);
            }
            updateInstLocs(idSolver, aInstLocs);

            spdlog::debug(
                "[InitialPlaceEVBicg] Iter: {} CG Error: {:0.8f} HPWL: {}",
                i,
                max(errors[X], errors[Y]),
                hpwl(ldb_));
            if (max(errors[X], errors[Y]) <= 1e-5 && i >= 5) {
                break;
            }
        }
    }
}

void InitialPlaceEV::refine_placeable_insts_to_max_connected_component() noexcept {
    UnionFindSet ufs(placeable_insts_);
    auto cell_name = [](const linden::db::Pin& pin) -> optional<NameView> {
        auto cell = pin.cell();
        if (cell) {
            return cell->get().name().to_view();
        } else {
            return nullopt;
        }
    };
    const string EMPTY_STR;
    for(auto const& x: ldb_.nets()) {
        auto const& net = x.get();
        auto const& pins = net.pins();
        if (pins.size() <= 1) {
            continue;
        }
        Name cell0_name(EMPTY_STR);
        auto it = pins.begin();
        for(; it != pins.end(); ++it) {
            auto const& pin = it->get();
            auto c_name = cell_name(pin);
            if (c_name) {
                cell0_name = c_name->to_owned();
                break;
            }
        }
        for(; it != pins.end(); ++it) {
            auto const& pin = it->get();
            auto c_name = cell_name(pin);
            if (c_name) {
                ufs.connect(cell0_name.to_view(), *c_name);
            }
        }
    }
    auto mcc = ufs.max_connected_component();
    std::swap(mcc, placeable_insts_);
}

void InitialPlaceEV::place_insts_to_core_center() {
    auto center = ldb_.core_region().c();
    for (auto& cell: placeable_insts_) {
        cell->move_to(center);
    }
}

// matrix of the connectivity graph
void InitialPlaceEV::initAdjGraph(Triplets& aList)
{
    const float NET_WEIGHT_SCALE = 800;
    NetModelStar model(
        NET_WEIGHT_SCALE,
        ldb_.nets(),
        placeable_insts_,
        placeable_insts_indices_);
    aList = model.getMatrixATriplets(X);
}

// construct the laplace matrix of the connectivity graph
void InitialPlaceEV::createSparseMatrix(
    size_t vertexNum,
    const Triplets& edges,
    SparseMatrix& graphL)
{
    graphL = SparseMatrix(
        vertexNum, vertexNum, octave_idx_type(2 * edges.size() + vertexNum));
    for (size_t i = 0; i < vertexNum; ++i) {
        graphL(i, i) = 0;
    }
    for (auto& t : edges) {
        graphL(t.row(), t.col()) = 0;
    }
    for (auto& t : edges) {
        graphL(t.row(), t.col()) += t.value();
    }
}

void InitialPlaceEV::tuneVertexWeights(
    const SparseMatrix& graphL,
    Snapshot& snap)
{
    const octave_idx_type placeCnt = placeable_insts_.size();
    SparseMatrix B(placeCnt, placeCnt, placeCnt);
    for (size_t i = 0; i < placeCnt; ++i) {
        B(i, i) = 1;
    }

    snap.ratio = placeCnt;

    const int MAX_ITER = 100;  // todo: make it configurable in runtime
    int iter = 1;
    const int K = 4;  // number of eig values to get
    do {
        Matrix eig_vecs;
        ColumnVector eig_vals;
        eigs(eig_vecs, eig_vals, graphL, B, K);

        int ev_min_idx = K - 1;  // the 1st smallest eigen value who is not 0 or nan
        for (; ev_min_idx >= 0; --ev_min_idx) {
            auto eval_min = eig_vals.elem(ev_min_idx);
            if (!std::isnan(eval_min) && std::abs(eval_min) > 1e-7) {
                break;
            }
        }
        if (ev_min_idx < 0) {
            spdlog::info(
                "[InitialPlace/EVPlace] eigen value is too small: {}",
                eig_vals.elem(0));
            break;
        }

        std::vector<Id2Val> vId2Val[2];
        double ratios[2];
        std::pair<size_t, size_t> results[2];
        for (int dim = 0; dim < DIMENSION_CNT; ++dim) {
            std::vector<Id2Val>& X = vId2Val[dim];
            auto ev_idx = ev_min_idx - dim;
            if (ev_idx < 0) {
                ev_idx = 0;
            }
            {
                // init X with eig_vec
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

            spdlog::debug(
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
            spdlog::info(
                "[InitialPlace/EVPlace] Iter: {}, HPWL: {}", iter, hpwl(ldb_));
        }
        if (snap.ratio < 1.2) {
            spdlog::info(
                "[InitialPlace/EVPlace] reach the goal ratio with max/avg={}",
                snap.ratio);
            break;
        }

        if (iter - snap.iter > 50) {
            spdlog::info(
                "[InitialPlace/EVPlace] no convergence in the latest 50 "
                "iterations");
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

    spdlog::info(
        "[InitialPlace/EVPlace] maxDensity: snapRatio={:0.4f}, snapIter={}, "
        "HPWL: {}",
        snap.ratio,
        snap.iter,
        hpwl(ldb_));

    // dump matrix B for debug
    // std::ofstream oss("debug.vertex.weights.mtx");
    // oss << placeCnt << " " << placeCnt << " " << placeCnt << std::endl;
    // oss << snap.B << std::endl;
}

void InitialPlaceEV::updateEdgeWeights(const SparseMatrix& vertexWeights,
                                       Triplets& edges)
{
    auto const placeCnt = placeable_insts_.size();
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
    inst_loc_vec_[X] = updateInstLoc(eig_vecs, X, minmaxLoc(X));
    inst_loc_vec_[Y] = updateInstLoc(eig_vecs, Y, minmaxLoc(Y));

    updateCoordi();
}

pair<int, int> InitialPlaceEV::minmaxLoc(DIMENSION dim) const noexcept {
    std::pair<int, int> minmax;
    int& minLoc = minmax.first;
    int& maxLoc = minmax.second;
    auto die = ldb_.die_region();
    minLoc = die.ll()[dim];
    maxLoc = die.ur()[dim];

    const double scale = std::sqrt(placeable_insts_area_ / die.area());
    const int delta = (1 - scale) * (dim == X ? die.dx() : die.dy());
    minLoc += delta;
    maxLoc -= delta;

    minLoc = abs2normLoc(minLoc, dim);
    maxLoc = abs2normLoc(maxLoc, dim);
    return minmax;
}

float InitialPlaceEV::abs2normLoc(float loc, DIMENSION dim) const noexcept {
#if 0
    // disabled，因为normalize 到die的中心点之后，结果似乎并没有变好
    // 要和Harry进一步分析后再定是否要做这个normalize
    float delta = dim == X ? pb_->die().dieCx() :pb_->die().dieCy();
#else
    float delta = 0; // absolute location respect to the die's origin (0, 0)
#endif
    return loc + delta;
}

float InitialPlaceEV::norm2absLoc(float loc, DIMENSION dim) const noexcept {
#if 0
    // disabled，因为normalize 到die的中心点之后，结果似乎并没有变好
    // 要和Harry进一步分析后再定是否要做这个normalize
    float delta = dim == X ? pb_->die().dieCx() :pb_->die().dieCy();
#else
    float delta = 0; // absolute location respect to the die's origin (0, 0)
#endif
    return loc - delta;
}

void InitialPlaceEV::updateCoordi()
{
    for (auto& inst : placeable_insts_) {
        auto it = placeable_insts_indices_.find(inst);
        if (it == placeable_insts_indices_.end()) {
            continue;
        }
        size_t idx = it->second;
        int64_t x = norm2absLoc(inst_loc_vec_[X](idx), X);
        int64_t y = norm2absLoc(inst_loc_vec_[Y](idx), Y);
        inst->move_to(linden::geom::Point {x, y});
    }
}

namespace {

void reposition(std::vector<double>& xs)
{
    double sum_x = accumulate(xs.begin(), xs.end(), 0.0);
    auto avg_x = sum_x / xs.size();
    for (auto& x : xs) {
        x -= avg_x;
    }
}

} // namespace

void InitialPlaceEV::dumpMatrixAB(const Triplets& aList, const std::string& suffix) const {
    auto debug_flag = LINDEN_IP_DEBUG_DIR();
    if (!debug_flag) {
        return;
    }
    using namespace std::filesystem;
    path dir(*debug_flag);
    std::string filenames[2] = {dir / ("LB.X." + suffix), dir / ("LB.Y." + suffix)};
    for(auto dim: {X, Y}) {
        spdlog::info("dumping matrix to {}", filenames[dim]);
        ofstream oss(filenames[dim]);
        oss << placeable_insts_.size()
            << " " << placeable_insts_.size() + 1
            << " " << aList.size() * 2 + placeable_insts_.size()
            << endl;
        {
            vector<double> vec;
            vec.reserve(placeable_insts_.size());
            for(auto const& x: placeable_insts_) {
                vec.push_back(x->rect().c()[dim]);
            }
            reposition(vec);

            // B[i] = Σ(Cij * (Xi - Xj) / Xi,
            // where j are the nodes connected with node i
            // Xi(Xj) is the coordindate value of node i(j)
            std::vector<double> B(placeable_insts_.size(), 0);
            for (auto const& t : aList) {
                auto i = t.row();
                auto j = t.col();
                auto c = t.value();
                spdlog::warn("inspect on aList:\n"
                    "  row: {}\n"
                    "  col: {}\n"
                    "  c: {}\n"
                    "  Xi: {}\n"
                    "  Xj: {}",
                    i, j, c, vec[i], vec[j]);
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

            std::vector<double> sum_c(placeable_insts_.size(), 0);
            std::vector<double> sum_cx(placeable_insts_.size(), 0);
            for (auto const& t : aList) {
                auto i = t.row();
                auto j = t.col();
                auto c = t.value();
                sum_c[i] += c;
                sum_c[j] += c;
                sum_cx[i] += c * vec[j];
                sum_cx[j] += c * vec[i];
            }

            std::vector<double> Deltas(placeable_insts_.size());
            for (size_t i = 0; i < placeable_insts_.size(); ++i) {
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
                printf(
                    "i:%-5d, X=%-8.2f, delta=%-8.2f, delta0=%-8.2f, delta1=%-8.2f, sum_cxj=%-8.2f, sum_c=%-4.0f\n",
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

            for (size_t i = 0; i < placeable_insts_.size(); ++i) {
                oss << i + 1 << " " << placeable_insts_.size() + 1 << " " << B[i] << std::endl;
            }
        }
        oss << "aList" << endl;
        for (auto const& t: aList) {
            auto i = t.row();
            auto j = t.col();
            auto c = t.value();
            oss << i + 1 << " " << j + 1 << " " << c << endl;
            oss << j + 1 << " " << i + 1 << " " << c << endl;
        }
    }
}

}  // namespace linden::ip::ev
