/**
 * @file mat4inverse.hpp
 * @brief The general 4x4 matrix inversion algorithm, factored out of mat4
 * itself: a self-contained numerical routine, not a "construct a mat4 for
 * purpose X" factory like mat4's own static methods, so it earns being its
 * own class rather than another few hundred lines on mat4.cpp.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class Mat4Inverse
{
  public:
	Mat4Inverse();
	Mat4Inverse(const Mat4Inverse &other);
	Mat4Inverse &operator=(const Mat4Inverse &other);
	~Mat4Inverse();

	/**
		* @brief Inverts a column-major 4x4 matrix via the classic
		* public-domain cofactor/adjugate formula (e.g. as used in MESA's
		* gluInvertMatrix).
		* @param in The 16 column-major elements of the matrix to invert.
		* @param out Receives the 16 inverted elements. Untouched if `in`
		* is singular.
		* @return false if `in` is (numerically) singular.
		*/
	static bool compute(const float in[16], float out[16]);
};

} // namespace vre
