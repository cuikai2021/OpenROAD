
#include "eigs.h"

#include <iostream>

#include "octave/dColVector.h"
#include "octave/dSparse.h"
#include "octave/eigs-base.h"

namespace gpl {

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
         double sigma)
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

// scale eigen vector to locations
Eigen::VectorXf eigvec2locs(const ColumnVector& evec,
                        const int minLoc,
                        const int maxLoc)
{
  Eigen::VectorXf Xf;
  Xf.resize(evec.numel());
  {
    std::vector<double> vecX;
    vecX.reserve(evec.numel());
    for (octave_idx_type i = 0; i < evec.numel(); ++i) {
      vecX.push_back(evec(i));
    }
    auto minmax = std::minmax_element(vecX.begin(), vecX.end());
    auto min_val = *minmax.first;
    auto max_val = *minmax.second;
    auto scale = (maxLoc - minLoc) / (max_val - min_val);
    for (size_t i = 0; i < vecX.size(); ++i) {
      auto c = (vecX[i] - min_val) * scale + minLoc;
      Xf(i) = c;
    }
  }
  return Xf;
}

}  // namespace gpl