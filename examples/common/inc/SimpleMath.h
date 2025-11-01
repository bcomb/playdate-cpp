#pragma once

#include "math.h"

constexpr float EPSILON = 1e-6f;

struct vec2 {
    float x, y;
    vec2() = default;
    vec2(float _x) : x(_x), y(_x) {}
    vec2(float _x, float _y) : x(_x), y(_y) {}
    vec2 operator/(float v) const { return vec2(x / v, y / v); }
    vec2 operator-(const vec2& v) const { return vec2(x - v.x, y - v.y); }
    vec2 operator+(const vec2& v) const { return vec2(x + v.x, y + v.y); }
    vec2 operator*(float v) const { return vec2(x * v, y * v); }
    vec2& operator-=(const vec2& v) { x -= v.x; y -= v.y; return *this; }
    vec2& operator+=(const vec2& v) { x += v.x; y += v.y; return *this; }

};

union vec4 {
    struct { float r, g, b, a; };
    struct { float x, y, z, w; };
    vec4(float _x, float _y, float _z, float _w) : r(_x), g(_y), b(_z), a(_w) {}
};

inline float clamp(float v, float minVal, float maxVal)
{
    return v < minVal ? minVal : (v > maxVal ? maxVal : v);
}

inline float squaredLength(const vec2& v)
{
    return v.x * v.x + v.y * v.y;
}

inline float dot(const vec2& a, const vec2& b)
{
    return a.x * b.x + a.y * b.y;
}

inline float length(const vec2& v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

inline vec2 normalize(const vec2& v)
{
    float len = length(v);
    return v / len;
}

inline vec2 normalizeSafe(const vec2& v)
{
    float len = length(v);
    if (len > EPSILON)
    {
        return v / len;
    }
    return vec2(0.0f, 0.0f);
}

// v : incident vector
// n : normal vector (assumed normalized)
inline vec2 reflect(const vec2& v, const vec2& n)
{
    return v - n * (2.0f * (v.x * n.x + v.y * n.y));
}

// v : incident vector
// n : normal vector (may be not normalized)
inline vec2 reflect_n(const vec2& v, const vec2& n)
{
    float nn = squaredLength(n);
    if (nn <= EPSILON) return v; // avoid division by zero
    return v - n * 2.0f * dot(v,n) / nn;
}

inline vec2 floor(const vec2& v)
{
    return vec2(floorf(v.x), floorf(v.y));
}

inline float fract(float v)
{
    return v - floorf(v);
}

inline vec2 fract(const vec2& v)
{
    return vec2(v.x - floorf(v.x), v.y - floorf(v.y));
}

inline float radians(float degrees) {
    return degrees * 0.017453292519943295f; // PI / 180
}

inline float degrees(float radians) {
    return radians * 57.29577951308232f; // 180 / PI
}

inline float smoothstep(float edge0, float edge1, float x)
{
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0f - 2.0f * t);
}

inline float min(float a, float b)
{
    return (a < b) ? a : b;
}

inline float max(float a, float b)
{
    return (a > b) ? a : b;
}

inline float mix(float a, float b, float t)
{
    return a * (1.0f - t) + b * t;
}

inline vec4 mix(vec4 a, vec4 b, float t)
{
    return vec4(
        a.r * (1.0f - t) + b.r * t,
        a.g * (1.0f - t) + b.g * t,
        a.b * (1.0f - t) + b.b * t,
        a.a * (1.0f - t) + b.a * t
    );
}

inline vec2 lerp(const vec2& a, const vec2& b, float t)
{
    return a.x + t * (b.x - a.x);
}

inline float step(float edge, float x)
{
    return x < edge ? 0.0f : 1.0f;
}

inline float luminance(float r, float g, float b)
{
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

inline vec2 rotate(const vec2& point, const vec2& center, float radian)
{
    float cosA = cosf(radian);
    float sinA = sinf(radian);

    vec2 translated = point - center;

    vec2 rotated(
        translated.x * cosA - translated.y * sinA,
        translated.x * sinA + translated.y * cosA
    );

    return rotated + center;
}

union mat2
{
    struct { float data[4]; };
    struct { float m00, m01, m10, m11; };
    mat2(float _m00, float _m01, float _m10, float _m11)
        : m00(_m00), m01(_m01), m10(_m10), m11(_m11) {
    }
    vec2 operator*(const vec2& v) const {
        return vec2(m00 * v.x + m01 * v.y,
            m10 * v.x + m11 * v.y);
    }
};