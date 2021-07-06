#ifndef PYBNESIAN_UTIL_VECH_OPS_HPP
#define PYBNESIAN_UTIL_VECH_OPS_HPP

#include <Eigen/Dense>

using Eigen::VectorXd, Eigen::MatrixXd;

namespace util {

VectorXd vech(const MatrixXd& m);
MatrixXd invvech(const VectorXd& m);
}  // namespace util

#endif  // PYBNESIAN_VECH_OPS_HPP