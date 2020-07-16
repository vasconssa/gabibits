#pragma once

typedef enum Handness {
    LEFT_HAND,
    RIGHT_HAND
} Handness;

typedef struct Vector3 {
    float x, y, z;
} Vector3;

typedef struct Quaternion {
    float x, y, z, w;
} Quaternion;
