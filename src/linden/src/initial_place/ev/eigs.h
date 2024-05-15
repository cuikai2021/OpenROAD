#pragma once

class Matrix;
class ColumnVector;
class SparseMatrix;

#include "octave/octave-config.h"
#include <Eigen/SparseCore>

namespace linden::ip::ev {

typedef Eigen::Triplet<float> T;
typedef std::vector<T> Triplets;
typedef Eigen::SparseMatrix<float, Eigen::RowMajor> SMatrix;

int eigs(Matrix& eig_vec,
         ColumnVector& eig_val,
         const SparseMatrix& A,
         const SparseMatrix& B,
         octave_idx_type k,
         double sigma = 0.0);

Eigen::VectorXf eigvec2locs(const ColumnVector& evec,
                        const int minLoc,
                        const int maxLoc);

}  // namespace linden::ip::ev
