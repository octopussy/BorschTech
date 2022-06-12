#pragma once

//#define GLM_FORCE_CXX17
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_MESSAGES // Or defined when building (e.g. -DGLM_FORCE_SWIZZLE)

// Include all GLM core / GLSL features
#include <glm/glm.hpp> // vec2, vec3, mat4, radians

// Include all GLM extensions
#include <glm/ext.hpp> // perspective, translate, rotate

typedef glm::vec<3, double, glm::defaultp> Vector;
typedef glm::vec<4, double, glm::defaultp> Vector4;
typedef glm::mat<4, 4, double, glm::defaultp> Matrix;
typedef glm::mat<4, 4, float, glm::defaultp> MatrixF;

inline Matrix MatrixTranspose(const Matrix& src) {
    return glm::transpose(src);
}