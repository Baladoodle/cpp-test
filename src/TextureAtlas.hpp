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
    static constexpr int MAX_TILES = 256;

    static GLuint createProceduralAtlas() {
        std::vector<uint8_t> pixels(MAX_TILES * TILE_SIZE * TILE_SIZE * 4, 255);

        int currentTileID = 0;
        // Helper to set pixel color at tile coordinate (x, y)
        auto setPixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            if (currentTileID < 0 || currentTileID >= MAX_TILES || x < 0 || x >= TILE_SIZE || y < 0 || y >= TILE_SIZE) return;
            int idx = (currentTileID * TILE_SIZE * TILE_SIZE + y * TILE_SIZE + x) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        };

        // Pseudo-random helper
        auto randNoise = [](int x, int y, int seed) -> float {
            uint32_t n = static_cast<uint32_t>(x) +
                static_cast<uint32_t>(y) * 57u +
                static_cast<uint32_t>(seed) * 131u;
            n = (n << 13) ^ n;
            uint32_t value = n * (n * n * 15731u + 789221u) + 1376312589u;
            return 1.0f - static_cast<float>(value & 0x7fffffffu) / 1073741824.0f;
        };

        // Dummy pixel masks for now. They are deliberately hand-authored as
        // clusters instead of radial shapes so the canopy reads as foliage at
        // close range. Real leaf art can replace these atlas tiles later.
        const char* leafMasks[6][16] = {
            {
                "......##........", "....######......", "...########.....", "..##########....",
                ".############...", "##############..", "##############..", "###############.",
                "##############..", ".############...", "..###########...", "...#########....",
                "....#######.....", ".....#####......", "......###.......", "................"
            },
            {
                ".....###........", "...#######......", "..#########.....", ".#####.#####....",
                ".############...", "##############..", "##############..", "###############.",
                ".#############..", "..#####.#####...", "...##########...", "....########....",
                ".....######.....", "......####......", "................", "................"
            },
            {
                ".......##.......", ".....#####......", "...########.....", "..#####.###.....",
                ".############...", "###########.##..", "##############..", "###############.",
                "##############..", "..###########...", "...###.######...", "....########....",
                ".....######.....", "......####......", ".......##.......", "................"
            },
            {
                "....####........", "..########......", ".##########.....", ".####.######....",
                "#############...", "##############..", "######.#######..", "###############.",
                "##############..", ".######.######..", "..###########...", "...##########...",
                "....#######.....", ".....#####......", "......###.......", "................"
            },
            {
                "......###.......", "....#######.....", "..##########....", ".######.#####...",
                "#############...", "###########.##..", "##############..", "###############.",
                "##############..", "..###########...", "...##########...", "....#####.##....",
                ".....######.....", "......####......", "................", "................"
            },
            {
                ".....####.......", "...########.....", "..##########....", ".#######.###....",
                "#############...", "##############..", "########.######.", "###############.",
                "##############..", ".###########....", "..######.####...", "...##########...",
                "....#######.....", ".....#####......", "......###.......", "................"
            }
        };

        // Leaves are rendered as alpha-tested cutouts rather than blended
        // quads. This keeps depth ordering deterministic in dense trees.
        auto leafMask = [&](int px, int py, int tileID) -> bool {
            int variant = 0;
            if (tileID >= 38 && tileID <= 57) {
                variant = (tileID - 38) % 5 + 1;
            }
            return leafMasks[variant][py][px] == '#';
        };

        // Generate each 16x16 tile
        for (int tileID = 0; tileID < 60; ++tileID) {
            currentTileID = tileID;

            for (int py = 0; py < TILE_SIZE; ++py) {
                for (int px = 0; px < TILE_SIZE; ++px) {
                    float n = randNoise(px, py, tileID * 17) * 0.15f;

                    switch (tileID) {
                        case 0: { // Grass Top
                            uint8_t r = (uint8_t)std::clamp(55.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(170.0f + n * 60.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 1: { // Grass Side
                            if (py < 4 + (int)(n * 10.0f)) { // Green top lip
                                uint8_t r = (uint8_t)std::clamp(55.0f + n * 40.0f, 0.0f, 255.0f);
                                uint8_t g = (uint8_t)std::clamp(170.0f + n * 60.0f, 0.0f, 255.0f);
                                uint8_t b = (uint8_t)std::clamp(45.0f + n * 30.0f, 0.0f, 255.0f);
                                setPixel(px, py, r, g, b);
                            } else { // Dirt body
                                uint8_t r = (uint8_t)std::clamp(115.0f + n * 40.0f, 0.0f, 255.0f);
                                uint8_t g = (uint8_t)std::clamp(75.0f + n * 30.0f, 0.0f, 255.0f);
                                uint8_t b = (uint8_t)std::clamp(45.0f + n * 20.0f, 0.0f, 255.0f);
                                setPixel(px, py, r, g, b);
                            }
                            break;
                        }
                        case 2: { // Dirt
                            uint8_t r = (uint8_t)std::clamp(115.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(75.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 20.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 3: { // Stone
                            uint8_t val = (uint8_t)std::clamp(120.0f + n * 50.0f, 0.0f, 255.0f);
                            setPixel(px, py, val, val, val);
                            break;
                        }
                        case 58: { // Deep Stone
                            // A cool, darker slate gives the underside a
                            // readable material gradient without relying on
                            // shader-only color tricks.
                            float vertical = static_cast<float>(py) / 15.0f;
                            float vein = ((px * 5 + py * 3) % 11 == 0) ? 1.0f : 0.0f;
                            uint8_t r = (uint8_t)std::clamp(55.0f + vertical * 10.0f + n * 30.0f + vein * 16.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(64.0f + vertical * 12.0f + n * 32.0f + vein * 18.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(70.0f + vertical * 14.0f + n * 36.0f + vein * 22.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 4: { // Glow Crystal
                            float dist = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f)) / 8.0f;
                            uint8_t r = (uint8_t)std::clamp(255.0f - dist * 100.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(230.0f - dist * 120.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(100.0f + dist * 120.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 5: { // Oak Log Side
                            float barkNoise = std::sin(px * 1.5f) * 0.2f + n * 0.3f;
                            uint8_t r = (uint8_t)std::clamp(100.0f + barkNoise * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(65.0f + barkNoise * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(35.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 6: { // Oak Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.3f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(160.0f + ring * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(120.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(70.0f + ring * 20.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 7: // Standard Oak Leaves
                        case 38:
                        case 39:
                        case 40:
                        case 41:
                        case 42: {
                            uint8_t r = (uint8_t)std::clamp(45.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(155.0f + n * 50.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(35.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 8: { // Sand
                            uint8_t r = (uint8_t)std::clamp(220.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(200.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(130.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 9: { // Sky Quartz
                            uint8_t r = (uint8_t)std::clamp(210.0f + n * 45.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(235.0f + n * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(255.0f + n * 10.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 10: { // Water
                            uint8_t r = (uint8_t)std::clamp(35.0f + n * 20.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(115.0f + n * 35.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(205.0f + n * 35.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 11: { // Birch Log Side
                            float stripe = (py % 4 == 0 && (px + (py / 4) * 5) % 7 < 3) ? 0.25f : 0.95f;
                            uint8_t val = (uint8_t)std::clamp(230.0f * stripe + n * 25.0f, 0.0f, 255.0f);
                            setPixel(px, py, val, val, (uint8_t)(val * 0.95f));
                            break;
                        }
                        case 12: { // Birch Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.2f) * 0.2f + n * 0.15f;
                            uint8_t r = (uint8_t)std::clamp(220.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(195.0f + ring * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(140.0f + ring * 20.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 13: // Light Spring Oak Leaves
                        case 43:
                        case 44:
                        case 45:
                        case 46:
                        case 47: {
                            uint8_t r = (uint8_t)std::clamp(65.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(180.0f + n * 50.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 14: { // Pine Log Side
                            float barkNoise = std::sin(px * 2.0f) * 0.25f + n * 0.35f;
                            uint8_t r = (uint8_t)std::clamp(75.0f + barkNoise * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(45.0f + barkNoise * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(25.0f + barkNoise * 15.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 15: { // Pine Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.5f) * 0.25f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(130.0f + ring * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(85.0f + ring * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + ring * 15.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 16: { // Pine Leaves
                            uint8_t r = (uint8_t)std::clamp(20.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(100.0f + n * 45.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 35.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 17: { // Dark Log Side
                            float barkNoise = std::sin(px * 1.8f) * 0.2f + n * 0.3f;
                            uint8_t r = (uint8_t)std::clamp(55.0f + barkNoise * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(35.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(20.0f + barkNoise * 15.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 18: { // Dark Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.2f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(90.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(60.0f + ring * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(35.0f + ring * 15.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 19: // Dark Forest Oak Leaves
                        case 48:
                        case 49:
                        case 50:
                        case 51:
                        case 52: {
                            uint8_t r = (uint8_t)std::clamp(25.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(125.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(25.0f + n * 25.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 20: { // Cherry Log Side
                            float barkNoise = std::sin(px * 1.5f) * 0.15f + n * 0.25f;
                            uint8_t r = (uint8_t)std::clamp(80.0f + barkNoise * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(45.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 21: { // Cherry Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.2f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(150.0f + ring * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(95.0f + ring * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(90.0f + ring * 25.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 22: { // Cherry Leaves
                            uint8_t r = (uint8_t)std::clamp(245.0f + n * 15.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(140.0f + n * 50.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(185.0f + n * 40.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 23: // Warm Golden-Tipped Oak Leaves
                        case 53:
                        case 54:
                        case 55:
                        case 56:
                        case 57: {
                            uint8_t r = (uint8_t)std::clamp(85.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(160.0f + n * 45.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(30.0f + n * 25.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 24: { // Magic Log Side
                            bool rune = (px + py * 3 + (int)(n * 10)) % 7 == 0;
                            uint8_t r = rune ? 40 : (uint8_t)std::clamp(45.0f + n * 25.0f, 0.0f, 255.0f);
                            uint8_t g = rune ? 220 : (uint8_t)std::clamp(25.0f + n * 20.0f, 0.0f, 255.0f);
                            uint8_t b = rune ? 255 : (uint8_t)std::clamp(85.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 25: { // Magic Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.3f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(110.0f + ring * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(70.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(220.0f + ring * 35.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 26: { // Magic Leaves
                            uint8_t r = (uint8_t)std::clamp(30.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(225.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(245.0f + n * 10.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 27: { // Golden Leaves
                            uint8_t r = (uint8_t)std::clamp(245.0f + n * 10.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(205.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(30.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 29: // Meadow grass lower
                        case 30: // Fine grass lower
                        case 31: // Seeded grass lower
                        case 32: // Meadow grass upper
                        case 33: // Fine grass upper
                        case 34: // Seeded grass upper
                        case 35: // Meadow grass two-tall lower
                        case 36: // Fine grass two-tall lower
                        case 37: { // Seeded grass two-tall lower
                            // The rare two-block plant is authored as one
                            // logical 16x32 sprite. The upper and lower atlas
                            // tiles use the same global Y coordinate, so a
                            // blade that crosses the split is mathematically
                            // continuous at the block boundary.
                            if (tileID >= 32) {
                                bool upperHalf = tileID <= 34;
                                int variant = upperHalf ? tileID - 32 : tileID - 35;
                                float spriteHeight = upperHalf
                                    ? static_cast<float>(31 - py)
                                    : static_cast<float>(15 - py);
                                float spriteNoise = randNoise(
                                    px,
                                    static_cast<int>(spriteHeight),
                                    500 + variant * 17
                                ) * 0.15f;
                                auto onTallBlade = [&](float baseX, float lean,
                                                       float bladeHeight, float width) {
                                    if (spriteHeight < 1.0f || spriteHeight > bladeHeight) return false;
                                    float t = spriteHeight / bladeHeight;
                                    float centerX = baseX + lean * t;
                                    float bladeWidth = width * (1.0f - 0.45f * t) *
                                        (upperHalf ? 0.85f : 1.0f);
                                    return std::abs((static_cast<float>(px) + 0.5f) - centerX) <= bladeWidth;
                                };

                                bool blade = false;
                                if (variant == 0) {
                                    blade = onTallBlade(2.4f, -1.2f, 22.0f, 0.9f) ||
                                        onTallBlade(5.2f, 1.3f, 31.0f, 1.05f) ||
                                        onTallBlade(8.0f, -0.7f, 31.0f, 1.2f) ||
                                        onTallBlade(10.7f, 1.1f, 27.0f, 1.0f) ||
                                        onTallBlade(13.2f, -1.0f, 24.0f, 0.9f);
                                } else if (variant == 1) {
                                    blade = onTallBlade(2.1f, 1.0f, 25.0f, 0.9f) ||
                                        onTallBlade(4.9f, -1.1f, 31.0f, 1.0f) ||
                                        onTallBlade(7.4f, 0.8f, 24.0f, 0.85f) ||
                                        onTallBlade(9.8f, -1.3f, 31.0f, 1.1f) ||
                                        onTallBlade(12.8f, 0.9f, 27.0f, 0.95f);
                                } else {
                                    blade = onTallBlade(2.8f, -0.9f, 23.0f, 0.9f) ||
                                        onTallBlade(5.8f, 1.2f, 29.0f, 1.0f) ||
                                        onTallBlade(8.1f, -0.8f, 31.0f, 1.1f) ||
                                        onTallBlade(10.9f, 1.0f, 26.0f, 0.95f) ||
                                        onTallBlade(13.3f, -0.7f, 21.0f, 0.85f);
                                }

                                float seedX = variant == 1 ? 9.8f : 8.0f;
                                bool seedHead = variant == 2 &&
                                    spriteHeight >= 27.0f && spriteHeight <= 31.0f &&
                                    std::abs((static_cast<float>(px) + 0.5f) - seedX) <= 1.0f;
                                bool rootedTuft = !upperHalf && spriteHeight <= 2.0f &&
                                    px >= 1 && px <= 14 && ((px + py + tileID) % 3 != 0);

                                if (blade || rootedTuft || seedHead) {
                                    float shade = spriteNoise * 0.8f +
                                        (((px * 3 + static_cast<int>(spriteHeight) + variant) % 7 == 0)
                                            ? 0.12f : 0.0f);
                                    // Use the exact same palette as the normal
                                    // grass sprites below. Only the sprite
                                    // layout differs between the two variants.
                                    uint8_t r = seedHead
                                        ? (uint8_t)std::clamp(150.0f + shade * 35.0f, 0.0f, 255.0f)
                                        : (uint8_t)std::clamp(30.0f + shade * 35.0f, 0.0f, 255.0f);
                                    uint8_t g = seedHead
                                        ? (uint8_t)std::clamp(170.0f + shade * 35.0f, 0.0f, 255.0f)
                                        : (uint8_t)std::clamp(118.0f + shade * 70.0f, 0.0f, 255.0f);
                                    uint8_t b = seedHead
                                        ? (uint8_t)std::clamp(55.0f + shade * 20.0f, 0.0f, 255.0f)
                                        : (uint8_t)std::clamp(30.0f + shade * 35.0f, 0.0f, 255.0f);
                                    setPixel(px, py, r, g, b, 255);
                                } else {
                                    setPixel(px, py, 0, 0, 0, 0);
                                }
                                break;
                            }

                            float height = static_cast<float>(15 - py);
                            bool upperHalf = tileID >= 32 && tileID <= 34;
                            bool specializedLower = tileID >= 35;
                            int grassVariant = specializedLower
                                ? (tileID - 35) % 3
                                : (tileID - 29) % 3;
                            auto onBlade = [&](float baseX, float lean, float bladeHeight, float width) {
                                if (height < 1.0f || height > bladeHeight) return false;
                                float t = height / bladeHeight;
                                float centerX = baseX + lean * t;
                                float upperWidthScale = upperHalf ? 0.85f : 1.0f;
                                float bladeWidth = width * (1.0f - 0.45f * t) * upperWidthScale;
                                return std::abs((static_cast<float>(px) + 0.5f) - centerX) <= bladeWidth;
                            };

                            bool blade = false;
                            bool seedHead = false;
                            if (grassVariant == 2) {
                                blade = onBlade(3.0f, 0.65f, 9.0f, 0.85f) ||
                                    onBlade(6.0f, -0.8f, 13.0f, 0.95f) ||
                                    onBlade(8.3f, 0.45f, 15.0f, 1.0f) ||
                                    onBlade(11.0f, -0.65f, 11.0f, 0.9f) ||
                                    onBlade(13.4f, 0.5f, 8.0f, 0.8f);
                                float seedX = (px < 8) ? 6.0f : 8.3f;
                                seedHead = height >= 12.0f && height <= 15.0f &&
                                    std::abs((static_cast<float>(px) + 0.5f) - seedX) <= 1.0f;
                            } else if (grassVariant == 0) {
                                blade = onBlade(2.5f, -0.8f, 8.0f, 0.85f) ||
                                    onBlade(5.3f, 0.9f, 12.0f, 1.0f) ||
                                    onBlade(8.0f, -0.45f, 15.0f, 1.1f) ||
                                    onBlade(10.7f, 0.75f, 10.0f, 0.9f) ||
                                    onBlade(13.2f, -0.7f, 7.0f, 0.8f);
                            } else {
                                blade = onBlade(2.2f, 0.7f, 10.0f, 0.9f) ||
                                    onBlade(4.8f, -0.55f, 14.0f, 0.95f) ||
                                    onBlade(7.2f, 0.35f, 9.0f, 0.8f) ||
                                    onBlade(9.6f, -0.75f, 15.0f, 1.05f) ||
                                    onBlade(12.8f, 0.55f, 11.0f, 0.9f);
                            }

                            // Upper sections stay narrower at their base so
                            // the two-block plant reads as one continuous stem.
                            // Opaque pixels at the bottom make the blades meet
                            // the soil instead of appearing to hover above it.
                            bool twoTallStem = specializedLower && height >= 6.0f && height <= 12.0f &&
                                px >= 7 && px <= 8;
                            bool rootedTuft = !upperHalf && height <= 2.0f && px >= 1 && px <= 14 &&
                                ((px + py + tileID) % 3 != 0);
                            if (blade || rootedTuft || seedHead || twoTallStem) {
                                float highlight = ((px * 3 + py + tileID) % 7 == 0) ? 1.0f : 0.0f;
                                float shade = n * 0.8f + highlight * 0.12f;
                                uint8_t r = 0;
                                uint8_t g = 0;
                                uint8_t b = 0;
                                if (seedHead) {
                                    r = (uint8_t)std::clamp(150.0f + shade * 35.0f, 0.0f, 255.0f);
                                    g = (uint8_t)std::clamp(170.0f + shade * 35.0f, 0.0f, 255.0f);
                                    b = (uint8_t)std::clamp(55.0f + shade * 20.0f, 0.0f, 255.0f);
                                } else {
                                    r = (uint8_t)std::clamp(30.0f + shade * 35.0f, 0.0f, 255.0f);
                                    g = (uint8_t)std::clamp(118.0f + shade * 70.0f, 0.0f, 255.0f);
                                    b = (uint8_t)std::clamp(30.0f + shade * 35.0f, 0.0f, 255.0f);
                                }
                                setPixel(px, py, r, g, b, 255);
                            } else {
                                setPixel(px, py, 0, 0, 0, 0);
                            }
                            break;
                        }
                        case 59: { // Snow
                            uint8_t value =
                                static_cast<uint8_t>(std::clamp(242.0f + n * 18.0f, 0.0f, 255.0f));

                            setPixel(
                                px,
                                py,
                                value,
                                value,
                                static_cast<uint8_t>(std::min(255, value + 3))
                            );
                            break;
                        }
                        default: { // Fallback magenta pattern
                            bool alt = (px + py) % 2 == 0;
                            setPixel(px, py, alt ? 255 : 0, 0, alt ? 255 : 0);
                            break;
                        }
                    }
                }
            }
        }
        for (int tileID = 60; tileID < MAX_TILES; ++tileID) {
            currentTileID = tileID;
            for (int py = 0; py < TILE_SIZE; ++py) {
                for (int px = 0; px < TILE_SIZE; ++px) {
                    bool alt = (px + py) % 2 == 0;
                    setPixel(px, py, alt ? 255 : 0, 0, alt ? 255 : 0, 255);
                }
            }
        }

        GLuint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texId);

        constexpr int mipLevels = 5; // 16, 8, 4, 2, 1
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipLevels, GL_RGBA8, TILE_SIZE, TILE_SIZE, MAX_TILES);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, TILE_SIZE, TILE_SIZE, MAX_TILES, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (epoxy_has_gl_extension("GL_EXT_texture_filter_anisotropic")) {
            GLfloat maxAniso = 0.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
            glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(maxAniso, 8.0f));
        }

        return texId;
    }
};

#endif // TEXTURE_ATLAS_HPP
