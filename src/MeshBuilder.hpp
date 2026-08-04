#ifndef MESH_BUILDER_HPP
#define MESH_BUILDER_HPP

#include "Chunk.hpp"
#include "Block.hpp"
#include "MathUtils.hpp"
#include <array>
#include <vector>
#include <queue>
#include <tuple>
#include <cstdint>
#include <algorithm>

struct MeshingNeighborhood {
    std::array<uint8_t, PADDED_VOL> blocks{};
    std::array<uint16_t, PADDED_VOL> light{};

    inline uint8_t block(int x, int y, int z) const {
        if (x < -1 || x > CHUNK_SIZE ||
            y < -1 || y > CHUNK_SIZE ||
            z < -1 || z > CHUNK_SIZE) {
            return BLOCK_AIR;
        }
        return blocks[getPaddedVoxelIndex(x, y, z)];
    }

    inline uint16_t lightAt(int x, int y, int z) const {
        if (x < -1 || x > CHUNK_SIZE ||
            y < -1 || y > CHUNK_SIZE ||
            z < -1 || z > CHUNK_SIZE) {
            int bx = std::clamp(x, 0, CHUNK_SIZE - 1);
            int by = std::clamp(y, 0, CHUNK_SIZE - 1);
            int bz = std::clamp(z, 0, CHUNK_SIZE - 1);
            return light[getPaddedVoxelIndex(bx, by, bz)];
        }
        return light[getPaddedVoxelIndex(x, y, z)];
    }
};

class MeshBuilder {
private:
    struct LightNode {
        int8_t x, y, z;
    };

    struct MeshScratch {
        std::vector<uint8_t> paddedBlocks;
        std::vector<uint16_t> paddedLight;
        std::vector<LightNode> lightQueue;
    };

    inline static thread_local MeshScratch threadScratch;

    inline static uint8_t getScratchPaddedBlock(const Chunk& chunk, int x, int y, int z) {
        if (threadScratch.paddedBlocks.empty()) return chunk.getBlock(x, y, z);
        if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) return BLOCK_AIR;
        return threadScratch.paddedBlocks[getPaddedVoxelIndex(x, y, z)];
    }

    inline static uint16_t getScratchPaddedLight(const Chunk& chunk, int x, int y, int z) {
        if (threadScratch.paddedLight.empty()) return chunk.getPaddedLight(x, y, z);
        if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) {
            return chunk.getPaddedLight(x, y, z);
        }
        return threadScratch.paddedLight[getPaddedVoxelIndex(x, y, z)];
    }

    inline static void setScratchPaddedLight(int x, int y, int z, uint16_t l) {
        if (threadScratch.paddedLight.empty()) return;
        if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) return;
        threadScratch.paddedLight[getPaddedVoxelIndex(x, y, z)] = l;
    }

    static void propagateLight3D(Chunk& chunk) {
        if (chunk.isEmpty) return;

        threadScratch.paddedLight.assign(PADDED_VOL, 0);
        threadScratch.lightQueue.clear();
        threadScratch.lightQueue.reserve(4096);

        // Seed emissive blocks and direct skylight inside this chunk only.
        // Cross-chunk propagation is coalesced by ChunkManager after all
        // generated blocks are available.
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                uint8_t skyVal = 15;
                for (int y = CHUNK_SIZE - 1; y >= 0; --y) {
                    uint8_t block = getScratchPaddedBlock(chunk, x, y, z);
                    const BlockInfo& info = getBlockInfo(block);
                    if (!info.isTransparent) skyVal = 0;

                    uint8_t r = info.lightR;
                    uint8_t g = info.lightG;
                    uint8_t b = info.lightB;
                    if (r > 0 || g > 0 || b > 0 || skyVal > 0) {
                        setScratchPaddedLight(x, y, z, packLight(r, g, b, skyVal));
                        threadScratch.lightQueue.push_back({
                            static_cast<int8_t>(x),
                            static_cast<int8_t>(y),
                            static_cast<int8_t>(z)
                        });
                    }
                }
            }
        }

        const int dx[6] = { 1, -1,  0,  0,  0,  0 };
        const int dy[6] = { 0,  0,  1, -1,  0,  0 };
        const int dz[6] = { 0,  0,  0,  0,  1, -1 };

        size_t head = 0;
        while (head < threadScratch.lightQueue.size()) {
            LightNode curr = threadScratch.lightQueue[head++];
            uint16_t currLight = getScratchPaddedLight(chunk, curr.x, curr.y, curr.z);
            uint8_t cr = getLightR(currLight);
            uint8_t cg = getLightG(currLight);
            uint8_t cb = getLightB(currLight);
            uint8_t csky = getLightSky(currLight);

            for (int i = 0; i < 6; ++i) {
                int nx = curr.x + dx[i];
                int ny = curr.y + dy[i];
                int nz = curr.z + dz[i];
                if (nx < 0 || nx >= CHUNK_SIZE ||
                    ny < 0 || ny >= CHUNK_SIZE ||
                    nz < 0 || nz >= CHUNK_SIZE) {
                    continue;
                }

                uint8_t nBlock = getScratchPaddedBlock(chunk, nx, ny, nz);
                if (!getBlockInfo(nBlock).isTransparent) continue;

                uint16_t nLight = getScratchPaddedLight(chunk, nx, ny, nz);
                uint8_t nr = getLightR(nLight);
                uint8_t ng = getLightG(nLight);
                uint8_t nb = getLightB(nLight);
                uint8_t nsky = getLightSky(nLight);

                uint8_t tr = (cr > 1) ? cr - 1 : 0;
                uint8_t tg = (cg > 1) ? cg - 1 : 0;
                uint8_t tb = (cb > 1) ? cb - 1 : 0;
                uint8_t tsky = (csky > 1) ? csky - 1 : 0;

                bool updated = false;
                if (tr > nr) { nr = tr; updated = true; }
                if (tg > ng) { ng = tg; updated = true; }
                if (tb > nb) { nb = tb; updated = true; }
                if (tsky > nsky) { nsky = tsky; updated = true; }

                if (updated) {
                    setScratchPaddedLight(nx, ny, nz, packLight(nr, ng, nb, nsky));
                    threadScratch.lightQueue.push_back({
                        static_cast<int8_t>(nx),
                        static_cast<int8_t>(ny),
                        static_cast<int8_t>(nz)
                    });
                }
            }
        }

        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    chunk.setLight(x, y, z, getScratchPaddedLight(chunk, x, y, z));
                }
            }
        }
    }

    // Helper to calculate vertex AO based on adjacent solid blocks
    static float calculateAO(bool side1, bool side2, bool corner) {
        if (side1 && side2) return 0.25f; // Darkest corner
        int count = (side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0);
        return 1.0f - (count * 0.22f);
    }

public:
    // rebuild the border and light data used by the worker mesher.
    static void finalizeVoxelData(
        Chunk& chunk,
        const std::vector<float>* densityGrid = nullptr,
        MeshingNeighborhood* neighborhood = nullptr
    ) {
        bool hasSolid = false;
        for (int z = 0; z < CHUNK_SIZE && !hasSolid; ++z) {
            for (int y = 0; y < CHUNK_SIZE && !hasSolid; ++y) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    if (chunk.getBlock(x, y, z) != BLOCK_AIR) {
                        hasSolid = true;
                        break;
                    }
                }
            }
        }

        chunk.isEmpty = !hasSolid;
        if (chunk.isEmpty) {
            threadScratch.paddedBlocks.clear();
            threadScratch.paddedLight.clear();
            std::fill(chunk.light, chunk.light + CHUNK_VOL, 0);
            if (neighborhood) {
                neighborhood->blocks.fill(BLOCK_AIR);
                neighborhood->light.fill(0);
            }
            chunk.isGenerated = true;
            return;
        }

        int scale = chunk.scale;
        int64_t wmx = chunk.worldMin.x;
        int64_t wmy = chunk.worldMin.y;
        int64_t wmz = chunk.worldMin.z;
        threadScratch.paddedBlocks.resize(PADDED_VOL);

        constexpr int GRID_DX = CHUNK_SIZE + 7; // 39
        constexpr int GRID_DZ = CHUNK_SIZE + 7; // 39
        auto gridIndex = [](int x, int y, int z) {
            return ((y + 1) * GRID_DZ + (z + 3)) * GRID_DX + (x + 3);
        };

        auto sampleDensity = [&](int x, int y, int z) {
            return (*densityGrid)[gridIndex(
                std::clamp(x, -3, 35),
                std::clamp(y, -1, 37),
                std::clamp(z, -3, 35)
            )];
        };

        for (int y = -1; y <= CHUNK_SIZE; ++y) {
            for (int z = -1; z <= CHUNK_SIZE; ++z) {
                for (int x = -1; x <= CHUNK_SIZE; ++x) {
                    uint8_t block = BLOCK_AIR;
                    bool inside = x >= 0 && x < CHUNK_SIZE &&
                        y >= 0 && y < CHUNK_SIZE &&
                        z >= 0 && z < CHUNK_SIZE;
                    if (inside) {
                        block = chunk.getBlock(x, y, z);
                    } else if (densityGrid) {
                        float density = sampleDensity(x, y, z);
                        if (density <= 0.0f) {
                            block = BLOCK_AIR;
                        } else {
                            float aboveDensity = sampleDensity(x, y + 1, z);
                            float above2Density = sampleDensity(x, y + 4, z);

                            float nearXPos = sampleDensity(x + 1, y, z);
                            float nearXNeg = sampleDensity(x - 1, y, z);
                            float nearZPos = sampleDensity(x, y, z + 1);
                            float nearZNeg = sampleDensity(x, y, z - 1);
                            float farXPos = sampleDensity(x + 3, y, z);
                            float farXNeg = sampleDensity(x - 3, y, z);
                            float farZPos = sampleDensity(x, y, z + 3);
                            float farZNeg = sampleDensity(x, y, z - 3);

                            float sharpness = WorldGen::getDeepStoneSharpnessFromValues(
                                density, nearXPos, nearXNeg, nearZPos, nearZNeg,
                                farXPos, farXNeg, farZPos, farZNeg
                            );

                            int64_t wx = wmx + x * scale + scale / 2;
                            int64_t wy = wmy + y * scale + scale / 2;
                            int64_t wz = wmz + z * scale + scale / 2;
                            block = WorldGen::getBlockAtWithDensitiesAndSharpness(
                                wx, wy, wz, scale, density, aboveDensity, above2Density, sharpness
                            );
                        }
                    } else {
                        int64_t wx = wmx + x * scale + scale / 2;
                        int64_t wy = wmy + y * scale + scale / 2;
                        int64_t wz = wmz + z * scale + scale / 2;
                        block = WorldGen::getBlockAt(wx, wy, wz, scale);
                    }
                    threadScratch.paddedBlocks[getPaddedVoxelIndex(x, y, z)] = block;
                }
            }
        }
        propagateLight3D(chunk);
        if (neighborhood) {
            std::copy(
                threadScratch.paddedBlocks.begin(),
                threadScratch.paddedBlocks.end(),
                neighborhood->blocks.begin()
            );
            std::copy(
                threadScratch.paddedLight.begin(),
                threadScratch.paddedLight.end(),
                neighborhood->light.begin()
            );
        }
        chunk.isGenerated = true;
    }

    static void generateVoxelData(Chunk& chunk, MeshingNeighborhood* neighborhood = nullptr) {
        bool hasSolid = false;
        int scale = chunk.scale;
        int64_t wmx = chunk.worldMin.x;
        int64_t wmy = chunk.worldMin.y;
        int64_t wmz = chunk.worldMin.z;

        constexpr int GRID_SIZE = 39; // ix, iy, iz: 0 to 38
        constexpr int GRID_VOL = GRID_SIZE * GRID_SIZE * GRID_SIZE; // 59,319

        thread_local std::vector<float> densityGrid;
        densityGrid.resize(GRID_VOL);

        auto gridIndex = [](int x, int y, int z) {
            return ((y + 1) * GRID_SIZE + (z + 3)) * GRID_SIZE + (x + 3);
        };

        // Evaluate every coarse voxel center so the density field has no
        // synthetic lattice to imprint on distant terrain.
        for (int iy = 0; iy < GRID_SIZE; ++iy) {
            int y = iy - 1;
            int64_t wy = wmy + y * scale + scale / 2;
            for (int iz = 0; iz < GRID_SIZE; ++iz) {
                int z = iz - 3;
                int64_t wz = wmz + z * scale + scale / 2;
                for (int ix = 0; ix < GRID_SIZE; ++ix) {
                    int x = ix - 3;
                    int64_t wx = wmx + x * scale + scale / 2;
                    densityGrid[gridIndex(x, y, z)] =
                        WorldGen::getDensity(wx, wy, wz, scale);
                }
            }
        }
        int64_t minSampleX = wmx + scale / 2;
        int64_t maxSampleX = wmx + (CHUNK_SIZE - 1) * scale + scale / 2;
        int64_t minSampleY = wmy + scale / 2;
        int64_t maxSampleY = wmy + (CHUNK_SIZE - 1) * scale + scale / 2;
        int64_t minSampleZ = wmz + scale / 2;
        int64_t maxSampleZ = wmz + (CHUNK_SIZE - 1) * scale + scale / 2;

        thread_local std::vector<WorldGen::TreeSite> treeSites;
        WorldGen::collectTreeSites(
            minSampleX,
            maxSampleX,
            minSampleY,
            maxSampleY,
            minSampleZ,
            maxSampleZ,
            treeSites
        );

        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    int64_t wx = wmx + x * scale + scale / 2;
                    int64_t wy = wmy + y * scale + scale / 2;
                    int64_t wz = wmz + z * scale + scale / 2;
                    float density = densityGrid[gridIndex(x, y, z)];
                    if (density <= 0.0f) {
                        chunk.setBlock(x, y, z, BLOCK_AIR);
                        continue;
                    }
                    float aboveDensity = densityGrid[gridIndex(x, y + 1, z)];
                    float above2Density = densityGrid[gridIndex(x, y + 4, z)];

                    float nearXPos = densityGrid[gridIndex(x + 1, y, z)];
                    float nearXNeg = densityGrid[gridIndex(x - 1, y, z)];
                    float nearZPos = densityGrid[gridIndex(x, y, z + 1)];
                    float nearZNeg = densityGrid[gridIndex(x, y, z - 1)];
                    float farXPos = densityGrid[gridIndex(x + 3, y, z)];
                    float farXNeg = densityGrid[gridIndex(x - 3, y, z)];
                    float farZPos = densityGrid[gridIndex(x, y, z + 3)];
                    float farZNeg = densityGrid[gridIndex(x, y, z - 3)];

                    float sharpness = WorldGen::getDeepStoneSharpnessFromValues(
                        density, nearXPos, nearXNeg, nearZPos, nearZNeg,
                        farXPos, farXNeg, farZPos, farZNeg
                    );

                    uint8_t block = WorldGen::getBlockAtWithDensitiesAndSharpness(
                        wx, wy, wz, scale, density, aboveDensity, above2Density, sharpness
                    );
                    chunk.setBlock(x, y, z, block);

                    if (block != BLOCK_AIR) {
                        hasSolid = true;
                    }
                }
            }
        }

        auto floorDiv = [](int64_t a, int64_t b) {
            int64_t quotient = a / b;
            int64_t remainder = a % b;
            if (remainder != 0 && ((a < 0) ^ (b < 0))) --quotient;
            return quotient;
        };
        auto ceilDiv = [&](int64_t a, int64_t b) {
            return -floorDiv(-a, b);
        };
        auto cellRange = [&](int64_t minWorld, int64_t maxWorld, int64_t worldOrigin) {
            int64_t minCell = ceilDiv(minWorld - worldOrigin - scale / 2, scale);
            int64_t maxCell = floorDiv(maxWorld - worldOrigin - scale / 2, scale);
            return std::pair<int, int>(
                static_cast<int>(std::max<int64_t>(0, minCell)),
                static_cast<int>(std::min<int64_t>(CHUNK_SIZE - 1, maxCell))
            );
        };

        // Evaluate each tree only inside its world-space footprint. The old
        // voxel-first pass tested every empty voxel against every nearby tree.
        for (const WorldGen::TreeSite& tree : treeSites) {
            auto xRange = cellRange(
                tree.tx - WorldGen::TREE_MAX_RADIUS,
                tree.tx + WorldGen::TREE_MAX_RADIUS,
                wmx
            );
            auto yRange = cellRange(tree.groundY, tree.groundY + 32, wmy);
            auto zRange = cellRange(
                tree.tz - WorldGen::TREE_MAX_RADIUS,
                tree.tz + WorldGen::TREE_MAX_RADIUS,
                wmz
            );
            if (xRange.first > xRange.second ||
                yRange.first > yRange.second ||
                zRange.first > zRange.second) {
                continue;
            }

            for (int z = zRange.first; z <= zRange.second; ++z) {
                for (int y = yRange.first; y <= yRange.second; ++y) {
                    for (int x = xRange.first; x <= xRange.second; ++x) {
                        if (densityGrid[gridIndex(x, y, z)] > 0.0f ||
                            chunk.getBlock(x, y, z) != BLOCK_AIR) {
                            continue;
                        }
                        int64_t wx = wmx + x * scale + scale / 2;
                        int64_t wy = wmy + y * scale + scale / 2;
                        int64_t wz = wmz + z * scale + scale / 2;
                        uint8_t block = WorldGen::evaluateTreeSite(
                            tree, wx, wy, wz, scale
                        );
                        if (block != BLOCK_AIR) {
                            chunk.setBlock(x, y, z, block);
                            hasSolid = true;
                        }
                    }
                }
            }
        }

        // Post-terrain feature pass. Keep features at LOD 0 for readable
        // detail and keep them away from chunk edges so neighboring chunks do
        // not need a second feature-generation dependency.
        if (scale == 1) {
            auto findSurfaceY = [&](int x, int z) -> int {
                for (int y = CHUNK_SIZE - 1; y >= 0; --y) {
                    uint8_t block = chunk.getBlock(x, y, z);
                    uint8_t above = y + 1 < CHUNK_SIZE
                        ? chunk.getBlock(x, y + 1, z)
                        : WorldGen::getBlockAt(wmx + x, wmy + y + 1, wmz + z, 1);
                    if (block != BLOCK_AIR && block != BLOCK_WATER && above == BLOCK_AIR) {
                        return y;
                    }
                }
                return -1;
            };

            int surfaceHeights[CHUNK_SIZE * CHUNK_SIZE];
            std::fill(surfaceHeights, surfaceHeights + CHUNK_SIZE * CHUNK_SIZE, -1);
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    surfaceHeights[z * CHUNK_SIZE + x] = findSurfaceY(x, z);
                }
            }

            auto surfaceAt = [&](int x, int z) -> int {
                if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return -1;
                return surfaceHeights[z * CHUNK_SIZE + x];
            };

            for (int z = 3; z < CHUNK_SIZE - 3; ++z) {
                for (int x = 3; x < CHUNK_SIZE - 3; ++x) {
                    int surfaceY = surfaceAt(x, z);
                    if (surfaceY < 1) continue;

                    int64_t wx = wmx + x;
                    int64_t wz = wmz + z;
                    float lakeNoise = WorldGen::getLakeNoise(wx, wz);
                    bool lakePeak = lakeNoise > WorldGen::getLakeNoise(wx + 12, wz) &&
                        lakeNoise > WorldGen::getLakeNoise(wx - 12, wz) &&
                        lakeNoise > WorldGen::getLakeNoise(wx, wz + 12) &&
                        lakeNoise > WorldGen::getLakeNoise(wx, wz - 12);

                    // A lake starts at a local depression: the surrounding
                    // ring must be fully sampled and higher than the center.
                    if (chunk.getBlock(x, surfaceY, z) == BLOCK_GRASS &&
                        lakeNoise > 0.64f && lakePeak) {
                        int rimMinY = CHUNK_SIZE;
                        bool completeRim = true;
                        for (int dz = -3; dz <= 3; ++dz) {
                            for (int dx = -3; dx <= 3; ++dx) {
                                if (std::max(std::abs(dx), std::abs(dz)) != 3) continue;
                                int rimY = surfaceAt(x + dx, z + dz);
                                if (rimY < 0) completeRim = false;
                                else rimMinY = std::min(rimMinY, rimY);
                            }
                        }

                        bool completeBasin = true;
                        bool flatBasin = true;
                        for (int dz = -2; dz <= 2 && completeBasin; ++dz) {
                            for (int dx = -2; dx <= 2; ++dx) {
                                int basinY = surfaceAt(x + dx, z + dz);
                                if (basinY < 0 || basinY > surfaceY) {
                                    completeBasin = false;
                                    break;
                                }
                                if (basinY != surfaceY) flatBasin = false;
                            }
                        }

                        bool naturalBasin = completeBasin && !flatBasin && rimMinY >= surfaceY + 1;
                        bool plateauPond = flatBasin && rimMinY >= surfaceY;
                        if (completeRim && (naturalBasin || plateauPond)) {
                            int waterLevel = std::min(surfaceY + 1, rimMinY);
                            for (int dz = -2; dz <= 2; ++dz) {
                                for (int dx = -2; dx <= 2; ++dx) {
                                    int basinY = surfaceAt(x + dx, z + dz);
                                    if (basinY < 0 || basinY >= waterLevel) continue;

                                    if (flatBasin && chunk.getBlock(x + dx, basinY, z + dz) == BLOCK_GRASS) {
                                        // Cut a shallow, level pond into a
                                        // grass plateau instead of floating
                                        // water above an arbitrary ledge.
                                        chunk.setBlock(x + dx, basinY, z + dz, BLOCK_WATER);
                                    } else {
                                        for (int y = basinY + 1; y <= waterLevel; ++y) {
                                            if (chunk.getBlock(x + dx, y, z + dz) == BLOCK_AIR) {
                                                chunk.setBlock(x + dx, y, z + dz, BLOCK_WATER);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Lightweight vegetation pass. Tall grass is deliberately kept
            // at LOD 0: it is visual detail, not part of the terrain shell.
            // The surface lookup is from before the lake edits, so checking
            // the current block and its air-filled cell also keeps plants out
            // of ponds and tree canopies.
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    int surfaceY = surfaceAt(x, z);
                    if (surfaceY < 0 || surfaceY + 1 >= CHUNK_SIZE) continue;
                    if (chunk.getBlock(x, surfaceY, z) != BLOCK_GRASS ||
                        chunk.getBlock(x, surfaceY + 1, z) != BLOCK_AIR) {
                        continue;
                    }

                    int64_t wx = wmx + x;
                    int64_t wz = wmz + z;
                    if (WorldGen::getLakeNoise(wx, wz) >= 0.72f) continue;

                    // Map habitat quality directly to placement chance:
                    // poor areas still get ~8% coverage, while the best areas
                    // approach ~80% without ever becoming guaranteed carpets.
                    float grassChance = 0.08f +
                        WorldGen::getTallGrassHabitatNoise(wx, wz) * 0.72f;
                    if (WorldGen::getTallGrassCellNoise(wx, wz) > grassChance) {
                        continue;
                    }
                    chunk.setBlock(x, surfaceY + 1, z, BLOCK_TALL_GRASS);

                    // Most grass stays one block high. A second independent
                    // deterministic roll makes roughly one in ten plants a
                    // rare two-block variant without changing the habitat
                    // coverage itself.
                    bool rareTwoTall = WorldGen::getTallGrassCellNoise(
                        wx + 104729,
                        wz - 7919
                    ) < 0.10f;
                    if (rareTwoTall && surfaceY + 2 < CHUNK_SIZE &&
                        chunk.getBlock(x, surfaceY + 2, z) == BLOCK_AIR) {
                        chunk.setBlock(x, surfaceY + 2, z, BLOCK_TALL_GRASS_TOP);
                    }
                }
            }
        }
        // Recompute after lakes/flora have been applied.
        finalizeVoxelData(chunk, &densityGrid, neighborhood);
    }

    static void updateNeighborhood(
        const Chunk& chunk,
        MeshingNeighborhood& neighborhood,
        const std::array<std::shared_ptr<Chunk>, 6>& neighbors,
        bool rebuildBlocks = false
    ) {
        if (rebuildBlocks) {
            neighborhood.blocks.fill(BLOCK_AIR);
        }
        neighborhood.light.fill(0);

        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    neighborhood.blocks[getPaddedVoxelIndex(x, y, z)] =
                        chunk.getBlock(x, y, z);
                    neighborhood.light[getPaddedVoxelIndex(x, y, z)] =
                        chunk.getLight(x, y, z);
                }
            }
        }

        for (int y = -1; y <= CHUNK_SIZE; ++y) {
            for (int z = -1; z <= CHUNK_SIZE; ++z) {
                for (int x = -1; x <= CHUNK_SIZE; ++x) {
                    int outsideCount = (x < 0 || x >= CHUNK_SIZE ? 1 : 0) +
                        (y < 0 || y >= CHUNK_SIZE ? 1 : 0) +
                        (z < 0 || z >= CHUNK_SIZE ? 1 : 0);
                    if (outsideCount == 0) continue;

                    int paddedIndex = getPaddedVoxelIndex(x, y, z);
                    if (outsideCount == 1) {
                        int direction = -1;
                        int nx = x;
                        int ny = y;
                        int nz = z;
                        if (x < 0) {
                            direction = DIR_NEG_X;
                            nx = CHUNK_SIZE - 1;
                        } else if (x >= CHUNK_SIZE) {
                            direction = DIR_POS_X;
                            nx = 0;
                        } else if (y < 0) {
                            direction = DIR_NEG_Y;
                            ny = CHUNK_SIZE - 1;
                        } else if (y >= CHUNK_SIZE) {
                            direction = DIR_POS_Y;
                            ny = 0;
                        } else if (z < 0) {
                            direction = DIR_NEG_Z;
                            nz = CHUNK_SIZE - 1;
                        } else {
                            direction = DIR_POS_Z;
                            nz = 0;
                        }

                        const std::shared_ptr<Chunk>& neighbor = neighbors[direction];
                        bool neighborReady = neighbor &&
                            neighbor->resident.load(std::memory_order_acquire) &&
                            neighbor->isGenerated.load(std::memory_order_acquire);
                        if (neighborReady) {
                            neighborhood.blocks[paddedIndex] =
                                neighbor->getBlock(nx, ny, nz);
                            neighborhood.light[paddedIndex] =
                                neighbor->getLight(nx, ny, nz);
                            continue;
                        }
                    }

                    // Fallback for missing/unready neighbor or diagonal padded cell:
                    // Inherit light from the nearest valid boundary voxel of chunk.
                    int bx = std::clamp(x, 0, CHUNK_SIZE - 1);
                    int by = std::clamp(y, 0, CHUNK_SIZE - 1);
                    int bz = std::clamp(z, 0, CHUNK_SIZE - 1);
                    uint16_t bLight = chunk.getLight(bx, by, bz);
                    if (bLight == 0 && y >= 0 && getBlockInfo(neighborhood.blocks[paddedIndex]).isTransparent) {
                        bLight = packLight(0, 0, 0, 15);
                    }
                    neighborhood.light[paddedIndex] = bLight;
                }
            }
        }
    }

    static void buildMesh(Chunk& chunk, const MeshingNeighborhood* neighborhood = nullptr) {
        chunk.stagedIndices.clear();
        chunk.stagedVertices.reserve(8192);
        chunk.stagedIndices.reserve(12288);
        if (chunk.isEmpty) {
            chunk.isMeshStaged = true;
            return;
        }

        int scale = chunk.scale;
        float fScale = static_cast<float>(scale);
        int64_t wmx = chunk.worldMin.x;
        int64_t wmy = chunk.worldMin.y;
        int64_t wmz = chunk.worldMin.z;

        auto getNeighborhoodBlock = [&](int x, int y, int z) -> uint8_t {
            return neighborhood
                ? neighborhood->block(x, y, z)
                : chunk.getPaddedBlock(x, y, z);
        };
        auto getNeighborhoodLight = [&](int x, int y, int z) -> uint16_t {
            return neighborhood
                ? neighborhood->lightAt(x, y, z)
                : chunk.getPaddedLight(x, y, z);
        };
        auto isSolidBlock = [&](int x, int y, int z) -> bool {
            const BlockInfo& info = getBlockInfo(getNeighborhoodBlock(x, y, z));
            return info.isSolid && !info.isTransparent;
        };
        auto getVertexAO = [&](int face, const Vec3& cornerPos, int nx, int ny, int nz) -> float {
            int dx = (cornerPos.x > 0.5f) ? 1 : -1;
            int dy = (cornerPos.y > 0.5f) ? 1 : -1;
            int dz = (cornerPos.z > 0.5f) ? 1 : -1;

            bool s1 = false, s2 = false, corner = false;
            if (face == DIR_POS_X || face == DIR_NEG_X) {
                s1 = isSolidBlock(nx, ny + dy, nz);
                s2 = isSolidBlock(nx, ny, nz + dz);
                corner = isSolidBlock(nx, ny + dy, nz + dz);
            } else if (face == DIR_POS_Y || face == DIR_NEG_Y) {
                s1 = isSolidBlock(nx + dx, ny, nz);
                s2 = isSolidBlock(nx, ny, nz + dz);
                corner = isSolidBlock(nx + dx, ny, nz + dz);
            } else {
                s1 = isSolidBlock(nx + dx, ny, nz);
                s2 = isSolidBlock(nx, ny + dy, nz);
                corner = isSolidBlock(nx + dx, ny + dy, nz);
            }

            return calculateAO(s1, s2, corner);
        };


        // only opaque cubic terrain is safe to combine into a rectangle.
        auto isGreedyOpaqueBlock = [&](uint8_t block) -> bool {
            const BlockInfo& info = getBlockInfo(block);
            return block != BLOCK_AIR &&
                info.isSolid && !info.isTransparent &&
                !isAnyLeaf(block) &&
                block != BLOCK_TALL_GRASS &&
                block != BLOCK_TALL_GRASS_TOP;
        };

        // 6 Cube Face Templates
        // Face order: +X, -X, +Y, -Y, +Z, -Z
        const Vec3 faceNormals[6] = {
            Vec3(1, 0, 0), Vec3(-1, 0, 0),
            Vec3(0, 1, 0), Vec3(0, -1, 0),
            Vec3(0, 0, 1), Vec3(0, 0, -1)
        };

        const int faceOffsetDirs[6][3] = {
            { 1,  0,  0}, {-1,  0,  0},
            { 0,  1,  0}, { 0, -1,  0},
            { 0,  0,  1}, { 0,  0, -1}
        };

        const Vec3 faceCorners[6][4] = {
            // +X
            { Vec3(1,0,0), Vec3(1,1,0), Vec3(1,1,1), Vec3(1,0,1) },
            // -X
            { Vec3(0,0,1), Vec3(0,1,1), Vec3(0,1,0), Vec3(0,0,0) },
            // +Y
            { Vec3(0,1,1), Vec3(1,1,1), Vec3(1,1,0), Vec3(0,1,0) },
            // -Y
            { Vec3(0,0,0), Vec3(1,0,0), Vec3(1,0,1), Vec3(0,0,1) },
            // +Z
            { Vec3(1,0,1), Vec3(1,1,1), Vec3(0,1,1), Vec3(0,0,1) },
            // -Z
            { Vec3(0,0,0), Vec3(0,1,0), Vec3(1,1,0), Vec3(1,0,0) }
        };

        const float faceUVs[4][2] = {
            {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}
        };

        // Append a crossed pair of alpha-tested quads. Both windings are
        // emitted because the regular voxel pass uses back-face culling.
        auto appendGrassQuad = [&](uint8_t grassTexTile, float heightScale,
                                   float windBase, float windTip,
                                   float blockRelX, float blockRelY, float blockRelZ,
                                   const Vec3& bottomA, const Vec3& bottomB, const Vec3& normal) {
            const uint8_t texTileID = grassTexTile;
            const Vec3 up(0.0f, fScale * heightScale, 0.0f);
            const Vec3 positions[4] = {
                bottomA, bottomA + up, bottomB + up, bottomB
            };
            const Vec3 reversePositions[4] = {
                bottomA, bottomB, bottomB + up, bottomA + up
            };
            const float windWeights[4] = { windBase, windTip, windTip, windBase };
            const float uv[4][2] = {
                {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}
            };
            const float reverseUV[4][2] = {
                {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}
            };

            auto appendWinding = [&](const Vec3* quad, const Vec3& quadNormal,
                                     const float quadUV[4][2],
                                     const float quadWindWeights[4]) {
                uint32_t baseIdx = static_cast<uint32_t>(chunk.stagedVertices.size());
                for (int c = 0; c < 4; ++c) {
                    VoxelVertex vert{};
                    vert.x = blockRelX + quad[c].x * fScale;
                    vert.y = blockRelY + quad[c].y;
                    vert.z = blockRelZ + quad[c].z * fScale;
                    vert.nx = quadNormal.x;
                    vert.ny = quadNormal.y;
                    vert.nz = quadNormal.z;
                    // Keep grass samples inside the tile by half a texel so
                    // atlas boundaries can never contribute neighbor colors.
                    vert.u = quadUV[c][0];
                    vert.v = quadUV[c][1];
                    vert.texIndex = static_cast<float>(texTileID);
                    vert.ao = 1.0f;
                    int plantX = std::clamp(
                        static_cast<int>(blockRelX / fScale),
                        0,
                        CHUNK_SIZE - 1
                    );
                    int plantY = std::clamp(
                        static_cast<int>((blockRelY + 0.05f) / fScale),
                        0,
                        CHUNK_SIZE - 1
                    );
                    int plantZ = std::clamp(
                        static_cast<int>(blockRelZ / fScale),
                        0,
                        CHUNK_SIZE - 1
                    );
                    uint16_t plantL = neighborhood
                        ? neighborhood->lightAt(plantX, plantY, plantZ)
                        : chunk.getLight(plantX, plantY, plantZ);
                    vert.lightR = static_cast<float>(getLightR(plantL)) / 15.0f;
                    vert.lightG = static_cast<float>(getLightG(plantL)) / 15.0f;
                    vert.lightB = static_cast<float>(getLightB(plantL)) / 15.0f;
                    vert.skyLight = static_cast<float>(getLightSky(plantL)) / 15.0f;
                    vert.windWeight = quadWindWeights[c];
                    chunk.stagedVertices.push_back(vert);
                }

                chunk.stagedIndices.push_back(baseIdx + 0);
                chunk.stagedIndices.push_back(baseIdx + 1);
                chunk.stagedIndices.push_back(baseIdx + 2);
                chunk.stagedIndices.push_back(baseIdx + 0);
                chunk.stagedIndices.push_back(baseIdx + 2);
                chunk.stagedIndices.push_back(baseIdx + 3);
            };

            const float reverseWindWeights[4] = {
                windBase, windBase, windTip, windTip
            };
            appendWinding(positions, normal, uv, windWeights);
            appendWinding(reversePositions, -normal, reverseUV, reverseWindWeights);
        };

        {
            auto faceCell = [&](int face, int slice, int u, int v) -> IVec3 {
                switch (face) {
                    case DIR_POS_X:
                    case DIR_NEG_X: return IVec3(slice, v, u); // u=Z, v=Y
                    case DIR_POS_Y:
                    case DIR_NEG_Y: return IVec3(u, slice, v); // u=X, v=Z
                    default: return IVec3(u, v, slice);         // u=X, v=Y
                }
            };

            auto facePosition = [&](int face, int slice, int u, int v) -> Vec3 {
                switch (face) {
                    case DIR_POS_X: return Vec3(slice + 1.0f, v, u);
                    case DIR_NEG_X: return Vec3(slice, v, u);
                    case DIR_POS_Y: return Vec3(u, slice + 1.0f, v);
                    case DIR_NEG_Y: return Vec3(u, slice, v);
                    case DIR_POS_Z: return Vec3(u, v, slice + 1.0f);
                    default: return Vec3(u, v, slice);
                }
            };

            auto localFaceCorner = [&](int face, int localU, int localV) -> Vec3 {
                float normalSide = (face % 2 == 0) ? 1.0f : 0.0f;
                switch (face) {
                    case DIR_POS_X:
                    case DIR_NEG_X: return Vec3(normalSide, localV, localU);
                    case DIR_POS_Y:
                    case DIR_NEG_Y: return Vec3(localU, normalSide, localV);
                    default: return Vec3(localU, localV, normalSide);
                }
            };

            // Corner ordering matches faceCorners above, so winding, AO, and
            // texture orientation stay consistent with the legacy mesher.
            const int cornerU[6][4] = {
                {0, 0, 1, 1}, {1, 1, 0, 0},
                {0, 1, 1, 0}, {0, 1, 1, 0},
                {1, 1, 0, 0}, {0, 0, 1, 1}
            };
            const int cornerV[6][4] = {
                {0, 1, 1, 0}, {0, 1, 1, 0},
                {1, 1, 0, 0}, {0, 0, 1, 1},
                {0, 1, 1, 0}, {0, 1, 1, 0}
            };

            auto appendGreedyQuad = [&](uint8_t blockType, int face, int slice,
                                        int u0, int v0, int width, int height) {
                uint8_t texTileID = getBlockTextureIndex(blockType, face);
                uint32_t baseIdx = static_cast<uint32_t>(chunk.stagedVertices.size());
                float aoVals[4];

                for (int c = 0; c < 4; ++c) {
                    int uBoundary = u0 + (cornerU[face][c] ? width : 0);
                    int vBoundary = v0 + (cornerV[face][c] ? height : 0);
                    Vec3 position = facePosition(face, slice, uBoundary, vBoundary);

                    int cellU = cornerU[face][c] ? uBoundary - 1 : uBoundary;
                    int cellV = cornerV[face][c] ? vBoundary - 1 : vBoundary;
                    IVec3 cell = faceCell(face, slice, cellU, cellV);
                    Vec3 cornerPos = localFaceCorner(
                        face,
                        cornerU[face][c],
                        cornerV[face][c]
                    );
                    int nx = static_cast<int>(cell.x) + faceOffsetDirs[face][0];
                    int ny = static_cast<int>(cell.y) + faceOffsetDirs[face][1];
                    int nz = static_cast<int>(cell.z) + faceOffsetDirs[face][2];
                    float ao = getVertexAO(face, cornerPos, nx, ny, nz);
                    aoVals[c] = ao;

                    uint16_t light = getNeighborhoodLight(nx, ny, nz);
                    const BlockInfo& info = getBlockInfo(blockType);
                    float lightR = info.lightR > 0 ? static_cast<float>(info.lightR) / 15.0f : static_cast<float>(getLightR(light)) / 15.0f;
                    float lightG = info.lightG > 0 ? static_cast<float>(info.lightG) / 15.0f : static_cast<float>(getLightG(light)) / 15.0f;
                    float lightB = info.lightB > 0 ? static_cast<float>(info.lightB) / 15.0f : static_cast<float>(getLightB(light)) / 15.0f;
                    float skyLight = (info.lightR > 0 || info.lightG > 0 || info.lightB > 0)
                        ? 0.0f
                        : static_cast<float>(getLightSky(light)) / 15.0f;

                    VoxelVertex vert{};
                    vert.x = position.x * fScale;
                    vert.y = position.y * fScale;
                    vert.z = position.z * fScale;
                    vert.nx = faceNormals[face].x;
                    vert.ny = faceNormals[face].y;
                    vert.nz = faceNormals[face].z;
                    vert.u = static_cast<float>(cornerU[face][c] * width);
                    vert.v = static_cast<float>(cornerV[face][c] * height);
                    vert.texIndex = static_cast<float>(texTileID);
                    vert.ao = ao;
                    vert.lightR = lightR;
                    vert.lightG = lightG;
                    vert.lightB = lightB;
                    vert.skyLight = skyLight;
                    vert.windWeight = 0.0f;
                    chunk.stagedVertices.push_back(vert);
                }

                if (aoVals[0] + aoVals[2] < aoVals[1] + aoVals[3]) {
                    chunk.stagedIndices.push_back(baseIdx + 0);
                    chunk.stagedIndices.push_back(baseIdx + 1);
                    chunk.stagedIndices.push_back(baseIdx + 3);
                    chunk.stagedIndices.push_back(baseIdx + 1);
                    chunk.stagedIndices.push_back(baseIdx + 2);
                    chunk.stagedIndices.push_back(baseIdx + 3);
                } else {
                    chunk.stagedIndices.push_back(baseIdx + 0);
                    chunk.stagedIndices.push_back(baseIdx + 1);
                    chunk.stagedIndices.push_back(baseIdx + 2);
                    chunk.stagedIndices.push_back(baseIdx + 0);
                    chunk.stagedIndices.push_back(baseIdx + 2);
                    chunk.stagedIndices.push_back(baseIdx + 3);
                }
            };

            struct GreedyCell {
                uint8_t block = BLOCK_AIR;
                uint16_t light = 0;
                uint8_t aoSignature = 0;

                bool operator==(const GreedyCell& other) const {
                    return block == other.block &&
                        light == other.light &&
                        aoSignature == other.aoSignature;
                }
            };

            auto calculateAoSignature = [&](const IVec3& cell, int face) -> uint8_t {
                uint8_t signature = 0;
                int nx = static_cast<int>(cell.x) + faceOffsetDirs[face][0];
                int ny = static_cast<int>(cell.y) + faceOffsetDirs[face][1];
                int nz = static_cast<int>(cell.z) + faceOffsetDirs[face][2];
                for (int c = 0; c < 4; ++c) {
                    Vec3 cornerPos = faceCorners[face][c];
                    float ao = getVertexAO(face, cornerPos, nx, ny, nz);
                    uint8_t value = ao <= 0.3f
                        ? 3
                        : (ao < 0.7f ? 2 : (ao < 0.9f ? 1 : 0));
                    signature |= static_cast<uint8_t>(value << (c * 2));
                }
                return signature;
            };

            std::vector<GreedyCell> mask(CHUNK_SIZE * CHUNK_SIZE);
            for (int face = 0; face < 6; ++face) {
                for (int slice = 0; slice < CHUNK_SIZE; ++slice) {
                    std::fill(mask.begin(), mask.end(), GreedyCell{});

                    for (int v = 0; v < CHUNK_SIZE; ++v) {
                        for (int u = 0; u < CHUNK_SIZE; ++u) {
                            IVec3 cell = faceCell(face, slice, u, v);
                            uint8_t block = chunk.getBlock(
                                static_cast<int>(cell.x),
                                static_cast<int>(cell.y),
                                static_cast<int>(cell.z)
                            );
                            if (!isGreedyOpaqueBlock(block)) continue;

                            int nx = static_cast<int>(cell.x) + faceOffsetDirs[face][0];
                            int ny = static_cast<int>(cell.y) + faceOffsetDirs[face][1];
                            int nz = static_cast<int>(cell.z) + faceOffsetDirs[face][2];
                            if (!isSolidBlock(nx, ny, nz)) {
                                mask[v * CHUNK_SIZE + u] = {
                                    block,
                                    getNeighborhoodLight(nx, ny, nz),
                                    calculateAoSignature(cell, face)
                                };
                            }
                        }
                    }

                    for (int v = 0; v < CHUNK_SIZE; ++v) {
                        for (int u = 0; u < CHUNK_SIZE; ++u) {
                            GreedyCell cell = mask[v * CHUNK_SIZE + u];
                            if (cell.block == BLOCK_AIR) continue;

                            int width = 1;
                            while (u + width < CHUNK_SIZE &&
                                   mask[v * CHUNK_SIZE + u + width] == cell) {
                                ++width;
                            }

                            int height = 1;
                            bool canGrow = true;
                            while (v + height < CHUNK_SIZE && canGrow) {
                                for (int du = 0; du < width; ++du) {
                                    if (mask[(v + height) * CHUNK_SIZE + u + du] != cell) {
                                        canGrow = false;
                                        break;
                                    }
                                }
                                if (canGrow) ++height;
                            }

                            for (int dv = 0; dv < height; ++dv) {
                                for (int du = 0; du < width; ++du) {
                                    mask[(v + dv) * CHUNK_SIZE + u + du] = GreedyCell{};
                                }
                            }
                            appendGreedyQuad(cell.block, face, slice, u, v, width, height);
                        }
                    }
                }
            }
        }

        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    uint8_t blockType = chunk.getBlock(x, y, z);
                    if (blockType == BLOCK_AIR) continue;
                    if (chunk.lod >= 2 &&
                        (blockType == BLOCK_TALL_GRASS ||
                         blockType == BLOCK_TALL_GRASS_TOP)) {
                        continue;
                    }
                    if (isGreedyOpaqueBlock(blockType)) {
                        continue;
                    }

                    // World position relative to camera
                    // Position relative to chunk origin
                    float relX = static_cast<float>(x * scale);
                    float relY = static_cast<float>(y * scale);
                    float relZ = static_cast<float>(z * scale);

                    uint8_t leafVariant = 0;
                    if (isAnyLeaf(blockType)) {
                        uint64_t leafHash =
                            static_cast<uint64_t>(wmx + x * scale) * 0x9E3779B185EBCA87ULL ^
                            static_cast<uint64_t>(wmy + y * scale) * 0xC2B2AE3D27D4EB4FULL ^
                            static_cast<uint64_t>(wmz + z * scale) * 0xBF58476D1CE4E5B9ULL;
                        leafHash ^= leafHash >> 30;
                        leafHash *= 0x94D049BB133111EBULL;
                        leafHash ^= leafHash >> 27;
                        leafVariant = static_cast<uint8_t>(leafHash % 6ULL);
                    }

                    if (blockType == BLOCK_TALL_GRASS || blockType == BLOCK_TALL_GRASS_TOP) {
                        // Stable per-cell variation keeps neighboring chunks
                        // seamless while preventing a repeated stamp pattern.
                        uint64_t grassHash = static_cast<uint64_t>(wmx + x * scale) *
                            0x9E3779B185EBCA87ULL ^
                            static_cast<uint64_t>(wmz + z * scale) *
                            0xC2B2AE3D27D4EB4FULL;
                        grassHash ^= grassHash >> 30;
                        grassHash *= 0xBF58476D1CE4E5B9ULL;
                        grassHash ^= grassHash >> 27;
                        bool upperHalf = blockType == BLOCK_TALL_GRASS_TOP;
                        bool rareTwoTallLower = blockType == BLOCK_TALL_GRASS &&
                            chunk.getBlock(x, y + 1, z) == BLOCK_TALL_GRASS_TOP;
                        bool twoTallPlant = rareTwoTallLower ||
                            (upperHalf && y > 0 &&
                                chunk.getBlock(x, y - 1, z) == BLOCK_TALL_GRASS);
                        // There are three sprite variants, but the yellow-tip
                        // variant is deliberately uncommon: 1/15 for normal
                        // grass instead of the old 1/3. Two-tall plants are
                        // restricted to the two green variants entirely.
                        uint8_t grassVariant = static_cast<uint8_t>((grassHash >> 8) & 1ULL);
                        if (!twoTallPlant && grassHash % 15ULL == 0ULL) {
                            grassVariant = 2;
                        }
                        uint8_t grassTexTile = upperHalf
                            ? static_cast<uint8_t>(32 + grassVariant)
                            : static_cast<uint8_t>((rareTwoTallLower ? 35 : 29) + grassVariant);
                        float heightScale = 1.0f;
                        float inset = 0.08f +
                            static_cast<float>((grassHash >> 16) & 0x1f) / 31.0f * 0.06f;
                        float baseY = relY - 0.02f;
                        float windBase = upperHalf ? 0.5f : 0.0f;
                        float windTip = upperHalf ? 1.0f :
                            (rareTwoTallLower ? 0.5f : 1.0f);
                        appendGrassQuad(
                            grassTexTile, heightScale, windBase, windTip, relX, baseY, relZ,
                            Vec3(inset, 0.0f, inset),
                            Vec3(1.0f - inset, 0.0f, 1.0f - inset),
                            Vec3(1.0f, 0.0f, -1.0f).normalized()
                        );
                        appendGrassQuad(
                            grassTexTile, heightScale, windBase, windTip, relX, baseY, relZ,
                            Vec3(inset, 0.0f, 1.0f - inset),
                            Vec3(1.0f - inset, 0.0f, inset),
                            Vec3(-1.0f, 0.0f, -1.0f).normalized()
                        );
                        continue;
                    }

                    for (int f = 0; f < 6; ++f) {
                        int nx = x + faceOffsetDirs[f][0];
                        int ny = y + faceOffsetDirs[f][1];
                        int nz = z + faceOffsetDirs[f][2];

                        // Perform face culling
                        if (isSolidBlock(nx, ny, nz)) continue;

                        uint8_t texTileID = getBlockTextureIndex(blockType, f);
                        if (isAnyLeaf(blockType)) {
                            texTileID = getLeafTextureIndex(blockType, leafVariant);
                        }
                        bool flipGrassSideV = blockType == BLOCK_GRASS &&
                            f != DIR_POS_Y && f != DIR_NEG_Y;

                        const BlockInfo& bInfo = getBlockInfo(blockType);
                        float fR = 0.0f, fG = 0.0f, fB = 0.0f, fSky = 0.0f;
                        if (bInfo.lightR > 0 || bInfo.lightG > 0 || bInfo.lightB > 0) {
                            fR = static_cast<float>(bInfo.lightR) / 15.0f;
                            fG = static_cast<float>(bInfo.lightG) / 15.0f;
                            fB = static_cast<float>(bInfo.lightB) / 15.0f;
                            fSky = 0.0f;
                        } else {
                            uint16_t lVal = getNeighborhoodLight(nx, ny, nz);
                            fR = static_cast<float>(getLightR(lVal)) / 15.0f;
                            fG = static_cast<float>(getLightG(lVal)) / 15.0f;
                            fB = static_cast<float>(getLightB(lVal)) / 15.0f;
                            fSky = static_cast<float>(getLightSky(lVal)) / 15.0f;
                        }
                        // Calculate AO for each corner vertex
                        float ao[4];
                        for (int c = 0; c < 4; ++c) {
                            Vec3 cornerPos = faceCorners[f][c];
                            ao[c] = getVertexAO(f, cornerPos, nx, ny, nz);
                        }

                        uint32_t baseIdx = static_cast<uint32_t>(chunk.stagedVertices.size());

                        for (int c = 0; c < 4; ++c) {
                            Vec3 p = faceCorners[f][c];
                            VoxelVertex vert;
                            vert.x = relX + p.x * fScale;
                            vert.y = relY + p.y * fScale;
                            vert.z = relZ + p.z * fScale;
                            vert.nx = faceNormals[f].x;
                            vert.ny = faceNormals[f].y;
                            vert.nz = faceNormals[f].z;
                            vert.u = faceUVs[c][0];
                            vert.v = flipGrassSideV ? 1.0f - faceUVs[c][1] : faceUVs[c][1];
                            vert.texIndex = static_cast<float>(texTileID);
                            vert.ao = ao[c];
                            vert.lightR = fR;
                            vert.lightG = fG;
                            vert.lightB = fB;
                            vert.skyLight = fSky;
                            vert.windWeight = 0.0f;

                            chunk.stagedVertices.push_back(vert);
                        }

                        // Add quad indices (2 triangles)
                        if (ao[0] + ao[2] < ao[1] + ao[3]) {
                            chunk.stagedIndices.push_back(baseIdx + 0);
                            chunk.stagedIndices.push_back(baseIdx + 1);
                            chunk.stagedIndices.push_back(baseIdx + 3);
                            chunk.stagedIndices.push_back(baseIdx + 1);
                            chunk.stagedIndices.push_back(baseIdx + 2);
                            chunk.stagedIndices.push_back(baseIdx + 3);
                        } else {
                            chunk.stagedIndices.push_back(baseIdx + 0);
                            chunk.stagedIndices.push_back(baseIdx + 1);
                            chunk.stagedIndices.push_back(baseIdx + 2);
                            chunk.stagedIndices.push_back(baseIdx + 0);
                            chunk.stagedIndices.push_back(baseIdx + 2);
                            chunk.stagedIndices.push_back(baseIdx + 3);
                        }
                    }
                }
            }
        }

        chunk.isMeshStaged = true;
    }
};

#endif // MESH_BUILDER_HPP
