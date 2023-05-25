#include "UnionFind.hpp"

UnionFind::UnionFind(int n) : parent_(n), count_(n)
{
  for (int i = 0; i < n; ++i) {
    parent_[i] = i;
  }
}

int UnionFind::find(int x)
{
  assert(x >= 0 && x < parent_.size());
  if (parent_[x] != x) {
    parent_[x] = find(parent_[x]);
  }
  return parent_[x];
}

void UnionFind::unite(int x, int y)
{
  int root_x = find(x);
  int root_y = find(y);
  if (root_x != root_y) {
    parent_[root_y] = root_x;
    count_--;
  }
}

int UnionFind::size() const
{
  return count_;
}

bool UnionFind::isSame(int x, int y)
{
  return find(x) == find(y);
}
