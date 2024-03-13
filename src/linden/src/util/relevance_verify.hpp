#pragma once

#include "linden/db/db.hpp"
#include <tuple>
#include <vector>
#include <functional>

namespace relevance {

// For Binary Index Tree data structure, refer to:
// https://www.geeksforgeeks.org/binary-indexed-tree-or-fenwick-tree-2
struct BinaryIndexTree {
    explicit BinaryIndexTree(size_t n)
    :   tree(n + 1)
    {}

    void clear() {
        size_t n = tree.size();
        tree.clear();
        tree.resize(n);
    }

    size_t lowbit(size_t x) {
        return x & -x;
    }

    void update(size_t i, int64_t x) {
        size_t j = i + 1;
        while (j < tree.size()) {
            tree[j] += x;
            j += lowbit(j);
        }
    }

    int64_t sum(size_t i) {
        size_t j = i + 1;
        int64_t res = 0;
        while (j > 0) {
            res += tree[j];
            j -= lowbit(j);
        }
        return res;
    }

private:
    std::vector<int64_t> tree;
};

class RelevanceDetector {
public:
     RelevanceDetector(const RelevanceDetector&) =default;
     RelevanceDetector(RelevanceDetector&&) =default;
     RelevanceDetector& operator=(const RelevanceDetector&) =default;
     RelevanceDetector& operator=(RelevanceDetector&&) =default;

     explicit RelevanceDetector(const linden::db::Database&);

     std::tuple<double, double> update(const linden::db::Database&);

private:
     double compute(
          const std::vector<size_t>& prev,
          std::vector<size_t>& curr_inv,
          const std::function<int64_t(size_t)>& key_fn) noexcept;

     size_t n_;
     std::vector<size_t> from_db_to_ordered_x_;
     std::vector<size_t> from_db_to_ordered_y_;

     BinaryIndexTree bit_;
     std::vector<size_t> buffer_;
};

}  // namespace relevance

