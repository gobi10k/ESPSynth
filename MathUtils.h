#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <Arduino.h>

#define TWO_PI 6.28318530717958647693f
#define TWO_PI_INV_SR -0.00013089969f // -2 * PI / 48000

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

/**
 * Fast log2 approximation
 */
inline float fastLog2(float x) {
    union { float f; uint32_t i; } vx = { x };
    float y = (float)vx.i;
    y *= 1.1920928955078125e-7f;
    return y - 126.94269504f;
}

/**
 * Fast PRNG (Xorshift)
 */
inline uint32_t fastRand(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

inline float fastRandFloat(uint32_t& state) {
    return (float)(int32_t)fastRand(state) / (float)INT32_MAX;
}

inline float fastRandFloat01(uint32_t& state) {
    return (float)(fastRand(state) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

#endif
