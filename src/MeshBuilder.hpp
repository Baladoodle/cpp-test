#ifndef MESH_BUILDER_HPP
#define MESH_BUILDER_HPP

#include "Chunk.hpp"
#include "VoxelMip.hpp"
#include "Block.hpp"
#include "MathUtils.hpp"
#include <vector>
#include <queue>
#include <tuple>
#include <cstdint>
#include <algorithm>

class MeshBuilder {
private:
    struct LightNode {
        int8_t x, y, z;
    };

    static void propagateLight3D(Chunk& chunk) {
        if (chunk.isEmpty) return;

        chunk.paddedLight.assign(PADDED_VOL, 0);

        thread_local std::vector<LightNode> lightQueue;
        lightQueue.clear();
        lightQueue.reserve(4096);
        for (int z = -1; z <= CHUNK_SIZE; ++z) {
            for (int x = -1; x <= CHUNK_SIZE; ++x) {
                uint8_t topBlock = chunk.getPaddedBlock(x, CHUNK_SIZE, z);
                bool openSky = getBlockInfo(topBlock).isTransparent;
                uint8_t skyVal = openSky ? 15 : 0;

                for (int y = CHUNK_SIZE; y >= -1; --y) {
                    uint8_t block = chunk.getPaddedBlock(x, y, z);
                    const BlockInfo& info = getBlockInfo(block);

                    if (!info.isTransparent) {
                        openSky = false;
                        skyVal = 0;
                    } else if (openSky && isAnyLeaf(block)) {
                        skyVal = (skyVal > 3) ? skyVal - 2 : 0;
                    }

                    uint8_t r = info.lightR;
                    uint8_t g = info.lightG;
                    uint8_t b = info.lightB;
                    uint8_t sky = skyVal;

                    bool isBorder = (x == -1 || x == CHUNK_SIZE ||
                                     y == -1 || y == CHUNK_SIZE ||
                                     z == -1 || z == CHUNK_SIZE);
                    if (isBorder && info.isTransparent && openSky) {
                        sky = 15;
                    }

                    if (r > 0 || g > 0 || b > 0 || sky > 0) {
                        chunk.setPaddedLight(x, y, z, packLight(r, g, b, sky));
                        lightQueue.push_back({static_cast<int8_t>(x), static_cast<int8_t>(y), static_cast<int8_t>(z)});
                    }
                }
            }
        }

        const int dx[6] = { 1, -1,  0,  0,  0,  0 };
        const int dy[6] = { 0,  0,  1, -1,  0,  0 };
        const int dz[6] = { 0,  0,  0,  0,  1, -1 };

        size_t head = 0;
        while (head < lightQueue.size()) {
            LightNode curr = lightQueue[head++];
            uint16_t currLight = chunk.getPaddedLight(curr.x, curr.y, curr.z);
            uint8_t cr = getLightR(currLight);
            uint8_t cg = getLightG(currLight);
            uint8_t cb = getLightB(currLight);
            uint8_t csky = getLightSky(currLight);

            for (int i = 0; i < 6; ++i) {
                int nx = curr.x + dx[i];
                int ny = curr.y + dy[i];
                int nz = curr.z + dz[i];

                if (nx < -1 || nx > CHUNK_SIZE || ny < -1 || ny > CHUNK_SIZE || nz < -1 || nz > CHUNK_SIZE)
                    continue;

                uint8_t nBlock = chunk.getPaddedBlock(nx, ny, nz);
                if (!getBlockInfo(nBlock).isTransparent) continue;

                uint16_t nLight = chunk.getPaddedLight(nx, ny, nz);
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
                    chunk.setPaddedLight(nx, ny, nz, packLight(nr, ng, nb, nsky));
                    lightQueue.push_back({static_cast<int8_t>(nx), static_cast<int8_t>(ny), static_cast<int8_t>(nz)});
                }
            }
        }

        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    chunk.setLight(x, y, z, chunk.getPaddedLight(x, y, z));
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
    // Rebuild the derived border/light state after a section's voxel values
    // have come from the mip pyramid instead of procedural sampling.
    static void finalizeVoxelData(Chunk& chunk, const std::vector<float>* densityGrid = nullptr, const VoxelMipStore* mipStore = nullptr) {
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
            chunk.paddedBlocks.clear();
            chunk.paddedLight.clear();
            std::fill(chunk.light, chunk.light + CHUNK_VOL, 0);
            chunk.isGenerated = true;
            return;
        }

        int scale = chunk.scale;
        int64_t wmx = chunk.worldMin.x;
        int64_t wmy = chunk.worldMin.y;
        int64_t wmz = chunk.worldMin.z;
        chunk.paddedBlocks.resize(PADDED_VOL);

        constexpr int GRID_DX = CHUNK_SIZE + 7; // 39
        constexpr int GRID_DZ = CHUNK_SIZE + 7; // 39
        auto gridIndex = [](int x, int y, int z) {
            return ((y + 1) * GRID_DZ + (z + 3)) * GRID_DX + (x + 3);
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
                        float density = (*densityGrid)[gridIndex(x, y, z)];
                        if (density <= 0.0f) {
                            block = BLOCK_AIR;
                        } else {
                            float aboveDensity = (*densityGrid)[gridIndex(x, y + 1, z)];
                            float above2Density = (*densityGrid)[gridIndex(x, y + 4, z)];

                            float nearXPos = (*densityGrid)[gridIndex(x + 1, y, z)];
                            float nearXNeg = (*densityGrid)[gridIndex(x - 1, y, z)];
                            float nearZPos = (*densityGrid)[gridIndex(x, y, z + 1)];
                            float nearZNeg = (*densityGrid)[gridIndex(x, y, z - 1)];
                            float farXPos = (*densityGrid)[gridIndex(x + 3, y, z)];
                            float farXNeg = (*densityGrid)[gridIndex(x - 3, y, z)];
                            float farZPos = (*densityGrid)[gridIndex(x, y, z + 3)];
                            float farZNeg = (*densityGrid)[gridIndex(x, y, z - 3)];

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
                    } else if (mipStore) {
                        uint8_t mipBlock = mipStore->readVoxel(chunk.lod, chunk.chunkPos, x, y, z);
                        if (mipBlock != BLOCK_AIR) {
                            block = mipBlock;
                        } else {
                            int64_t wx = wmx + x * scale + scale / 2;
                            int64_t wy = wmy + y * scale + scale / 2;
                            int64_t wz = wmz + z * scale + scale / 2;
                            block = WorldGen::getBlockAt(wx, wy, wz, scale);
                        }
                    } else {
                        int64_t wx = wmx + x * scale + scale / 2;
                        int64_t wy = wmy + y * scale + scale / 2;
                        int64_t wz = wmz + z * scale + scale / 2;
                        block = WorldGen::getBlockAt(wx, wy, wz, scale);
                    }
                    chunk.paddedBlocks[getPaddedVoxelIndex(x, y, z)] = block;
                }
            }
        }
        propagateLight3D(chunk);
        chunk.isGenerated = true;
    }

    static void generateVoxelData(Chunk& chunk) {
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

        int stride = (scale >= 2) ? 4 : 2;

        // 1. Evaluate density at sampled grid points
        for (int iy = 0; iy < GRID_SIZE; iy += (iy + stride < GRID_SIZE ? stride : (GRID_SIZE - 1 - iy))) {
            if (iy >= GRID_SIZE) break;
            int y = iy - 1;
            int64_t wy = wmy + y * scale + scale / 2;
            for (int iz = 0; iz < GRID_SIZE; iz += (iz + stride < GRID_SIZE ? stride : (GRID_SIZE - 1 - iz))) {
                if (iz >= GRID_SIZE) break;
                int z = iz - 3;
                int64_t wz = wmz + z * scale + scale / 2;
                for (int ix = 0; ix < GRID_SIZE; ix += (ix + stride < GRID_SIZE ? stride : (GRID_SIZE - 1 - ix))) {
                    if (ix >= GRID_SIZE) break;
                    int x = ix - 3;
                    int64_t wx = wmx + x * scale + scale / 2;
                    densityGrid[gridIndex(x, y, z)] =
                        WorldGen::getDensity(wx, wy, wz, scale);
                    if (ix == GRID_SIZE - 1) break;
                }
                if (iz == GRID_SIZE - 1) break;
            }
            if (iy == GRID_SIZE - 1) break;
        }

        // 2. Interpolate X coordinates
        if (stride == 4) {
            for (int iy = 0; iy < GRID_SIZE; iy += 4) {
                int cy = iy < 36 ? iy : 38;
                for (int iz = 0; iz < GRID_SIZE; iz += 4) {
                    int cz = iz < 36 ? iz : 38;
                    for (int ix = 2; ix < 36; ix += 4) {
                        int idx = ((cy * GRID_SIZE) + cz) * GRID_SIZE + ix;
                        densityGrid[idx] = 0.5f * (densityGrid[idx - 2] + densityGrid[idx + 2]);
                    }
                    int idx37 = ((cy * GRID_SIZE) + cz) * GRID_SIZE + 37;
                    densityGrid[idx37] = 0.5f * (densityGrid[((cy * GRID_SIZE) + cz) * GRID_SIZE + 36] + densityGrid[((cy * GRID_SIZE) + cz) * GRID_SIZE + 38]);
                }
            }
        }
        for (int iy = 0; iy < GRID_SIZE; iy += stride) {
            int cy = (stride == 4 && iy >= 36) ? 38 : iy;
            for (int iz = 0; iz < GRID_SIZE; iz += stride) {
                int cz = (stride == 4 && iz >= 36) ? 38 : iz;
                for (int ix = 1; ix < GRID_SIZE - 1; ix += 2) {
                    int idx = ((cy * GRID_SIZE) + cz) * GRID_SIZE + ix;
                    densityGrid[idx] = 0.5f * (densityGrid[idx - 1] + densityGrid[idx + 1]);
                }
            }
        }

        // 3. Interpolate Y coordinates
        if (stride == 4) {
            for (int iz = 0; iz < GRID_SIZE; iz += 4) {
                int cz = iz < 36 ? iz : 38;
                for (int ix = 0; ix < GRID_SIZE; ++ix) {
                    for (int iy = 2; iy < 36; iy += 4) {
                        int idx = ((iy * GRID_SIZE) + cz) * GRID_SIZE + ix;
                        int prev = (((iy - 2) * GRID_SIZE) + cz) * GRID_SIZE + ix;
                        int next = (((iy + 2) * GRID_SIZE) + cz) * GRID_SIZE + ix;
                        densityGrid[idx] = 0.5f * (densityGrid[prev] + densityGrid[next]);
                    }
                    int idx37 = ((37 * GRID_SIZE) + cz) * GRID_SIZE + ix;
                    int prev = ((36 * GRID_SIZE) + cz) * GRID_SIZE + ix;
                    int next = ((38 * GRID_SIZE) + cz) * GRID_SIZE + ix;
                    densityGrid[idx37] = 0.5f * (densityGrid[prev] + densityGrid[next]);
                }
            }
        }
        for (int iz = 0; iz < GRID_SIZE; iz += (stride == 4 ? 4 : 2)) {
            int cz = (stride == 4 && iz >= 36) ? 38 : iz;
            for (int ix = 0; ix < GRID_SIZE; ++ix) {
                for (int iy = 1; iy < GRID_SIZE - 1; iy += 2) {
                    int idx = ((iy * GRID_SIZE) + cz) * GRID_SIZE + ix;
                    int prev = (((iy - 1) * GRID_SIZE) + cz) * GRID_SIZE + ix;
                    int next = (((iy + 1) * GRID_SIZE) + cz) * GRID_SIZE + ix;
                    densityGrid[idx] = 0.5f * (densityGrid[prev] + densityGrid[next]);
                }
            }
        }

        // 4. Interpolate Z coordinates
        if (stride == 4) {
            for (int iy = 0; iy < GRID_SIZE; ++iy) {
                for (int ix = 0; ix < GRID_SIZE; ++ix) {
                    for (int iz = 2; iz < 36; iz += 4) {
                        int idx = ((iy * GRID_SIZE) + iz) * GRID_SIZE + ix;
                        int prev = ((iy * GRID_SIZE) + (iz - 2)) * GRID_SIZE + ix;
                        int next = ((iy * GRID_SIZE) + (iz + 2)) * GRID_SIZE + ix;
                        densityGrid[idx] = 0.5f * (densityGrid[prev] + densityGrid[next]);
                    }
                    int idx37 = ((iy * GRID_SIZE) + 37) * GRID_SIZE + ix;
                    int prev = ((iy * GRID_SIZE) + 36) * GRID_SIZE + ix;
                    int next = ((iy * GRID_SIZE) + 38) * GRID_SIZE + ix;
                    densityGrid[idx37] = 0.5f * (densityGrid[prev] + densityGrid[next]);
                }
            }
        }
        for (int iy = 0; iy < GRID_SIZE; ++iy) {
            for (int ix = 0; ix < GRID_SIZE; ++ix) {
                for (int iz = 1; iz < GRID_SIZE - 1; iz += 2) {
                    int idx = ((iy * GRID_SIZE) + iz) * GRID_SIZE + ix;
                    int prev = ((iy * GRID_SIZE) + (iz - 1)) * GRID_SIZE + ix;
                    int next = ((iy * GRID_SIZE) + (iz + 1)) * GRID_SIZE + ix;
                    densityGrid[idx] = 0.5f * (densityGrid[prev] + densityGrid[next]);
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

        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    if (densityGrid[gridIndex(x, y, z)] > 0.0f) continue;
                    int64_t wx = wmx + x * scale + scale / 2;
                    int64_t wy = wmy + y * scale + scale / 2;
                    int64_t wz = wmz + z * scale + scale / 2;
                    uint8_t block = WorldGen::getTreeBlockFromSites(
                        treeSites, wx, wy, wz, scale
                    );
                    if (block != BLOCK_AIR) {
                        chunk.setBlock(x, y, z, block);
                        hasSolid = true;
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
        finalizeVoxelData(chunk, &densityGrid);
    }

    static void buildMesh(Chunk& chunk) {
        chunk.stagedIndices.clear();
        chunk.stagedVertices.reserve(8192);
        chunk.stagedIndices.reserve(12288);
        if (chunk.isEmpty) {
            chunk.isMeshStaged = true;
            return;
        }

        int scale = chunk.scale;
        float fScale = static_cast<float>(scale);
        float lodLvl = static_cast<float>(chunk.lod);
        int64_t wmx = chunk.worldMin.x;
        int64_t wmy = chunk.worldMin.y;
        int64_t wmz = chunk.worldMin.z;

        // check the cached one-block border instead of regenerating neighbors.
        auto isSolidBlock = [&](int x, int y, int z) -> bool {
            uint8_t b = chunk.getPaddedBlock(x, y, z);
            const BlockInfo& info = getBlockInfo(b);
            return info.isSolid && !info.isTransparent;
        };

        // Far sections benefit most from merging. Keep leaves, plants, and
        // fluids on the existing model-aware path; only opaque cubic terrain
        // is safe to combine into a rectangular face.
        auto isGreedyOpaqueBlock = [&](uint8_t block) -> bool {
            const BlockInfo& info = getBlockInfo(block);
            return block != BLOCK_AIR &&
                info.isSolid && !info.isTransparent &&
                !isAnyLeaf(block) &&
                block != BLOCK_TALL_GRASS &&
                block != BLOCK_TALL_GRASS_TOP;
        };

        // Lambda to get block light helper
        auto getLightVal = [&](int x, int y, int z) -> float {
            if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
                return static_cast<float>(chunk.getLight(x, y, z)) / 15.0f;
            }
            return 0.0f;
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
            const float tileU0 = (texTileID % 16) / 16.0f;
            const float tileV0 = (texTileID / 16) / 16.0f;
            const float tileSize = 1.0f / 16.0f;
            const float halfTexel = 0.5f / 256.0f;
            const float safeTileSize = tileSize - halfTexel * 2.0f;
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
                    vert.u = tileU0 + halfTexel + quadUV[c][0] * safeTileSize;
                    vert.v = tileV0 + halfTexel + quadUV[c][1] * safeTileSize;
                    vert.texIndex = static_cast<float>(texTileID);
                    vert.ao = 1.0f;
                    uint16_t plantL = chunk.getLight(
                        std::clamp(static_cast<int>(blockRelX / fScale), 0, CHUNK_SIZE - 1),
                        std::clamp(static_cast<int>((blockRelY + 0.05f) / fScale), 0, CHUNK_SIZE - 1),
                        std::clamp(static_cast<int>(blockRelZ / fScale), 0, CHUNK_SIZE - 1)
                    );
                    vert.lightR = static_cast<float>(getLightR(plantL)) / 15.0f;
                    vert.lightG = static_cast<float>(getLightG(plantL)) / 15.0f;
                    vert.lightB = static_cast<float>(getLightB(plantL)) / 15.0f;
                    vert.skyLight = static_cast<float>(getLightSky(plantL)) / 15.0f;
                    vert.lodLevel = lodLvl;
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

        if (chunk.lod >= 2) {
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
                float tileU0 = (texTileID % 16) / 16.0f;
                float tileV0 = (texTileID / 16) / 16.0f;
                float tileSize = 1.0f / 16.0f;
                uint32_t baseIdx = static_cast<uint32_t>(chunk.stagedVertices.size());

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
                    int c1x = static_cast<int>(cell.x) + static_cast<int>(cornerPos.x) + faceOffsetDirs[face][0];
                    int c1y = static_cast<int>(cell.y) + static_cast<int>(cornerPos.y) + faceOffsetDirs[face][1];
                    int c1z = static_cast<int>(cell.z) + static_cast<int>(cornerPos.z) + faceOffsetDirs[face][2];
                    bool s1 = isSolidBlock(c1x, ny, nz);
                    bool s2 = isSolidBlock(nx, c1y, nz);
                    bool corner = isSolidBlock(c1x, c1y, nz);

                    uint16_t light = chunk.getPaddedLight(nx, ny, nz);
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
                    vert.u = tileU0 + cornerU[face][c] * tileSize;
                    vert.v = tileV0 + cornerV[face][c] * tileSize;
                    vert.texIndex = static_cast<float>(texTileID);
                    vert.ao = calculateAO(s1, s2, corner);
                    vert.lightR = lightR;
                    vert.lightG = lightG;
                    vert.lightB = lightB;
                    vert.skyLight = skyLight;
                    vert.lodLevel = lodLvl;
                    vert.windWeight = 0.0f;
                    chunk.stagedVertices.push_back(vert);
                }

                chunk.stagedIndices.push_back(baseIdx + 0);
                chunk.stagedIndices.push_back(baseIdx + 1);
                chunk.stagedIndices.push_back(baseIdx + 2);
                chunk.stagedIndices.push_back(baseIdx + 0);
                chunk.stagedIndices.push_back(baseIdx + 2);
                chunk.stagedIndices.push_back(baseIdx + 3);
            };

            std::vector<uint8_t> mask(CHUNK_SIZE * CHUNK_SIZE, BLOCK_AIR);
            for (int face = 0; face < 6; ++face) {
                for (int slice = 0; slice < CHUNK_SIZE; ++slice) {
                    std::fill(mask.begin(), mask.end(), BLOCK_AIR);

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
                                mask[v * CHUNK_SIZE + u] = block;
                            }
                        }
                    }

                    for (int v = 0; v < CHUNK_SIZE; ++v) {
                        for (int u = 0; u < CHUNK_SIZE; ++u) {
                            uint8_t block = mask[v * CHUNK_SIZE + u];
                            if (block == BLOCK_AIR) continue;

                            int width = 1;
                            while (u + width < CHUNK_SIZE &&
                                   mask[v * CHUNK_SIZE + u + width] == block) {
                                ++width;
                            }

                            int height = 1;
                            bool canGrow = true;
                            while (v + height < CHUNK_SIZE && canGrow) {
                                for (int du = 0; du < width; ++du) {
                                    if (mask[(v + height) * CHUNK_SIZE + u + du] != block) {
                                        canGrow = false;
                                        break;
                                    }
                                }
                                if (canGrow) ++height;
                            }

                            for (int dv = 0; dv < height; ++dv) {
                                for (int du = 0; du < width; ++du) {
                                    mask[(v + dv) * CHUNK_SIZE + u + du] = BLOCK_AIR;
                                }
                            }
                            appendGreedyQuad(block, face, slice, u, v, width, height);
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

                    if (chunk.lod >= 2 && isGreedyOpaqueBlock(blockType)) {
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
                        float tileU0 = (texTileID % 16) / 16.0f;
                        float tileV0 = (texTileID / 16) / 16.0f;
                        float tileSize = 1.0f / 16.0f;
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
                            uint16_t lVal = chunk.getPaddedLight(nx, ny, nz);
                            fR = static_cast<float>(getLightR(lVal)) / 15.0f;
                            fG = static_cast<float>(getLightG(lVal)) / 15.0f;
                            fB = static_cast<float>(getLightB(lVal)) / 15.0f;
                            fSky = static_cast<float>(getLightSky(lVal)) / 15.0f;
                        }
                        // Calculate AO for each corner vertex
                        float ao[4];
                        for (int c = 0; c < 4; ++c) {
                            Vec3 cornerPos = faceCorners[f][c];
                            int c1x = x + (int)cornerPos.x + faceOffsetDirs[f][0];
                            int c1y = y + (int)cornerPos.y + faceOffsetDirs[f][1];
                            int c1z = z + (int)cornerPos.z + faceOffsetDirs[f][2];

                            bool s1 = isSolidBlock(c1x, ny, nz);
                            bool s2 = isSolidBlock(nx, c1y, nz);
                            bool corner = isSolidBlock(c1x, c1y, nz);

                            ao[c] = calculateAO(s1, s2, corner);
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
                            vert.u = tileU0 + faceUVs[c][0] * tileSize;
                            float faceV = flipGrassSideV ? 1.0f - faceUVs[c][1] : faceUVs[c][1];
                            vert.v = tileV0 + faceV * tileSize;
                            vert.texIndex = static_cast<float>(texTileID);
                            vert.ao = ao[c];
                            vert.lightR = fR;
                            vert.lightG = fG;
                            vert.lightB = fB;
                            vert.skyLight = fSky;
                            vert.lodLevel = lodLvl;
                            vert.windWeight = 0.0f;

                            chunk.stagedVertices.push_back(vert);
                        }

                        // Add quad indices (2 triangles)
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

        chunk.isMeshStaged = true;
    }
};

#endif // MESH_BUILDER_HPP
