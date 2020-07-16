#pragma once

#include "math/types.h"
#include <stdbool.h>

#define kPi 3.1415926535897932384626433832795f

void mat4_transpose(float* result, const float* a);

void orth_proj_matrix(float* result, float left, float right, float bottom, float top, float near, float far);
void perp_proj_matrix_helper(float* result, float left, float right, float bottom, float top, float near, float far);
void perp_proj_matrix(float* result, float fovy, float aspect, float near, float far);

void mat4_mul_vec4(float* result, const float* mat, const float* vec);
void mat4_mul(float* result, const float* a, const float* b);
void mat4_identy(float* result);
void mat4_translate(float* result, float tx, float ty, float tz);
void mat4_rotate_X(float* result, float ax);
void mat4_rotate_Y(float* result, float ay);
void mat4_rotate_Z(float* result, float az);
void mat4_rotate_XYZ(float* result, float ax, float ay, float az);
/* Here we assume all matriz are orthonormal */
void mat4_orthonormal_invert(float* result, float* a);

void mat4_print(float *m);

float to_rad(float deg);

Quaternion quat_invert(Quaternion a);

Vector3 mul_xyz(Quaternion a, Quaternion b);

Quaternion quat_mul(Quaternion a, Quaternion b);

Vector3 vec_mul_quat(Vector3 v, Quaternion q);

float quat_dot(Quaternion a, Quaternion b);

Quaternion quat_normalize(Quaternion a);

Vector3 quat_to_euler(Quaternion a);

Quaternion rotate_axis(Vector3 axis, float angle);

Quaternion rotate_X(float ax);

Quaternion rotate_Y(float ay);

Quaternion rotate_Z(float az);

void mat4_from_quat(float* result, Quaternion q);

void mat4_quat_translation(float* result, Quaternion q, Vector3 translation);

Vector3 right(Quaternion q);

Vector3 up(Quaternion q);

Vector3 forward(Quaternion q);

Vector3 vec3_mul(Vector3 v, float scalar);

Vector3 vec3_add(Vector3 a, Vector3 b);

