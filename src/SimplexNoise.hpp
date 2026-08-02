#ifndef SIMPLEX_NOISE_HPP
#define SIMPLEX_NOISE_HPP

#include <cmath>
#include <cstdint>
#include <algorithm>

class SimplexNoise {
private:
    static const uint8_t perm[512];
    static const int grad3[16][3];

    static inline int fastfloor(float x) {
        int xi = (int)x;
        return x < xi ? xi - 1 : xi;
    }

    static inline float dot(const int g[3], float x, float y, float z) {
        return g[0] * x + g[1] * y + g[2] * z;
    }

public:
    static float eval3D(float xin, float yin, float zin) {
        float n0, n1, n2, n3;

        const float F3 = 1.0f / 3.0f;
        float s = (xin + yin + zin) * F3;
        int i = fastfloor(xin + s);
        int j = fastfloor(yin + s);
        int k = fastfloor(zin + s);

        const float G3 = 1.0f / 6.0f;
        float t = (i + j + k) * G3;
        float X0 = i - t;
        float Y0 = j - t;
        float Z0 = k - t;
        float x0 = xin - X0;
        float y0 = yin - Y0;
        float z0 = zin - Z0;

        int i1, j1, k1;
        int i2, j2, k2;

        if (x0 >= y0) {
            if (y0 >= z0)      { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }
            else if (x0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }
            else               { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }
        } else {
            if (y0 < z0)       { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }
            else if (x0 < z0)  { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }
            else               { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }
        }

        float x1 = x0 - i1 + G3;
        float y1 = y0 - j1 + G3;
        float z1 = z0 - k1 + G3;
        float x2 = x0 - i2 + 2.0f * G3;
        float y2 = y0 - j2 + 2.0f * G3;
        float z2 = z0 - k2 + 2.0f * G3;
        float x3 = x0 - 1.0f + 3.0f * G3;
        float y3 = y0 - 1.0f + 3.0f * G3;
        float z3 = z0 - 1.0f + 3.0f * G3;

        int ii = i & 255;
        int jj = j & 255;
        int kk = k & 255;

        float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;
        if (t0 < 0) n0 = 0.0f;
        else {
            t0 *= t0;
            int gi0 = perm[ii + perm[jj + perm[kk]]] % 16;
            n0 = t0 * t0 * dot(grad3[gi0], x0, y0, z0);
        }

        float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
        if (t1 < 0) n1 = 0.0f;
        else {
            t1 *= t1;
            int gi1 = perm[ii+i1 + perm[jj+j1 + perm[kk+k1]]] % 16;
            n1 = t1 * t1 * dot(grad3[gi1], x1, y1, z1);
        }

        float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
        if (t2 < 0) n2 = 0.0f;
        else {
            t2 *= t2;
            int gi2 = perm[ii+i2 + perm[jj+j2 + perm[kk+k2]]] % 16;
            n2 = t2 * t2 * dot(grad3[gi2], x2, y2, z2);
        }

        float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
        if (t3 < 0) n3 = 0.0f;
        else {
            t3 *= t3;
            int gi3 = perm[ii+1 + perm[jj+1 + perm[kk+1]]] % 16;
            n3 = t3 * t3 * dot(grad3[gi3], x3, y3, z3);
        }

        return 32.0f * (n0 + n1 + n2 + n3);
    }

    static float octave3D(float x, float y, float z, int octaves, float persistence, float lacunarity) {
        float total = 0.0f;
        float frequency = 1.0f;
        float amplitude = 1.0f;
        float maxValue = 0.0f;

        for (int i = 0; i < octaves; i++) {
            total += eval3D(x * frequency, y * frequency, z * frequency) * amplitude;
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return total / maxValue;
    }
};

#endif // SIMPLEX_NOISE_HPP
