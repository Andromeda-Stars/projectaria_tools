/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "SO3PyBind.h"

#include <sophus/average.hpp>
#include <sophus/common.hpp>
#include <sophus/interpolate.hpp>
#include <sophus/se3.hpp>

#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace sophus {

// In python, we choose to export our Sophus::SE3 as a vector of SE3 objects by binding the cpp
// object `SE3Group` defined below.
template <typename Scalar>
class SE3Group : public std::vector<Sophus::SE3<Scalar>> {
 public:
  // The empty constructor is not accessible from python.
  // Python always create at least one identity element (like c++ sophus)
  SE3Group() = default;
  // implicit copy conversion from a Sophus::SE3 value
  SE3Group(const Sophus::SE3<Scalar>& in) {
    this->push_back(in);
  }
};
} // namespace sophus

// The following caster makes so that, even if we wrap SE3Group in python, those can be
// implicitly converted to the c++ Sophus::SE3 object at boundaries between languages.
// This is so we can pass python SE3 object to c++ function as if they were regular 1-element
// Sophus::SE3 object. This simplifies binding the rest of c++ code. This implicit cast fails if the
// python object is not a 1-element SE3 object.
// NOTE: this caster makes a copy, so can't not be used for passing a reference of a SE3 element to
// a c++ function.
namespace pybind11 {
namespace detail {
template <>
struct type_caster<Sophus::SE3<double>> {
 public:
  PYBIND11_TYPE_CASTER(Sophus::SE3<double>, _("SE3"));

  // converting from python -> c++ type
  bool load(handle src, bool convert) {
    try {
      sophus::SE3Group<double>& ref = src.cast<sophus::SE3Group<double>&>();
      if (ref.size() != 1) {
        throw std::domain_error(fmt::format(
            "A element of size 1 is required here. Input has {} elements.", ref.size()));
      }
      value = ref[0];
      return true;
    } catch (const pybind11::cast_error&) {
      return false; // Conversion failed
    }
  }

  // converting from c++ -> python type
  static handle cast(Sophus::SE3<double> src, return_value_policy policy, handle parent) {
    return type_caster_base<sophus::SE3Group<double>>::cast(sophus::SE3Group(src), policy, parent);
  }
};
} // namespace detail
} // namespace pybind11

namespace sophus {
/*SE3*/
template <typename Scalar>
using PybindSE3Type = pybind11::class_<SE3Group<Scalar>>;

template <typename Scalar>
PybindSE3Type<Scalar> exportSE3Transformation(
    pybind11::module& module,
    const std::string& name = "SE3") {
  PybindSE3Type<Scalar> type(module, name.c_str());

  type.def(
      pybind11::init([]() {
        SE3Group<Scalar> ret;
        ret.push_back({});
        return ret;
      }),
      " Default Constructor initializing a group containing 1 identity element");
  type.def(pybind11::init<const Sophus::SE3<Scalar>&>(), "Copy constructor from single element");

  type.def_static("from_matrix", [](const Eigen::Matrix<Scalar, 4, 4>& matrix) -> SE3Group<Scalar> {
    return SE3Group<Scalar>{Sophus::SE3<Scalar>::fitToSE3(matrix)};
  });
  type.def_static(
      "from_matrix",
      [](const std::vector<Eigen::Matrix<Scalar, 4, 4>>& matrices) -> SE3Group<Scalar> {
        SE3Group<Scalar> output;
        output.reserve(matrices.size());
        for (size_t i = 0; i < matrices.size(); ++i) {
          output.push_back(Sophus::SE3<Scalar>::fitToSE3(matrices[i]));
        }
        return output;
      });

  type.def_static(
      "from_matrix3x4", [](const Eigen::Matrix<Scalar, 3, 4>& matrix) -> SE3Group<Scalar> {
        return SE3Group<Scalar>{Sophus::SE3<Scalar>(
            Sophus::SO3<Scalar>::fitToSO3(matrix.template block<3, 3>(0, 0)),
            matrix.template block<3, 1>(0, 3))};
      });
  type.def_static(
      "from_matrix3x4",
      [](const std::vector<Eigen::Matrix<Scalar, 4, 4>>& matrices) -> SE3Group<Scalar> {
        SE3Group<Scalar> output;
        output.reserve(matrices.size());
        for (size_t i = 0; i < matrices.size(); ++i) {
          output.push_back(Sophus::SE3<Scalar>(
              Sophus::SO3<Scalar>::fitToSO3(matrices[i].template block<3, 3>(0, 0)),
              matrices[i].template block<3, 1>(0, 3)));
        }
        return output;
      });

  type.def_static(
      "exp",
      [](const Eigen::Matrix<Scalar, 3, 1>& translational_part,
         const Eigen::Matrix<Scalar, 3, 1>& rotvec) -> SE3Group<Scalar> {
        auto tangentVec = Eigen::Matrix<Scalar, 6, 1>{
            translational_part[0],
            translational_part[1],
            translational_part[2],
            rotvec[0],
            rotvec[1],
            rotvec[2]};
        return {Sophus::SE3<Scalar>::exp(tangentVec)};
      },
      "Create SE3 from a translational_part (3x1) and a rotation vector (3x1) of magnitude in rad. NOTE: translational_part is not translation vector in SE3");
  type.def_static(
      "exp",
      [](const Eigen::Matrix<Scalar, Eigen::Dynamic, 3>& translational_parts,
         const Eigen::Matrix<Scalar, Eigen::Dynamic, 3>& rotvecs) -> SE3Group<Scalar> {
        SE3Group<Scalar> output;
        output.reserve(rotvecs.rows());
        for (size_t i = 0; i < rotvecs.rows(); ++i) {
          auto tangentVec = Eigen::Matrix<Scalar, 6, 1>{
              translational_parts(i, 0),
              translational_parts(i, 1),
              translational_parts(i, 2),
              rotvecs(i, 0),
              rotvecs(i, 1),
              rotvecs(i, 2)};
          output.emplace_back(Sophus::SE3<Scalar>::exp(tangentVec));
        }
        return output;
      },
      "Create a set of SE3 from translational_parts (Nx3) and rotation vectors (Nx3) of magnitude in rad. NOTE: translational_part is not translation vector in SE3");

  type.def_static(
      "from_quat_and_translation",
      [](const Scalar& w,
         const Eigen::Matrix<Scalar, 3, 1>& xyz,
         const Eigen::Matrix<Scalar, 3, 1>& translation) -> SE3Group<Scalar> {
        Eigen::Matrix<Scalar, 4, 1> mat(w, xyz[0], xyz[1], xyz[2]);
        if (std::fabs(mat.norm() - 1.0) > Sophus::Constants<Scalar>::epsilon()) {
          throw std::runtime_error("The norm of the quaternion is not 1");
        }
        Eigen::Quaternion quat(mat[0], mat[1], mat[2], mat[3]);
        return {Sophus::SE3<Scalar>(quat, translation)};
      },
      "Create SE3 from a quaternion as w, [x, y, z], and translation vector");

  type.def_static(
      "from_quat_and_translation",
      [](const std::vector<Scalar>& x_vec,
         const Eigen::Matrix<Scalar, -1, 3>& xyz_vec,
         const Eigen::Matrix<Scalar, -1, 3>& translations) -> SE3Group<Scalar> {
        if (x_vec.size() != xyz_vec.rows() || x_vec.size() != translations.size()) {
          throw std::domain_error(fmt::format(
              "Size of the input variables are not the same: x_vec = {}, xyz_vec = {}, translation = {}",
              x_vec.size(),
              xyz_vec.rows(),
              translations.size()));
        }
        SE3Group<Scalar> output;
        output.reserve(x_vec.size());
        for (size_t i = 0; i < x_vec.size(); ++i) {
          Eigen::Matrix<Scalar, 4, 1> mat(x_vec[i], xyz_vec(i, 0), xyz_vec(i, 1), xyz_vec(i, 2));
          if (std::fabs(mat.norm() - 1.0) > Sophus::Constants<Scalar>::epsilon()) {
            throw std::runtime_error(
                fmt::format("The norm of the quaternion is not 1 for quaternion {}", mat));
          }
          Eigen::Quaternion quat(mat[0], mat[1], mat[2], mat[3]);
          output.push_back(Sophus::SE3<Scalar>(quat, translations.row(i)));
        }
        return output;
      },
      "Create SE3 from a list of quaternion as w_vec: Nx1, xyz_vec: Nx3, and a list of translation vectors: Nx3");

  type.def(
      "to_matrix3x4",
      [](const SE3Group<Scalar>& transformations) -> std::vector<Eigen::Matrix<Scalar, 3, 4>> {
        std::vector<Eigen::Matrix<Scalar, 3, 4>> output;
        output.reserve(transformations.size());

        for (size_t i = 0; i < transformations.size(); i++) {
          output.push_back(transformations[i].matrix3x4());
        }
        return output;
      },
      "Convert an array of SE3 into an array of transformation matrices of size 3x4");

  type.def(
      "to_matrix",
      [](const SE3Group<Scalar>& transformations) -> std::vector<Eigen::Matrix<Scalar, 4, 4>> {
        std::vector<Eigen::Matrix<Scalar, 4, 4>> output;
        output.reserve(transformations.size());

        for (size_t i = 0; i < transformations.size(); i++) {
          output.push_back(transformations[i].matrix());
        }
        return output;
      },
      "Convert an array of SE3 into an array of transformation matrices of size 4x4");

  type.def(
      "to_quat_and_translation",
      [](const SE3Group<Scalar>& transformations) -> Eigen::Matrix<Scalar, Eigen::Dynamic, 7> {
        auto output = Eigen::Matrix<Scalar, Eigen::Dynamic, 7>(transformations.size(), 7);
        for (size_t i = 0; i < transformations.size(); ++i) {
          output.row(i) = Eigen::Matrix<Scalar, 1, 7>{
              transformations[i].so3().unit_quaternion().w(),
              transformations[i].so3().unit_quaternion().x(),
              transformations[i].so3().unit_quaternion().y(),
              transformations[i].so3().unit_quaternion().z(),
              transformations[i].translation()[0],
              transformations[i].translation()[1],
              transformations[i].translation()[2]};
        }
        return output;
      },
      "Return quaternion and translation as Nx7 vectors of [quat (w, x, y, z), translation]");

  type.def(
      "log",
      [](const SE3Group<Scalar>& transformations) -> Eigen::Matrix<Scalar, Eigen::Dynamic, 6> {
        auto output = Eigen::Matrix<Scalar, Eigen::Dynamic, 6>(transformations.size(), 6);
        for (size_t i = 0; i < transformations.size(); ++i) {
          output.row(i) = transformations[i].log();
        }
        return output;
      },
      "Return the log of SE3 as [translational_part, rotation_vector] of dimension Nx6.");

  type.def(
      "inverse",
      [](const SE3Group<Scalar>& transformations) -> SE3Group<Scalar> {
        SE3Group<Scalar> result;
        for (size_t i = 0; i < transformations.size(); ++i) {
          result = transformations[i].inverse();
        }
        return result;
      },
      "Compute the inverse of the transformations.");

  type.def(
      "rotation",
      [](const SE3Group<Scalar>& transformations) -> SO3Group<Scalar> {
        SO3Group<Scalar> rotations;
        rotations.reserve(transformations.size());
        for (size_t i = 0; i < transformations.size(); ++i) {
          rotations.push_back(transformations[i].so3());
        }
        return rotations;
      },
      "Get the rotation component of the transformation.");
  type.def(
      "translation",
      [](const SE3Group<Scalar>& transformations) -> Eigen::Matrix<Scalar, Eigen::Dynamic, 3> {
        auto translations = Eigen::Matrix<Scalar, Eigen::Dynamic, 3>(transformations.size(), 3);
        for (size_t i = 0; i < transformations.size(); ++i) {
          translations.row(i) = transformations[i].translation();
        }
        return translations;
      },
      "Get the translation component of the transformation.");

  type.def("__copy__", [](const SE3Group<Scalar>& transformations) -> SE3Group<Scalar> {
    return transformations; // copy is done with the std::vector copy constructor
  });
  type.def("__repr__", [](const SE3Group<Scalar>& transformation) -> std::string {
    std::stringstream stream;
    stream << fmt::format(
        "SE3 (quaternion(w,x,y,z), translation (x,y,z)) (x{})\n[", transformation.size());
    for (const auto& se3 : transformation) {
      stream << fmt::format(
          "[{}, {}, {}, {}, {}, {}, {}],\n",
          se3.unit_quaternion().w(),
          se3.unit_quaternion().x(),
          se3.unit_quaternion().y(),
          se3.unit_quaternion().z(),
          se3.translation().x(),
          se3.translation().y(),
          se3.translation().z());
    }
    // replace last to previous characters
    stream.seekp(-2, stream.cur);
    stream << "]";
    return stream.str();
  });
  type.def(
      "__len__", [](const SE3Group<Scalar>& transformations) { return transformations.size(); });

  type.def("__str__", [](const SE3Group<Scalar>& transformations) -> std::string {
    return fmt::format("sophus.SE3 (x{})", transformations.size());
  });

  type.def(
      "__matmul__",
      [](const SE3Group<Scalar>& transformations,
         const SE3Group<Scalar>& other) -> SE3Group<Scalar> {
        if (other.size() == 0 || transformations.size() == 0) {
          throw std::domain_error("Both operand should have size greater than 0");
        }
        SE3Group<Scalar> result;
        if (other.size() == 1) {
          result.reserve(transformations.size());
          for (size_t i = 0; i < transformations.size(); ++i) {
            result.push_back(transformations[i] * other[0]);
          }
        } else if (transformations.size() == 1) {
          result.reserve(other.size());
          for (size_t i = 0; i < other.size(); ++i) {
            result.push_back(transformations[0] * other[i]);
          }
        } else {
          throw std::domain_error(
              "Only allows transformations of size 1 to N (or N to 1) multiplication.");
        }
        return result;
      });

  type.def("__imatmul__", [](SE3Group<Scalar>& transformations, const SE3Group<Scalar>& other) {
    if (transformations.size() == 0 || other.size() == 0) {
      throw std::domain_error("Both operand should have size greater than 0");
    }

    if (transformations.size() == 1) {
      for (size_t i = 0; i < other.size(); ++i) {
        transformations[0] = transformations[0] * other[i];
      }
    } else if (other.size() == 1) {
      for (size_t i = 0; i < transformations.size(); ++i) {
        transformations[i] = transformations[i] * other[0];
      }
    } else {
      throw std::domain_error(
          "Only allows transformations of size 1 to N (or N to 1) multiplication.");
    }

    return transformations;
  });

  type.def(
      "__matmul__",
      [](const SE3Group<Scalar>& transformations,
         const Eigen::Matrix<Scalar, 3, Eigen::Dynamic>& matrix)
          -> Eigen::Matrix<Scalar, 3, Eigen::Dynamic> {
        if (matrix.cols() == 0 || transformations.size() == 0) {
          throw std::domain_error("Both operand should have size greater than 0");
        }
        if (transformations.size() != 1) {
          throw std::domain_error("Number of transformations must be 1.");
        }

        Eigen::Matrix<Scalar, 3, Eigen::Dynamic> result(3, matrix.cols());
        for (size_t i = 0; i < matrix.cols(); ++i) {
          result.col(i) = transformations[0] * matrix.col(i);
        }
        return result;
      });

  type.def(
      "__getitem__",
      [](const SE3Group<Scalar>& transformations, const size_t index) -> SE3Group<Scalar> {
        if (index < 0 || index >= transformations.size()) {
          throw std::out_of_range("Index out of range");
        }
        return transformations[index];
      });

  return type;
}

constexpr int kMaxAverageIteration = 10000;
template <typename Scalar>
void exportSE3Average(pybind11::module& module) {
  module.def(
      "iterativeMean",
      [](const SE3Group<Scalar>& transformations) -> SE3Group<Scalar> {
        return *(Sophus::iterativeMean<SE3Group<Scalar>>(transformations, kMaxAverageIteration));
      },
      "Compute the iterative mean of a sequence.");
}

template <typename Scalar>
void exportSE3Interpolate(pybind11::module& module) {
  module.def(
      "interpolate",
      [](const SE3Group<Scalar>& a, const SE3Group<Scalar>& b, double t) -> Sophus::SE3<Scalar> {
        if (a.size() != b.size() && a.size() != 1) {
          throw std::domain_error("Should have SE3 of size 1.");
        }
        return Sophus::interpolate<Sophus::SE3<double>>(a[0], b[0], t);
      },
      "Interpolate two SE3s of size 1.");
}

} // namespace sophus
