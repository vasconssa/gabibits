#include "math/math.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

void orth_proj_matrix(float* result, float left, float right, float bottom, float top, float near, float far) {
    memset(result, 0, sizeof(float)*16);

    result[0]  = 2.0 / (right - left);
    result[3]  = -(right + left) / (right - left);
    result[5]  = 2.0 / (top - bottom);
    result[7]  = -(top + bottom) / (top - bottom);
    result[10] = 1.0 / (far - near);
    result[11] = -near / (far - near);
    result[15] = 1.0;
}

void perp_proj_matrix_helper(float* result, float x, float y, float width, float height, float near, float far) {
    memset(result, 0, sizeof(float)*16);
    
    result[0] = width;
    result[2] = x;
    result[5] = height;
    result[6] = y;
    result[10] = -far / (far - near);
    result[11] = -far * near/ (far - near);
    result[14] = -1.0;
}

void perp_proj_matrix(float* result, float fovy, float aspect, float near, float far) {
    float height = 1.0f / tanf(to_rad(fovy) * 0.5f);
    float width  = height * 1.0f / aspect;
    perp_proj_matrix_helper(result, 0.0f, 0.0f, width, height, near, far);
}

void mat4_mul_vec4(float* result, const float* mat, const float* vec) {
    result[0] = mat[0]  * vec[0] + mat[1]  * vec[1] + mat[2]  * vec[2] + mat[3]  * vec[3];
    result[1] = mat[4]  * vec[0] + mat[5]  * vec[1] + mat[6]  * vec[2] + mat[7]  * vec[3];
    result[2] = mat[8]  * vec[0] + mat[9]  * vec[1] + mat[10] * vec[2] + mat[11] * vec[3];
    result[3] = mat[12] * vec[0] + mat[13] * vec[1] + mat[14] * vec[2] + mat[15] * vec[3];
}
void mat4_mul(float* result, const float* a, const float* b) {
    result[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8] + a[3] * b[12];
    result[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9] + a[3] * b[13];
    result[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10] + a[3] * b[14];
    result[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3] * b[15];

    result[4] = a[4] * b[0] + a[5] * b[4] + a[6] * b[8] + a[7] * b[12];
    result[5] = a[4] * b[1] + a[5] * b[5] + a[6] * b[9] + a[7] * b[13];
    result[6] = a[4] * b[2] + a[5] * b[6] + a[6] * b[10] + a[7] * b[14];
    result[7] = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7] * b[15];

    result[8]  = a[8] * b[0] + a[9] * b[4] + a[10] * b[8] + a[11] * b[12];
    result[9]  = a[8] * b[1] + a[9] * b[5] + a[10] * b[9] + a[11] * b[13];
    result[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10] + a[11] * b[14];
    result[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11] * b[15];

    result[12] = a[12] * b[0] + a[13] * b[4] + a[14] * b[8] + a[15] * b[12];
    result[13] = a[12] * b[1] + a[13] * b[5] + a[14] * b[9] + a[15] * b[13];
    result[14] = a[12] * b[2] + a[13] * b[6] + a[14] * b[10] + a[15] * b[14];
    result[15] = a[12] * b[3] + a[13] * b[7] + a[14] * b[11] + a[15] * b[15];
}

void mat4_identy(float* result) {
    memset(result, 0, sizeof(float)*16);

    result[0]  = 1.0;
    result[5]  = 1.0;
    result[10] = 1.0;
    result[15] = 1.0;
}

void mat4_translate(float* result, float tx, float ty, float tz) {
    mat4_identy(result);
    result[3]  = tx;
    result[7]  = ty;
    result[11] = tz;
}

void mat4_rotate_X(float* result, float ax) {
    memset(result, 0, sizeof(float)*16);

    result[0]  = 1.0;
    result[5]  = cosf(ax);
    result[6]  = -sinf(ax);
    result[9]  = sinf(ax);
    result[10] = cosf(ax);
    result[15] = 1.0;
}

void mat4_rotate_Y(float* result, float ay) {
    memset(result, 0, 16 * sizeof(float));

    result[0]  = cosf(ay);
    result[2]  = sinf(ay);
    result[5]  = 1.0;
    result[8]  = -sinf(ay);
    result[10] = cosf(ay);
    result[15] = 1.0;
}

void mat4_rotate_Z(float* result, float az) {
    memset(result, 0, 16 * sizeof(float));

    result[0]  = cosf(az);
    result[1]  = -sinf(az);
    result[4]  = sinf(az);
    result[5]  = cosf(az);
    result[10] = 1.0;
    result[15] = 1.0;
}

void mat4_rotate_XYZ(float* result, float ax, float ay, float az) {
    memset(result, 0, 16 * sizeof(float));

    result[0]  = cosf(az) * cosf(ay);
    result[1]  = cosf(az) * sinf(ay) * sinf(ax) - sinf(az) * cosf(ax);
    result[2]  = cosf(az) * sinf(ay) * cosf(ax) + sinf(az) * sinf(ax);
    result[4]  = sinf(az) * cosf(ay);
    result[5]  = sinf(az) * sinf(ay) * sinf(ax) + cosf(az) * cosf(ax);
    result[6]  = sinf(az) * sinf(ay) * cosf(ax) - cosf(az) * sinf(ax);
    result[8]  = -sinf(ay);
    result[9]  = cosf(ay) * sinf(ax); 
    result[10] = cosf(ay) * cosf(ax);
    result[15] = 1.0;
}

void mat4_print(float *m) {
    printf("x        y        z        t\n");
    printf("%.6f %.6f %.6f %.6f\n", m[0], m[1], m[2], m[3]);
    printf("%.6f %.6f %.6f %.6f\n", m[4], m[5], m[6], m[7]);
    printf("%.6f %.6f %.6f %.6f\n", m[8], m[9], m[10], m[11]);
    printf("%.6f %.6f %.6f %.6f\n", m[12], m[13], m[14], m[15]);
}

void mat4_transpose(float* result, const float* a) {
    result[ 0] = a[ 0];
    result[ 4] = a[ 1];
    result[ 8] = a[ 2];
    result[12] = a[ 3];
    result[ 1] = a[ 4];
    result[ 5] = a[ 5];
    result[ 9] = a[ 6];
    result[13] = a[ 7];
    result[ 2] = a[ 8];
    result[ 6] = a[ 9];
    result[10] = a[10];
    result[14] = a[11];
    result[ 3] = a[12];
    result[ 7] = a[13];
    result[11] = a[14];
    result[15] = a[15];
}

void mat4_orthonormal_invert(float* result, float* a) {
    float tx = a[3];
    float ty = a[7];
    float tz = a[11];
    a[3] = 0;
    a[7] = 0;
    a[11] = 0;
    mat4_transpose(result, a);
    result[3]  = -tx;
    result[7]  = -ty;
    result[11] = -tz;
}

float to_rad(float deg)
{
    return deg * kPi / 180.0;
}

Quaternion invert(Quaternion a) {
    Quaternion r;
    r.x = -a.x;
    r.y = -a.y;
    r.z = -a.z;
    r.w = a.w;
    return r;
}

Vector3 mul_xyz(Quaternion a, Quaternion b) {
    Vector3 r;
    r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return r;
}

Quaternion quat_mul(Quaternion a, Quaternion b) {
    Quaternion r;
    r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    return r;
}

Vector3 vec_mul_quat(Vector3 v, Quaternion q) {
    const Quaternion tmp0 = invert(q);
    const Quaternion qv   = { v.x, v.y, v.z, 0.0f };
    const Quaternion tmp1 = quat_mul(tmp0, qv);
    const Vector3 r = mul_xyz(tmp1, q);
    return r;
}

float quat_dot(Quaternion a, Quaternion b) {
    float r;
    r = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

    return r;
}

Quaternion quat_normalize(Quaternion a) {
    Quaternion r = {0.0, 0.0, 0.0, 1.0};
    float norm = quat_dot(a, a);
    if (norm > 0.0) {
        float inv_norm = 1 / sqrtf(norm);
        
        r.x = a.x * inv_norm;
        r.y = a.y * inv_norm;
        r.z = a.z * inv_norm;
        r.w = a.w * inv_norm;
        return r;
    }

    return r;
}

Vector3 quat_to_euler(Quaternion a) {
    Vector3 r;
    const float xx  = a.x;
    const float yy  = a.y;
    const float zz  = a.z;
    const float ww  = a.w;
    const float xsq = xx * xx;
    const float ysq = yy * yy;
    const float zsq = zz * zz;

    r.x = atan2f(2.0f * (xx * ww - yy * zz), 1.0f - 2.0f * (xsq + zsq) );
    r.y = atan2f(2.0f * (yy * ww + xx * zz), 1.0f - 2.0f * (ysq + zsq) );
    r.z = asinf( 2.0f * (xx * yy + zz * ww) );

    return r;
}

Quaternion rotate_axis(Vector3 axis, float angle) {
    Quaternion r;
    float ha = angle * 0.5f;
    float sa = sinf(ha);

    r.x = axis.x * sa;
    r.y = axis.y * sa;
    r.z = axis.z * sa;
    r.w = cosf(ha);
    return r;
}

Quaternion rotate_X(float ax) {
    Quaternion r;

    float hx = ax * 0.5f;
    r.x = sinf(hx);
    r.y = 0.0f;
    r.z = 0.0f;
    r.w = cosf(hx);

    return r;
}

Quaternion rotate_Y(float ay) {
    Quaternion r;

    float hy = ay * 0.5f;
    r.x = 0.0f;
    r.y = sinf(hy);
    r.z = 0.0f;
    r.w = cosf(hy);

    return r;
}

Quaternion rotate_Z(float az) {
    Quaternion r;

    float hz = az * 0.5f;
    r.x = 0.0f;
    r.y = 0.0f;
    r.z = sinf(hz);
    r.w = cosf(hz);

    return r;
}

void mat4_from_quat(float* result, Quaternion q) {
    memset(result, 0, 16 * sizeof(float));
    result[0] = 1 - 2 * q.y * q.y - 2 * q.z * q.z;
    result[1] = 2 * q.x * q.y - 2 * q.w * q.z;
    result[2] = 2 * q.x * q.z + 2 * q.w * q.y;

    result[4] = 2 * q.x * q.y + 2 * q.w * q.z;
    result[5] = 1 - 2 * q.x * q.x - 2 * q.z * q.z;
    result[6] = 2 * q.y * q.z - 2 * q.w * q.x;

    result[8] = 2 * q.x * q.z - 2 * q.w * q.y;
    result[9] = 2 * q.y * q.y + 2 * q.w * q.x;
    result[10] = 1 - 2 * q.x * q.x - 2 * q.y * q.y;

    result[15] = 1.0;
}

void mat4_quat_translation(float* result, Quaternion q, Vector3 translation) {
    mat4_from_quat(result, q);

    result[3]  = translation.x;
    result[7]  = translation.y;
    result[11] = translation.z;
}

Vector3 right(Quaternion q) {
    float temp[16];
    mat4_from_quat(temp, q);
    Vector3 r;
    r.x = temp[0];
    r.y = temp[4];
    r.z = temp[8];
    return r;
}

Vector3 up(Quaternion q) {
    float temp[16];
    mat4_from_quat(temp, q);
    Vector3 r;
    r.x = temp[1];
    r.y = temp[5];
    r.z = temp[9];
    return r;
}

Vector3 forward(Quaternion q) {
    float temp[16];
    mat4_from_quat(temp, q);
    Vector3 r;
    r.x = temp[2];
    r.y = temp[6];
    r.z = temp[10];
    return r;
}

Vector3 vec3_mul(Vector3 v, float scalar) {
    Vector3 r;
    r.x = v.x * scalar;
    r.y = v.y * scalar;
    r.z = v.z * scalar;
    return r;
}

Vector3 vec3_add(Vector3 a, Vector3 b) {
    Vector3 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}
