#pragma once
#include <vector>
#include "assert.h"
class UnionFind
{
 public:
  UnionFind(int n);

  int find(int x);

  bool isSame(int x,int y);

  void unite(int x, int y);

  int size() const;

 private:
  std::vector<int> parent_;
  int count_;
};