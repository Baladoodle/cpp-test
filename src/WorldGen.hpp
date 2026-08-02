#ifndef WORLD_GEN_HPP
#define WORLD_GEN_HPP

#include "MathUtils.hpp"
#include "SimplexNoise.hpp"
#include "Block.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

class WorldGen {
private:
    static inline int64_t floorDiv(int64_t a, int64_t b) {
        return (a >= 0) ? (a / b) : ((a - b + 1) / b);
    }

    static inline uint64_t treeHash(int64_t tx, int64_t tz) {
        uint64_t h = static_cast<uint64_t>(tx) * 0x9E3779B185EBCA87ULL ^ static_cast<uint64_t>(tz) * 0xC2B2AE3D27D4EB4FULL;
        h ^= h >> 30;
        h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 27;
        h *= 0x94D049BB133111EBULL;
        h ^= h >> 31;
        return h;
    }

    struct SurfaceCacheEntry {
        int64_t x = -9999999;
        int64_t z = -9999999;
        int64_t surfaceY = -999;
    };

    static inline int64_t getSurfaceYAtCached(int64_t wx, int64_t wz) {
        thread_local SurfaceCacheEntry cache[512];
        uint32_t slot = static_cast<uint32_t>((wx * 73856093LL ^ wz * 19349663LL) & 511);
        if (cache[slot].x == wx && cache[slot].z == wz) {
            return cache[slot].surfaceY;
        }
        int64_t sy = getSurfaceYAt(wx, wz);
        cache[slot] = { wx, wz, sy };
        return sy;
    }

    static inline int64_t getSurfaceYAt(int64_t wx, int64_t wz) {
        for (int64_t y = 250; y >= -50; y -= 4) {
            if (getDensity(wx, y, wz, 1) > 0.0f) {
                for (int64_t ry = y + 4; ry >= y; --ry) {
                    if (getDensity(wx, ry, wz, 1) > 0.0f && getDensity(wx, ry + 1, wz, 1) <= 0.0f) {
                        return ry;
                    }
                }
                return y;
            }
        }
        return -999;
    }
    static inline float distToSegmentSq(float px, float py, float pz, float x0, float y0, float z0, float x1, float y1, float z1) {
        float vx = x1 - x0, vy = y1 - y0, vz = z1 - z0;
        float wx = px - x0, wy = py - y0, wz = pz - z0;
        float c1 = wx * vx + wy * vy + wz * vz;
        if (c1 <= 0.0f) return wx * wx + wy * wy + wz * wz;
        float c2 = vx * vx + vy * vy + vz * vz;
        if (c2 <= c1) {
            float dx = px - x1, dy = py - y1, dz = pz - z1;
            return dx * dx + dy * dy + dz * dz;
        }
        float b = c1 / c2;
        float bx = x0 + b * vx, by = y0 + b * vy, bz = z0 + b * vz;
        float dx = px - bx, dy = py - by, dz = pz - bz;
        return dx * dx + dy * dy + dz * dz;
    }

    static inline bool inEllipsoid(float px, float py, float pz, float cx, float cy, float cz, float rx, float ry, float rz, uint64_t seed) {
        float dx = (px - cx) / rx;
        float dy = (py - cy) / ry;
        float dz = (pz - cz) / rz;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > 1.35f) return false;
        uint64_t pSeed = seed ^ treeHash(static_cast<int64_t>(px), static_cast<int64_t>(pz)) ^ (static_cast<int64_t>(py) * 1337);
        float noise = ((pSeed % 100) / 100.0f - 0.5f) * 0.35f;
        return (distSq + noise) <= 1.0f;
    }
    struct TreeCandidateCacheEntry {
        int64_t tx = 0;
        int64_t tz = 0;
        int64_t groundY = -999;
        uint64_t seed = 0;
        int roll = 0;
        uint8_t leafBlock = BLOCK_AIR;
        uint8_t logBlock = BLOCK_OAK_LOG;
        bool valid = false;
    };
public:
    // evaluates the tree block at one world position.
    struct TreeSite {
        int64_t tx;
        int64_t tz;
        int64_t groundY;
        uint64_t seed;
        int roll;
        uint8_t leafBlock;
        uint8_t logBlock;
    };

    static uint8_t evaluateTreeSite(
        const TreeSite& tree,
        int64_t wx,
        int64_t wy,
        int64_t wz,
        int scale
    ) {
        int64_t tx = tree.tx;
        int64_t tz = tree.tz;
        int64_t groundY = tree.groundY;
        uint64_t seed = tree.seed;
        int roll = tree.roll;
        uint8_t leafBlock = tree.leafBlock;
        uint8_t logBlock = tree.logBlock;

        float fwx = static_cast<float>(wx);
        float fwy = static_cast<float>(wy);
        float fwz = static_cast<float>(wz);

        if (scale >= 2) {
            int height = 8 + static_cast<int>((seed >> 8) % 7);
            if (roll < 6) height = 4;
            else if (roll >= 94) height = 20;

            if (wx == tx && wz == tz && wy > groundY && wy <= groundY + height) {
                return logBlock;
            }
            float cdx = static_cast<float>(wx - tx);
            float cdy = static_cast<float>(wy - (groundY + height - 2));
            float cdz = static_cast<float>(wz - tz);
            if (cdx * cdx + cdy * cdy * 1.2f + cdz * cdz <= 16.0f) {
                return leafBlock;
            }
            return BLOCK_AIR;
        }

        float ftx = static_cast<float>(tx);
        float fty = static_cast<float>(groundY);
        float ftz = static_cast<float>(tz);

        if (roll < 6) {
            int height = 3 + static_cast<int>((seed >> 8) % 3);
            float topY = fty + height;
            int64_t leanX = (seed & 1) ? 1 : 0;
            int64_t leanZ = ((seed >> 1) & 1) ? 1 : 0;

            if (wy > groundY && wy <= groundY + height) {
                int64_t curX = (wy > groundY + 2) ? (tx + leanX) : tx;
                int64_t curZ = (wy > groundY + 2) ? (tz + leanZ) : tz;
                if (wx == curX && wz == curZ) return logBlock;
            }
            if (inEllipsoid(fwx, fwy, fwz, ftx + leanX, topY, ftz + leanZ,
                    1.8f, 1.4f, 1.8f, seed + 1)) {
                return leafBlock;
            }
        } else if (roll >= 94) {
            int height = 18 + static_cast<int>((seed >> 8) % 9);
            float topY = fty + height;

            if (wx == tx && wz == tz && fwy > fty && fwy <= topY) return logBlock;

            int branchY1 = height - 12;
            int branchY2 = height - 7;
            float bLen1 = 5.5f;
            float bLen2 = 4.5f;

            if (distToSegmentSq(fwx, fwy, fwz, ftx + 1, fty + branchY1, ftz,
                    ftx + bLen1, fty + branchY1 + 2, ftz + 1) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz,
                    ftx - bLen1, fty + branchY1 + 2, ftz - 1) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz + 1,
                    ftx - 1, fty + branchY1 + 2, ftz + bLen1) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz,
                    ftx + 1, fty + branchY1 + 2, ftz - bLen1) <= 0.8f) return logBlock;

            if (distToSegmentSq(fwx, fwy, fwz, ftx + 1, fty + branchY2, ftz,
                    ftx + bLen2, fty + branchY2 + 2, ftz - 2) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY2, ftz,
                    ftx - bLen2, fty + branchY2 + 2, ftz + 2) <= 0.8f) return logBlock;

            if (inEllipsoid(fwx, fwy, fwz, ftx + bLen1, fty + branchY1 + 2,
                    ftz + 1, 3.2f, 2.2f, 3.2f, seed + 1)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx - bLen1, fty + branchY1 + 2,
                    ftz - 1, 3.2f, 2.2f, 3.2f, seed + 2)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx - 1, fty + branchY1 + 2,
                    ftz + bLen1, 3.2f, 2.2f, 3.2f, seed + 3)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx + 1, fty + branchY1 + 2,
                    ftz - bLen1, 3.2f, 2.2f, 3.2f, seed + 4)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx + bLen2, fty + branchY2 + 2,
                    ftz - 2, 3.2f, 2.2f, 3.2f, seed + 5)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx - bLen2, fty + branchY2 + 2,
                    ftz + 2, 3.2f, 2.2f, 3.2f, seed + 6)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx, topY + 2, ftz,
                    4.0f, 2.8f, 4.0f, seed + 7)) return leafBlock;
        } else {
            int height = 7 + static_cast<int>((seed >> 8) % 9);
            float topY = fty + height;
            float leanAngle = static_cast<float>((seed >> 12) % 360) * 0.0174533f;
            float leanAmount = 0.8f + static_cast<float>((seed >> 16) % 15) * 0.1f;

            for (int dy = 1; dy <= height; ++dy) {
                float progress = static_cast<float>(dy) / static_cast<float>(height);
                float curveX = std::sin(progress * 2.5f + leanAngle) * (progress * leanAmount);
                float curveZ = std::cos(progress * 2.5f + leanAngle) * (progress * leanAmount);
                int64_t curX = tx + static_cast<int64_t>(std::round(curveX));
                int64_t curZ = tz + static_cast<int64_t>(std::round(curveZ));
                if (wx == curX && wz == curZ && wy == groundY + dy) return logBlock;
            }

            float finalCurveX = std::sin(2.5f + leanAngle) * leanAmount;
            float finalCurveZ = std::cos(2.5f + leanAngle) * leanAmount;
            float tipX = ftx + finalCurveX;
            float tipZ = ftz + finalCurveZ;
            float branchY = topY - 3.0f;
            float br1X = tipX + 3.0f;
            float br1Z = tipZ + 1.5f;
            float br2X = tipX - 2.8f;
            float br2Z = tipZ - 2.0f;
            float br3X = tipX + 1.0f;
            float br3Z = tipZ - 3.2f;

            if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ,
                    br1X, topY - 1.0f, br1Z) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ,
                    br2X, topY - 1.0f, br2Z) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ,
                    br3X, topY - 1.0f, br3Z) <= 0.8f) return logBlock;

            if (inEllipsoid(fwx, fwy, fwz, br1X, topY - 1.0f, br1Z,
                    2.8f, 2.0f, 2.8f, seed + 1)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, br2X, topY - 1.0f, br2Z,
                    2.8f, 2.0f, 2.8f, seed + 2)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, br3X, topY - 1.0f, br3Z,
                    2.8f, 2.0f, 2.8f, seed + 3)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, tipX, topY + 1.2f, tipZ,
                    3.6f, 2.4f, 3.6f, seed + 4)) return leafBlock;
        }

        return BLOCK_AIR;
    }

    static void collectTreeSites(
        int64_t minWX,
        int64_t maxWX,
        int64_t minWY,
        int64_t maxWY,
        int64_t minWZ,
        int64_t maxWZ,
        std::vector<TreeSite>& outTrees
    ) {
        outTrees.clear();
        constexpr int64_t CELL_SIZE = 5;
        int64_t minCX = floorDiv(minWX - 6, CELL_SIZE);
        int64_t maxCX = floorDiv(maxWX + 6, CELL_SIZE);
        int64_t minCZ = floorDiv(minWZ - 6, CELL_SIZE);
        int64_t maxCZ = floorDiv(maxWZ + 6, CELL_SIZE);

        for (int64_t cz = minCZ; cz <= maxCZ; ++cz) {
            for (int64_t cx = minCX; cx <= maxCX; ++cx) {
                uint64_t hash =
                    static_cast<uint64_t>(cx) * 0x9E3779B185EBCA87ULL ^
                    static_cast<uint64_t>(cz) * 0xC2B2AE3D27D4EB4FULL;
                hash ^= hash >> 30;
                hash *= 0xBF58476D1CE4E5B9ULL;
                hash ^= hash >> 27;

                int64_t tx = cx * CELL_SIZE + static_cast<int64_t>(hash % CELL_SIZE);
                int64_t tz = cz * CELL_SIZE + static_cast<int64_t>((hash >> 8) % CELL_SIZE);
                if (tx < minWX - 6 || tx > maxWX + 6 ||
                    tz < minWZ - 6 || tz > maxWZ + 6) {
                    continue;
                }

                float lakeNoise = getLakeNoise(tx, tz);
                float floraPatchNoise = getFloraPatchNoise(tx, tz);
                if (lakeNoise >= 0.72f || floraPatchNoise <= 0.40f) continue;

                int64_t groundY = getSurfaceYAtCached(tx, tz);
                if (groundY <= -900 || maxWY < groundY || minWY > groundY + 30) continue;

                float aboveGroundDensity = getDensity(tx, groundY + 1, tz, 1);
                float groundDensity = getDensity(tx, groundY, tz, 1);
                if (groundDensity <= 0.0f || aboveGroundDensity > 0.0f) continue;

                uint64_t seed = treeHash(tx, tz);
                int roll = static_cast<int>(seed % 100);
                uint8_t leafShade[4] = {
                    BLOCK_LEAVES,
                    BLOCK_LEAVES_LIGHT,
                    BLOCK_LEAVES_DARK,
                    BLOCK_LEAVES_WARM
                };
                outTrees.push_back({
                    tx,
                    tz,
                    groundY,
                    seed,
                    roll,
                    leafShade[(seed >> 4) % 4],
                    BLOCK_OAK_LOG
                });
            }
        }
    }

    static uint8_t getTreeBlockFromSites(
        const std::vector<TreeSite>& trees,
        int64_t wx,
        int64_t wy,
        int64_t wz,
        int scale
    ) {
        if (wy < -40 || wy > 300) return BLOCK_AIR;
        for (const TreeSite& tree : trees) {
            if (std::abs(wx - tree.tx) > 6 ||
                std::abs(wz - tree.tz) > 6 ||
                wy < tree.groundY ||
                wy > tree.groundY + 30) {
                continue;
            }
            uint8_t block = evaluateTreeSite(tree, wx, wy, wz, scale);
            if (block != BLOCK_AIR) return block;
        }
        return BLOCK_AIR;
    }

    static uint8_t getTreeBlockAt(int64_t wx, int64_t wy, int64_t wz, int scale = 1) {
        if (wy < -40 || wy > 300) return BLOCK_AIR;

        constexpr int64_t cellSize = 5;
        int64_t cellX = floorDiv(wx, cellSize);
        int64_t cellZ = floorDiv(wz, cellSize);

        float fwx = static_cast<float>(wx);
        float fwy = static_cast<float>(wy);
        float fwz = static_cast<float>(wz);

        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                int64_t cx = cellX + dx;
                int64_t cz = cellZ + dz;

                uint64_t hash = static_cast<uint64_t>(cx) * 0x9E3779B185EBCA87ULL ^ static_cast<uint64_t>(cz) * 0xC2B2AE3D27D4EB4FULL;
                hash ^= hash >> 30;
                hash *= 0xBF58476D1CE4E5B9ULL;
                hash ^= hash >> 27;

                int targetX = static_cast<int>(hash % cellSize);
                int targetZ = static_cast<int>((hash >> 8) % cellSize);

                int64_t tx = cx * cellSize + targetX;
                int64_t tz = cz * cellSize + targetZ;

                if (std::abs(wx - tx) > 6 || std::abs(wz - tz) > 6) continue;

                static thread_local TreeCandidateCacheEntry cache[2048];
                uint32_t slot = static_cast<uint32_t>(
                    (tx * 73856093LL ^ tz * 19349663LL) & 2047
                );
                TreeCandidateCacheEntry& candidate = cache[slot];

                if (candidate.tx != tx || candidate.tz != tz) {
                    candidate.tx = tx;
                    candidate.tz = tz;
                    candidate.valid = false;
                    candidate.groundY = -999;

                    float lakeNoise = getLakeNoise(tx, tz);
                    float floraPatchNoise = getFloraPatchNoise(tx, tz);
                    if (lakeNoise < 0.72f && floraPatchNoise > 0.40f) {
                        int64_t cachedGroundY = getSurfaceYAtCached(tx, tz);
                        if (cachedGroundY > -900) {
                            float aboveGroundDensity = getDensity(tx, cachedGroundY + 1, tz, 1);
                            float groundDensity = getDensity(tx, cachedGroundY, tz, 1);
                            if (groundDensity > 0.0f && aboveGroundDensity <= 0.0f) {
                                candidate.groundY = cachedGroundY;
                                candidate.seed = treeHash(tx, tz);
                                candidate.roll = static_cast<int>(candidate.seed % 100);
                                uint8_t leafShade[4] = {
                                    BLOCK_LEAVES,
                                    BLOCK_LEAVES_LIGHT,
                                    BLOCK_LEAVES_DARK,
                                    BLOCK_LEAVES_WARM
                                };
                                candidate.leafBlock = leafShade[(candidate.seed >> 4) % 4];
                                candidate.logBlock = BLOCK_OAK_LOG;
                                candidate.valid = true;
                            }
                        }
                    }
                }

                if (!candidate.valid ||
                    wy < candidate.groundY ||
                    wy > candidate.groundY + 30) {
                    continue;
                }

                int64_t groundY = candidate.groundY;
                uint64_t seed = candidate.seed;
                int roll = candidate.roll;
                uint8_t leafBlock = candidate.leafBlock;
                uint8_t logBlock = candidate.logBlock;

                // Coarse LOD fast-path for scale >= 2 (LOD 1, 2, 3, 4)
                if (scale >= 2) {
                    int height = 8 + static_cast<int>((seed >> 8) % 7);
                    if (roll < 6) height = 4;
                    else if (roll >= 94) height = 20;

                    if (wx == tx && wz == tz && wy > groundY && wy <= groundY + height) {
                        return logBlock;
                    }
                    float cdx = static_cast<float>(wx - tx);
                    float cdy = static_cast<float>(wy - (groundY + height - 2));
                    float cdz = static_cast<float>(wz - tz);
                    if (cdx * cdx + cdy * cdy * 1.2f + cdz * cdz <= 16.0f) {
                        return leafBlock;
                    }
                    continue;
                }

                // LOD 0 Detail Oak Tree Generation
                float ftx = static_cast<float>(tx);
                float fty = static_cast<float>(groundY);
                float ftz = static_cast<float>(tz);

                if (roll < 6) {
                    // RARE TYPE 1: Tiny Sapling Oak (3-5 blocks tall, ~6% chance)
                    int height = 3 + static_cast<int>((seed >> 8) % 3);
                    float topY = fty + height;

                    int64_t leanX = (seed & 1) ? 1 : 0;
                    int64_t leanZ = ((seed >> 1) & 1) ? 1 : 0;

                    if (wy > groundY && wy <= groundY + height) {
                        int64_t curX = (wy > groundY + 2) ? (tx + leanX) : tx;
                        int64_t curZ = (wy > groundY + 2) ? (tz + leanZ) : tz;
                        if (wx == curX && wz == curZ) return logBlock;
                    }

                    if (inEllipsoid(fwx, fwy, fwz, ftx + leanX, topY, ftz + leanZ, 1.8f, 1.4f, 1.8f, seed + 1)) return leafBlock;
                }
                else if (roll >= 94) {
                    // RARE TYPE 2: Huge Elder Greatwood Oak (18-26 blocks tall, ~6% chance)
                    int height = 18 + static_cast<int>((seed >> 8) % 9);
                    float topY = fty + height;

                    if (wx == tx && wz == tz && fwy > fty && fwy <= topY) return logBlock;

                    int branchY1 = height - 12, branchY2 = height - 7;
                    float bLen1 = 5.5f, bLen2 = 4.5f;

                    if (distToSegmentSq(fwx, fwy, fwz, ftx + 1, fty + branchY1, ftz, ftx + bLen1, fty + branchY1 + 2, ftz + 1) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz, ftx - bLen1, fty + branchY1 + 2, ftz - 1) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz + 1, ftx - 1, fty + branchY1 + 2, ftz + bLen1) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz, ftx + 1, fty + branchY1 + 2, ftz - bLen1) <= 0.8f) return logBlock;

                    if (distToSegmentSq(fwx, fwy, fwz, ftx + 1, fty + branchY2, ftz, ftx + bLen2, fty + branchY2 + 2, ftz - 2) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY2, ftz, ftx - bLen2, fty + branchY2 + 2, ftz + 2) <= 0.8f) return logBlock;

                    if (inEllipsoid(fwx, fwy, fwz, ftx + bLen1, fty + branchY1 + 2, ftz + 1, 3.2f, 2.2f, 3.2f, seed + 1)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx - bLen1, fty + branchY1 + 2, ftz - 1, 3.2f, 2.2f, 3.2f, seed + 2)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx - 1, fty + branchY1 + 2, ftz + bLen1, 3.2f, 2.2f, 3.2f, seed + 3)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx + 1, fty + branchY1 + 2, ftz - bLen1, 3.2f, 2.2f, 3.2f, seed + 4)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx + bLen2, fty + branchY2 + 2, ftz - 2, 3.2f, 2.2f, 3.2f, seed + 5)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx - bLen2, fty + branchY2 + 2, ftz + 2, 3.2f, 2.2f, 3.2f, seed + 6)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx, topY + 2, ftz, 4.0f, 2.8f, 4.0f, seed + 7)) return leafBlock;
                }
                else {
                    // STANDARD VARIED OAK TREE (~88% of trees)
                    int height = 7 + static_cast<int>((seed >> 8) % 9);
                    float topY = fty + height;

                    float leanAngle = static_cast<float>((seed >> 12) % 360) * 0.0174533f;
                    float leanAmount = 0.8f + static_cast<float>((seed >> 16) % 15) * 0.1f;


                    for (int dy = 1; dy <= height; ++dy) {
                        float progress = static_cast<float>(dy) / static_cast<float>(height);
                        float curveX = std::sin(progress * 2.5f + leanAngle) * (progress * leanAmount);
                        float curveZ = std::cos(progress * 2.5f + leanAngle) * (progress * leanAmount);

                        int64_t curX = tx + static_cast<int64_t>(std::round(curveX));
                        int64_t curZ = tz + static_cast<int64_t>(std::round(curveZ));

                        if (wx == curX && wz == curZ && wy == groundY + dy) return logBlock;
                    }

                    float finalCurveX = std::sin(2.5f + leanAngle) * leanAmount;
                    float finalCurveZ = std::cos(2.5f + leanAngle) * leanAmount;
                    float tipX = ftx + finalCurveX;
                    float tipZ = ftz + finalCurveZ;

                    float branchY = topY - 3.0f;
                    float br1X = tipX + 3.0f, br1Z = tipZ + 1.5f;
                    float br2X = tipX - 2.8f, br2Z = tipZ - 2.0f;
                    float br3X = tipX + 1.0f, br3Z = tipZ - 3.2f;

                    if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ, br1X, topY - 1.0f, br1Z) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ, br2X, topY - 1.0f, br2Z) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ, br3X, topY - 1.0f, br3Z) <= 0.8f) return logBlock;

                    if (inEllipsoid(fwx, fwy, fwz, br1X, topY - 1.0f, br1Z, 2.8f, 2.0f, 2.8f, seed + 1)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, br2X, topY - 1.0f, br2Z, 2.8f, 2.0f, 2.8f, seed + 2)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, br3X, topY - 1.0f, br3Z, 2.8f, 2.0f, 2.8f, seed + 3)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, tipX, topY + 1.2f, tipZ, 3.6f, 2.4f, 3.6f, seed + 4)) return leafBlock;
                }
            }
        }
        return BLOCK_AIR;
    }
    // Anti-aliases high-frequency detail noise for coarse LOD scales (scale > 1)
    static float getDensity(int64_t wx, int64_t wy, int64_t wz, int scale = 1) {
        float fx = static_cast<float>(wx);
        float fy = static_cast<float>(wy);
        float fz = static_cast<float>(wz);

        // Domain warping (suppressed for very coarse LODs to prevent aliasing)
        // Domain warping (consistent across all LOD scales to prevent terrain shifting between LOD levels)
        float warpX = SimplexNoise::eval3D(fx * 0.005f, fy * 0.005f, fz * 0.005f) * 40.0f;
        float warpY = SimplexNoise::eval3D(fx * 0.005f + 100.0f, fy * 0.005f, fz * 0.005f) * 20.0f;
        float warpZ = SimplexNoise::eval3D(fx * 0.005f, fy * 0.005f, fz * 0.005f + 100.0f) * 40.0f;

        float wx_w = fx + warpX;
        float wy_w = fy + warpY;
        float wz_w = fz + warpZ;

        // Large scale 3D continent noise
        float nLarge = SimplexNoise::octave3D(wx_w * 0.004f, wy_w * 0.006f, wz_w * 0.004f, 3, 0.5f, 2.0f);

        // Low-frequency regional noise groups islands into broad cloud fields
        // instead of distributing similarly sized islands uniformly everywhere.
        float nMacro = SimplexNoise::octave3D(
            wx_w * 0.0012f,
            wy_w * 0.0020f,
            wz_w * 0.0012f,
            2,
            0.5f,
            2.0f
        );

        // Ridged secondary noise gives some islands spines, shelves, and
        // irregular silhouettes without changing the large island footprint.
        float nRidge = 1.0f - std::abs(SimplexNoise::eval3D(
            wx_w * 0.010f,
            wy_w * 0.012f,
            wz_w * 0.010f
        ));

        // Continuous 3D island field: there are no periodic vertical layers.
        // The formation threshold keeps the field open while the 3D noises
        // create isolated volumes at every height.
        float density = nLarge * 1.65f +
            nMacro * 0.35f +
            (nRidge - 0.45f) * 0.25f -
            0.75f;

        // Sparse low-frequency cavities create occasional arches and hollow
        // undersides while remaining part of the shared LOD density field.
        if (density > 0.15f) {
            float cavernNoise = SimplexNoise::eval3D(
                wx_w * 0.022f,
                wy_w * 0.022f,
                wz_w * 0.022f
            );
            if (cavernNoise > 0.68f) {
                density -= (cavernNoise - 0.68f) * 3.0f;
            }
        }

        // Fine detail 3D noise (low-pass filtered for coarse LOD scales)
        if (scale <= 4) {
            float nDetail = SimplexNoise::eval3D(wx_w * 0.02f, wy_w * 0.02f, wz_w * 0.02f) * 0.25f;
            density += nDetail;
        }

        return density;
    }

    // Broad 2D habitat fields used by post-terrain features. Keeping these
    // separate from density lets one biome contain distinct local habitats.
    static float getLakeNoise(int64_t wx, int64_t wz) {
        return SimplexNoise::octave3D(
            static_cast<float>(wx) * 0.0025f,
            0.0f,
            static_cast<float>(wz) * 0.0025f,
            2,
            0.5f,
            2.0f
        ) * 0.5f + 0.5f;
    }

    static float getFloraPatchNoise(int64_t wx, int64_t wz) {
        return SimplexNoise::octave3D(
            static_cast<float>(wx + 7919) * 0.0008f,
            0.0f,
            static_cast<float>(wz - 104729) * 0.0008f,
            2,
            0.5f,
            2.0f
        ) * 0.5f + 0.5f;
    }

    static float getFloraNoise(int64_t wx, int64_t wz) {
        float broad = getFloraPatchNoise(wx, wz) * 2.0f - 1.0f;
        float local = SimplexNoise::eval3D(
            static_cast<float>(wx) * 0.025f,
            0.0f,
            static_cast<float>(wz) * 0.025f
        );
        return std::clamp(0.5f + broad * 0.35f + local * 0.15f, 0.0f, 1.0f);
    }

    // Continuous habitat quality for grass. There is no hard patch boundary:
    // the value is mapped directly to the chance of placing a blade.
    static float getTallGrassHabitatNoise(int64_t wx, int64_t wz) {
        return SimplexNoise::octave3D(
            static_cast<float>(wx + 17321) * 0.012f,
            0.0f,
            static_cast<float>(wz - 9511) * 0.012f,
            3,
            0.5f,
            2.0f
        ) * 0.5f + 0.5f;
    }

    // Stable per-cell variation turns the continuous chance into actual
    // deterministic placements without adding world-state or save data.
    static float getTallGrassCellNoise(int64_t wx, int64_t wz) {
        uint64_t hash = static_cast<uint64_t>(wx) * 0x9E3779B185EBCA87ULL ^
            static_cast<uint64_t>(wz) * 0xC2B2AE3D27D4EB4FULL;
        hash ^= hash >> 30;
        hash *= 0xBF58476D1CE4E5B9ULL;
        hash ^= hash >> 27;
        hash *= 0x94D049BB133111EBULL;
        hash ^= hash >> 31;
        return static_cast<float>((hash >> 11) & 0x1FFFFFFFFFFFFFULL) /
            9007199254740992.0f;
    }

    // Keep zone boundaries deterministic in world space, but break up the
    // exact cutoff with a low-frequency field. The broad signal controls
    // where a zone lives; the transition noise only affects its edge, so
    // interiors remain stable and recognizable.
    static float getDesertBiomeNoise(int64_t wx, int64_t wz) {
        return SimplexNoise::eval3D(
            static_cast<float>(wx) * 0.001f,
            0.0f,
            static_cast<float>(wz) * 0.001f
        );
    }

    static bool isDesertZone(int64_t wx, int64_t wz) {
        constexpr float threshold = 0.4f;
        constexpr float transitionHalfWidth = 0.12f;
        float biomeNoise = getDesertBiomeNoise(wx, wz);

        if (biomeNoise <= threshold - transitionHalfWidth) return false;
        if (biomeNoise >= threshold + transitionHalfWidth) return true;

        float transitionNoise = SimplexNoise::octave3D(
            static_cast<float>(wx + 3749) * 0.018f,
            0.0f,
            static_cast<float>(wz - 9277) * 0.018f,
            3,
            0.5f,
            2.0f
        ) * 0.5f + 0.5f;
        float desertBlend = (biomeNoise - (threshold - transitionHalfWidth)) /
            (2.0f * transitionHalfWidth);
        return transitionNoise < desertBlend;
    }

    static bool isHighSkyZone(int64_t wx, int64_t wy, int64_t wz) {
        constexpr float boundaryY = 300.0f;
        constexpr float transitionHalfWidth = 8.0f;

        // Wobble the nominal altitude boundary in X/Z first. This gives the
        // zone a large readable shape instead of a perfectly level slice.
        float boundaryNoise = SimplexNoise::eval3D(
            static_cast<float>(wx - 6103) * 0.012f,
            0.0f,
            static_cast<float>(wz + 1187) * 0.012f
        );
        float boundary = boundaryY + boundaryNoise * 18.0f;
        float distance = static_cast<float>(wy) - boundary;

        if (distance <= -transitionHalfWidth) return false;
        if (distance >= transitionHalfWidth) return true;

        // Within the narrow edge band, select patches from a second field so
        // the boundary is ragged and interlocking instead of one voxel-wide.
        float transitionNoise = SimplexNoise::eval3D(
            static_cast<float>(wx + 14783) * 0.038f,
            static_cast<float>(wy - 3251) * 0.032f,
            static_cast<float>(wz - 4819) * 0.038f
        ) * 0.5f + 0.5f;
        float skyBlend = distance / (2.0f * transitionHalfWidth) + 0.5f;
        return transitionNoise < skyBlend;
    }

    // One jittered site per small world cell gives forests an even, natural
    // distribution without the clumping caused by local-noise maxima.
    static bool isTreeSite(int64_t wx, int64_t wz) {
        constexpr int64_t cellSize = 5;
        int64_t localX = wx % cellSize;
        int64_t localZ = wz % cellSize;
        if (localX < 0) localX += cellSize;
        if (localZ < 0) localZ += cellSize;

        int64_t cellX = (wx - localX) / cellSize;
        int64_t cellZ = (wz - localZ) / cellSize;
        uint64_t hash = static_cast<uint64_t>(cellX) * 0x9E3779B185EBCA87ULL;
        hash ^= static_cast<uint64_t>(cellZ) + 0xC2B2AE3D27D4EB4FULL +
            (hash << 6) + (hash >> 2);
        hash ^= hash >> 30;
        hash *= 0xBF58476D1CE4E5B9ULL;
        hash ^= hash >> 27;

        int targetX = static_cast<int>(hash % cellSize);
        int targetZ = static_cast<int>((hash >> 8) % cellSize);
        return localX == targetX && localZ == targetZ;
    }

    // Evaluates the block type at world position (wx, wy, wz)
    static uint8_t getBlockAt(int64_t wx, int64_t wy, int64_t wz, int scale = 1) {
        float density = getDensity(wx, wy, wz, scale);

        if (density <= 0.0f) {
            return getTreeBlockAt(wx, wy, wz, scale);
        }

        // Check block directly above to determine surface
        float aboveDensity = getDensity(wx, wy + scale, wz, scale);

        // High sky island biome with a noisy spatial transition around its
        // nominal altitude boundary.
        if (isHighSkyZone(wx, wy, wz)) {
            if (aboveDensity <= 0.0f) return BLOCK_SKY_QUARTZ;
            return BLOCK_STONE;
        }

        // Desert floating island biome with a noisy edge between regional
        // habitats instead of a single hard contour.
        bool isDesert = isDesertZone(wx, wz);

        if (aboveDensity <= 0.0f) {
            // Surface block
            if (isDesert) return BLOCK_SAND;
            return BLOCK_GRASS;
        }

        // Check depth below surface
        float above2Density = getDensity(wx, wy + 4 * scale, wz, scale);
        if (above2Density <= 0.0f) {
            if (isDesert) return BLOCK_SAND;
            return BLOCK_DIRT;
        }

        // Underside or deep interior crystal lights (rare, elegant illumination)
        if (scale <= 4) {
            float crystalNoise = SimplexNoise::eval3D(wx * 0.015f, wy * 0.015f, wz * 0.015f);
            if (crystalNoise > 0.78f && density < 0.2f) {
                return BLOCK_GLOW_CRYSTAL;
            }
        }

        return BLOCK_STONE;
    }
    static uint8_t getBlockAtWithDensities(
        int64_t wx,
        int64_t wy,
        int64_t wz,
        int scale,
        float density,
        float aboveDensity,
        float above2Density
    ) {
        if (density <= 0.0f) return BLOCK_AIR;

        if (isHighSkyZone(wx, wy, wz)) {
            if (aboveDensity <= 0.0f) return BLOCK_SKY_QUARTZ;
            return BLOCK_STONE;
        }

        bool isDesert = isDesertZone(wx, wz);

        if (aboveDensity <= 0.0f) {
            if (isDesert) return BLOCK_SAND;
            return BLOCK_GRASS;
        }

        if (above2Density <= 0.0f) {
            if (isDesert) return BLOCK_SAND;
            return BLOCK_DIRT;
        }

        if (scale <= 4) {
            float crystalNoise = SimplexNoise::eval3D(
                static_cast<float>(wx) * 0.015f,
                static_cast<float>(wy) * 0.015f,
                static_cast<float>(wz) * 0.015f
            );
            if (crystalNoise > 0.78f && density < 0.2f) return BLOCK_GLOW_CRYSTAL;
        }

        return BLOCK_STONE;
    }
};

#endif // WORLD_GEN_HPP
