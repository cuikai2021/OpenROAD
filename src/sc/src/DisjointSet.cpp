#include "DisjointSet.h"

namespace sc {

DisjointSet::DisjointSet(int n) : father{}, rank{} {
  father.resize(n);
  rank.resize(n);
  for (int i = 0; i < n; i++) {
    father[i] = i;
    rank[i] = 1;
  }
}

int DisjointSet::findRoot(int x) {
  return father[x] == x ? x : father[x] = findRoot(father[x]);
}

void DisjointSet::merge(int x, int y) {
  int x_root = findRoot(x);
  int y_root = findRoot(y);
  if (x_root == y_root) {
    return;
  }
  if (rank[x_root] == rank[y_root]) {
    father[x_root] = y_root;
    rank[y_root]++;
  } else if (rank[x_root] > rank[y_root]) {
    father[y_root] = x_root;
  } else {
    father[x_root] = y_root;
  }
}

} // namespace sc