#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <Arduino.h>

/**
 * Fast exponential approximation
 * e^x approx (1 + x/n)^n for n=256
 */
inline float fastExp(float x) {
    // Clamp to prevent explosion
    if (x < -10.0f) return 0.0f;
    if (x > 4.0f) x = 4.0f;

    x = 1.0f + x / 256.0f;
    x *= x; x *= x; x *= x; x *= x; // 2, 4, 8, 16
    x *= x; x *= x; x *= x; x *= x; // 32, 64, 128, 256
    return x;
}

/**
 * Fast 2^x approximation
 */
inline float fastExp2(float x) {
    // 2^x = e^(x * ln(2))
    return fastExp(x * 0.69314718f);
}

/**
 * Fast tanh approximation for saturation
 */
inline float fastTanh(float x) {
    if (x < -3.0f) return -1.0f;
    if (x > 3.0f) return 1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

#endif
