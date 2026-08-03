#ifndef MESH_BUILDER_HPP
#define MESH_BUILDER_HPP

#include "Chunk.hpp"
#include "WorldGen.hpp"
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

        std::queue<LightNode> lightQueue;

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
                        lightQueue.push({static_cast<int8_t>(x), static_cast<int8_t>(y), static_cast<int8_t>(z)});
                    }
                }
            }
        }

        const int dx[6] = { 1, -1,  0,  0,  0,  0 };
        const int dy[6] = { 0,  0,  1, -1,  0,  0 };
        const int dz[6] = { 0,  0,  0,  0,  1, -1 };

        while (!lightQueue.empty()) {
            LightNode curr = lightQueue.front();
            lightQueue.pop();

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
                    lightQueue.push({static_cast<int8_t>(nx), static_cast<int8_t>(ny), static_cast<int8_t>(nz)});
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
    static void generateVoxelData(Chunk& chunk) {
        bool hasSolid = false;
        int scale = chunk.scale;
        int64_t wmx = chunk.worldMin.x;
        int64_t wmy = chunk.worldMin.y;
        int64_t wmz = chunk.worldMin.z;

        constexpr int DENSITY_HEIGHT = CHUNK_SIZE + 4;
        thread_local std::vector<float> densityGrid;
        densityGrid.resize(DENSITY_HEIGHT * CHUNK_SIZE * CHUNK_SIZE);

        auto densityIndex = [](int x, int y, int z) {
            return (y * CHUNK_SIZE + z) * CHUNK_SIZE + x;
        };

        for (int y = 0; y < DENSITY_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    int64_t wx = wmx + x * scale + scale / 2;
                    int64_t wy = wmy + y * scale + scale / 2;
                    int64_t wz = wmz + z * scale + scale / 2;
                    densityGrid[densityIndex(x, y, z)] =
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
                    float density = densityGrid[densityIndex(x, y, z)];
                    float aboveDensity = densityGrid[densityIndex(x, y + 1, z)];
                    float above2Density = densityGrid[densityIndex(x, y + 4, z)];

                    uint8_t block = WorldGen::getBlockAtWithDensities(
                        wx, wy, wz, scale, density, aboveDensity, above2Density
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
                    if (densityGrid[densityIndex(x, y, z)] > 0.0f) continue;
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
        hasSolid = false;
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
        if (!chunk.isEmpty) {
            chunk.paddedBlocks.resize(PADDED_VOL);
            for (int y = -1; y <= CHUNK_SIZE; ++y) {
                for (int z = -1; z <= CHUNK_SIZE; ++z) {
                    for (int x = -1; x <= CHUNK_SIZE; ++x) {
                        uint8_t block = BLOCK_AIR;
                        bool inside = x >= 0 && x < CHUNK_SIZE &&
                            y >= 0 && y < CHUNK_SIZE &&
                            z >= 0 && z < CHUNK_SIZE;
                        if (inside) {
                            block = chunk.getBlock(x, y, z);
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
        }
        chunk.isGenerated = true;
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

        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    uint8_t blockType = chunk.getBlock(x, y, z);
                    if (blockType == BLOCK_AIR) continue;

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
