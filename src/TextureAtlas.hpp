#ifndef TEXTURE_ATLAS_HPP
#define TEXTURE_ATLAS_HPP

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

class TextureAtlas {
public:
    static constexpr int TILE_SIZE = 16;
    static constexpr int TILES_PER_ROW = 16;
    static constexpr int ATLAS_SIZE = TILE_SIZE * TILES_PER_ROW; // 256x256 pixels

    static GLuint createProceduralAtlas() {
        std::vector<uint8_t> pixels(ATLAS_SIZE * ATLAS_SIZE * 4, 255);

        // Helper to set pixel color at atlas coordinate (x, y)
        auto setPixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            if (x < 0 || x >= ATLAS_SIZE || y < 0 || y >= ATLAS_SIZE) return;
            int idx = (y * ATLAS_SIZE + x) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        };

        // Pseudo-random helper
        auto randNoise = [](int x, int y, int seed) -> float {
            int n = x + y * 57 + seed * 131;
            n = (n << 13) ^ n;
            return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
        };

        // Generate each 16x16 tile
        for (int tileID = 0; tileID < 32; ++tileID) {
            int startX = (tileID % TILES_PER_ROW) * TILE_SIZE;
            int startY = (tileID / TILES_PER_ROW) * TILE_SIZE;

            for (int py = 0; py < TILE_SIZE; ++py) {
                for (int px = 0; px < TILE_SIZE; ++px) {
                    int ax = startX + px;
                    int ay = startY + py;
                    float n = randNoise(px, py, tileID * 17) * 0.15f;

                    switch (tileID) {
                        case 0: { // Grass Top
                            uint8_t r = (uint8_t)std::clamp(55.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(170.0f + n * 60.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 1: { // Grass Side
                            if (py < 4 + (int)(n * 10.0f)) { // Green top lip
                                uint8_t r = (uint8_t)std::clamp(55.0f + n * 40.0f, 0.0f, 255.0f);
                                uint8_t g = (uint8_t)std::clamp(170.0f + n * 60.0f, 0.0f, 255.0f);
                                uint8_t b = (uint8_t)std::clamp(45.0f + n * 30.0f, 0.0f, 255.0f);
                                setPixel(ax, ay, r, g, b);
                            } else { // Dirt body
                                uint8_t r = (uint8_t)std::clamp(115.0f + n * 40.0f, 0.0f, 255.0f);
                                uint8_t g = (uint8_t)std::clamp(75.0f + n * 30.0f, 0.0f, 255.0f);
                                uint8_t b = (uint8_t)std::clamp(45.0f + n * 20.0f, 0.0f, 255.0f);
                                setPixel(ax, ay, r, g, b);
                            }
                            break;
                        }
                        case 2: { // Dirt
                            uint8_t r = (uint8_t)std::clamp(115.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(75.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 20.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 3: { // Stone
                            uint8_t val = (uint8_t)std::clamp(120.0f + n * 50.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, val, val, val);
                            break;
                        }
                        case 4: { // Glow Crystal
                            float dist = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f)) / 8.0f;
                            uint8_t r = (uint8_t)std::clamp(255.0f - dist * 100.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(230.0f - dist * 120.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(100.0f + dist * 120.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 5: { // Oak Log Side
                            float barkNoise = std::sin(px * 1.5f) * 0.2f + n * 0.3f;
                            uint8_t r = (uint8_t)std::clamp(100.0f + barkNoise * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(65.0f + barkNoise * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(35.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 6: { // Oak Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.3f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(160.0f + ring * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(120.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(70.0f + ring * 20.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 7: { // Standard Oak Leaves
                            uint8_t r = (uint8_t)std::clamp(45.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(155.0f + n * 50.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(35.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t alpha = (px % 3 == 0 && py % 3 == 0) ? 200 : 255;
                            setPixel(ax, ay, r, g, b, alpha);
                            break;
                        }
                        case 8: { // Sand
                            uint8_t r = (uint8_t)std::clamp(220.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(200.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(130.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 9: { // Sky Quartz
                            uint8_t r = (uint8_t)std::clamp(210.0f + n * 45.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(235.0f + n * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(255.0f + n * 10.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 10: { // Water
                            uint8_t r = (uint8_t)std::clamp(35.0f + n * 20.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(115.0f + n * 35.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(205.0f + n * 35.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 11: { // Birch Log Side
                            float stripe = (py % 4 == 0 && (px + (py / 4) * 5) % 7 < 3) ? 0.25f : 0.95f;
                            uint8_t val = (uint8_t)std::clamp(230.0f * stripe + n * 25.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, val, val, (uint8_t)(val * 0.95f));
                            break;
                        }
                        case 12: { // Birch Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.2f) * 0.2f + n * 0.15f;
                            uint8_t r = (uint8_t)std::clamp(220.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(195.0f + ring * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(140.0f + ring * 20.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 13: { // Light Spring Oak Leaves
                            uint8_t r = (uint8_t)std::clamp(65.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(180.0f + n * 50.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t alpha = (px % 3 == 0 && py % 3 == 0) ? 200 : 255;
                            setPixel(ax, ay, r, g, b, alpha);
                            break;
                        }
                        case 14: { // Pine Log Side
                            float barkNoise = std::sin(px * 2.0f) * 0.25f + n * 0.35f;
                            uint8_t r = (uint8_t)std::clamp(75.0f + barkNoise * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(45.0f + barkNoise * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(25.0f + barkNoise * 15.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 15: { // Pine Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.5f) * 0.25f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(130.0f + ring * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(85.0f + ring * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + ring * 15.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 16: { // Pine Leaves
                            uint8_t r = (uint8_t)std::clamp(20.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(100.0f + n * 45.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 35.0f, 0.0f, 255.0f);
                            uint8_t alpha = (px % 3 == 0 && py % 3 == 0) ? 200 : 255;
                            setPixel(ax, ay, r, g, b, alpha);
                            break;
                        }
                        case 17: { // Dark Log Side
                            float barkNoise = std::sin(px * 1.8f) * 0.2f + n * 0.3f;
                            uint8_t r = (uint8_t)std::clamp(55.0f + barkNoise * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(35.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(20.0f + barkNoise * 15.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 18: { // Dark Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.2f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(90.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(60.0f + ring * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(35.0f + ring * 15.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 19: { // Dark Forest Oak Leaves
                            uint8_t r = (uint8_t)std::clamp(25.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(125.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(25.0f + n * 25.0f, 0.0f, 255.0f);
                            uint8_t alpha = (px % 3 == 0 && py % 3 == 0) ? 200 : 255;
                            setPixel(ax, ay, r, g, b, alpha);
                            break;
                        }
                        case 20: { // Cherry Log Side
                            float barkNoise = std::sin(px * 1.5f) * 0.15f + n * 0.25f;
                            uint8_t r = (uint8_t)std::clamp(80.0f + barkNoise * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(45.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 21: { // Cherry Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.2f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(150.0f + ring * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(95.0f + ring * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(90.0f + ring * 25.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 22: { // Cherry Leaves
                            uint8_t r = (uint8_t)std::clamp(245.0f + n * 15.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(140.0f + n * 50.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(185.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t alpha = (px % 3 == 0 && py % 3 == 0) ? 200 : 255;
                            setPixel(ax, ay, r, g, b, alpha);
                            break;
                        }
                        case 23: { // Warm Golden-Tipped Oak Leaves
                            uint8_t r = (uint8_t)std::clamp(85.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(160.0f + n * 45.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(30.0f + n * 25.0f, 0.0f, 255.0f);
                            uint8_t alpha = (px % 3 == 0 && py % 3 == 0) ? 200 : 255;
                            setPixel(ax, ay, r, g, b, alpha);
                            break;
                        }
                        case 24: { // Magic Log Side
                            bool rune = (px + py * 3 + (int)(n * 10)) % 7 == 0;
                            uint8_t r = rune ? 40 : (uint8_t)std::clamp(45.0f + n * 25.0f, 0.0f, 255.0f);
                            uint8_t g = rune ? 220 : (uint8_t)std::clamp(25.0f + n * 20.0f, 0.0f, 255.0f);
                            uint8_t b = rune ? 255 : (uint8_t)std::clamp(85.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 25: { // Magic Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.3f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(110.0f + ring * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(70.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(220.0f + ring * 35.0f, 0.0f, 255.0f);
                            setPixel(ax, ay, r, g, b);
                            break;
                        }
                        case 26: { // Magic Leaves
                            uint8_t r = (uint8_t)std::clamp(30.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(225.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(245.0f + n * 10.0f, 0.0f, 255.0f);
                            uint8_t alpha = (px % 3 == 0 && py % 3 == 0) ? 200 : 255;
                            setPixel(ax, ay, r, g, b, alpha);
                            break;
                        }
                        case 27: { // Golden Leaves
                            uint8_t r = (uint8_t)std::clamp(245.0f + n * 10.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(205.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(30.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t alpha = (px % 3 == 0 && py % 3 == 0) ? 200 : 255;
                            setPixel(ax, ay, r, g, b, alpha);
                            break;
                        }
                        default: { // Fallback magenta pattern
                            bool alt = (px + py) % 2 == 0;
                            setPixel(ax, ay, alt ? 255 : 0, 0, alt ? 255 : 0);
                            break;
                        }
                    }
                }
            }
        }

        GLuint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_2D);

        return texId;
    }
};

#endif // TEXTURE_ATLAS_HPP
