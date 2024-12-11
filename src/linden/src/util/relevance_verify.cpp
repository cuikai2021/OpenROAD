#include "relevance_verify.hpp"
#include "fassert.hpp"
#include <algorithm>
#include <cinttypes>
#include <iostream>

using namespace std;
using namespace linden::geom::dimension;

namespace linden {

namespace {

void invert(vector<size_t>& to, const vector<size_t>& from) {
     FASSERT(from.size() == to.size())
          (from.size())
          (to.size());
     for(size_t i = 0, sz = from.size(); i < sz; ++i) {
          to[from[i]] = i;
     }
}

template<class Key>
void init_by(vector<size_t>& out, size_t n, Key key_fn) {
     out.clear();
     for(size_t i = 0; i < n; ++i) {
          out.push_back(i);
     }
     std::sort(
          out.begin(),
          out.end(),
          [&](size_t a, size_t b) {
               return key_fn(a) < key_fn(b);
          }
     );
}

int64_t cell_size_by_index(
     const linden::db::Database& db,
     Dimension dim,
     size_t idx)
{
     auto const& fixed = db.fixed_insts();
     if (idx < fixed.size()) {
          return fixed[idx].get().rect().c()[dim];
     }
     idx -= fixed.size();
     auto const& moveable = db.moveable_insts();
     if (idx < moveable.size()) {
          return moveable[idx].get().rect().c()[dim];
     }
     idx -= moveable.size();
     auto const& fillers = db.fillers();
     FASSERT(idx < fillers.size())
          (idx)
          (fillers.size());
     return fillers[idx].get().rect().c()[dim];
}

} // namespace

RelevanceDetector::RelevanceDetector(const linden::db::Database& db)
:    n_(db.total_cells()),
     from_db_to_ordered_x_(db.total_cells()),
     from_db_to_ordered_y_(db.total_cells()),
     bit_(db.total_cells()),
     buffer_(db.total_cells())
{
     init_by(buffer_, n_, [&](size_t i) {return cell_size_by_index(db, X, i);});
     invert(from_db_to_ordered_x_, buffer_);

     init_by(buffer_, n_, [&](size_t i) {return cell_size_by_index(db, Y, i);});
     invert(from_db_to_ordered_y_, buffer_);
}

tuple<double, double> RelevanceDetector::update(const linden::db::Database& db) {
     if (db.total_cells() == 0) {
          return make_tuple(0, 0);
     }

     double res_x = compute(
          from_db_to_ordered_x_,
          buffer_,
          [&](size_t i) {return cell_size_by_index(db, X, i);});
     invert(from_db_to_ordered_x_, buffer_);

     double res_y = compute(
          from_db_to_ordered_y_,
          buffer_,
          [&](size_t i) {return cell_size_by_index(db, Y, i);});
     invert(from_db_to_ordered_y_, buffer_);

     return make_tuple(res_x, res_y);
}

double RelevanceDetector::compute(
     const vector<size_t>& prev,
     vector<size_t>& curr_inv,
     const function<int64_t(size_t)>& key_fn) noexcept
{
     init_by(curr_inv, n_, key_fn);

     bit_.clear();
     int64_t sum = 0;
     for(int64_t i = n_ - 1; i >= 0; --i) {
          size_t w = prev[curr_inv[i]];
          sum += bit_.sum(w);
          bit_.update(w, 1);
     }

     double res = 1.0 * sum / (n_ * (n_ - 1) / 2);
     return res;
}

} // namespace linden
