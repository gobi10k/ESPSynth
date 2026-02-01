#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <Arduino.h>

/**
 * Fast exponential approximation
 * e^x approx (1 + x/n)^n for n=256
 */
inline float fastExp(float x) {
    x = 1.0f + x / 256.0f;
    x *= x; x *= x; x *= x; x *= x; // 2, 4, 8, 16
    x *= x; x *= x; x *= x; x *= x; // 32, 64, 128, 256
    return x;
}

#endif
