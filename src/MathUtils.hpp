#ifndef MATH_UTILS_HPP
#define MATH_UTILS_HPP

#include <cmath>
#include <algorithm>
#include <array>
#include <iostream>

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;

struct Vec3 {
    float x, y, z;

    constexpr Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    constexpr Vec3(float s) : x(s), y(s), z(s) {}
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& r) const { return Vec3(x + r.x, y + r.y, z + r.z); }
    Vec3 operator-(const Vec3& r) const { return Vec3(x - r.x, y - r.y, z - r.z); }
    Vec3 operator*(const Vec3& r) const { return Vec3(x * r.x, y * r.y, z * r.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    Vec3& operator+=(const Vec3& r) { x += r.x; y += r.y; z += r.z; return *this; }
    Vec3& operator-=(const Vec3& r) { x -= r.x; y -= r.y; z -= r.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }

    Vec3 normalized() const {
        float len = length();
        if (len > 1e-6f) return *this / len;
        return Vec3(0, 0, 0);
    }

    static float dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return Vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    static Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
        return a * (1.0f - t) + b * t;
    }
};

struct Vec4 {
    float x, y, z, w;

    constexpr Vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    constexpr Vec4(float s) : x(s), y(s), z(s), w(s) {}
    constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

struct IVec3 {
    int64_t x, y, z;

    constexpr IVec3() : x(0), y(0), z(0) {}
    constexpr IVec3(int64_t x_, int64_t y_, int64_t z_) : x(x_), y(y_), z(z_) {}

    bool operator==(const IVec3& r) const { return x == r.x && y == r.y && z == r.z; }
    bool operator!=(const IVec3& r) const { return !(*this == r); }
    bool operator<(const IVec3& r) const {
        if (x != r.x) return x < r.x;
        if (y != r.y) return y < r.y;
        return z < r.z;
    }

    IVec3 operator+(const IVec3& r) const { return IVec3(x + r.x, y + r.y, z + r.z); }
    IVec3 operator-(const IVec3& r) const { return IVec3(x - r.x, y - r.y, z - r.z); }
    IVec3 operator*(int64_t s) const { return IVec3(x * s, y * s, z * s); }
};

struct IVec3Hash {
    std::size_t operator()(const IVec3& v) const noexcept {
        std::size_t h1 = std::hash<int64_t>{}(v.x);
        std::size_t h2 = std::hash<int64_t>{}(v.y);
        std::size_t h3 = std::hash<int64_t>{}(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct Mat4 {
    float m[16];

    Mat4() {
        std::fill(m, m + 16, 0.0f);
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static Mat4 identity() {
        return Mat4();
    }

    static Mat4 perspective(float fovRad, float aspect, float nearVal, float farVal) {
        Mat4 r;
        std::fill(r.m, r.m + 16, 0.0f);
        float tanHalfFov = std::tan(fovRad / 2.0f);
        r.m[0] = 1.0f / (aspect * tanHalfFov);
        r.m[5] = 1.0f / tanHalfFov;
        r.m[10] = -(farVal + nearVal) / (farVal - nearVal);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * farVal * nearVal) / (farVal - nearVal);
        r.m[15] = 0.0f;
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalized();
        Vec3 s = Vec3::cross(f, up).normalized();
        Vec3 u = Vec3::cross(s, f);

        Mat4 r;
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;  r.m[12] = -Vec3::dot(s, eye);
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;  r.m[13] = -Vec3::dot(u, eye);
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] = Vec3::dot(f, eye);
        r.m[3] = 0.0f; r.m[7] = 0.0f; r.m[11] = 0.0f; r.m[15] = 1.0f;
        return r;
    }

    Mat4 operator*(const Mat4& r) const {
        Mat4 out;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                out.m[row + col * 4] =
                    m[row + 0 * 4] * r.m[0 + col * 4] +
                    m[row + 1 * 4] * r.m[1 + col * 4] +
                    m[row + 2 * 4] * r.m[2 + col * 4] +
                    m[row + 3 * 4] * r.m[3 + col * 4];
            }
        }
        return out;
    }
};

struct Plane {
    Vec3 normal;
    float d;

    Plane() : normal(0, 1, 0), d(0) {}
    Plane(float a, float b, float c, float d_) : normal(a, b, c), d(d_) {
        float len = normal.length();
        if (len > 1e-6f) {
            normal = normal / len;
            d = d_ / len;
        }
    }

    float distance(const Vec3& p) const {
        return Vec3::dot(normal, p) + d;
    }
};

struct Frustum {
    Plane planes[6];

    static Frustum extract(const Mat4& vp) {
        Frustum f;
        const float* m = vp.m;

        // Left
        f.planes[0] = Plane(m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]);
        // Right
        f.planes[1] = Plane(m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]);
        // Bottom
        f.planes[2] = Plane(m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]);
        // Top
        f.planes[3] = Plane(m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]);
        // Near
        f.planes[4] = Plane(m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]);
        // Far
        f.planes[5] = Plane(m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]);

        return f;
    }

    bool intersectsAABB(const Vec3& minP, const Vec3& maxP) const {
        for (int i = 0; i < 6; ++i) {
            const Plane& pl = planes[i];
            float px = (pl.normal.x >= 0.0f) ? maxP.x : minP.x;
            float py = (pl.normal.y >= 0.0f) ? maxP.y : minP.y;
            float pz = (pl.normal.z >= 0.0f) ? maxP.z : minP.z;

            if (pl.normal.x * px + pl.normal.y * py + pl.normal.z * pz + pl.d < 0.0f) {
                return false;
            }
        }
        return true;
    }
};

#endif // MATH_UTILS_HPP
