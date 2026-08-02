#ifndef MESH_BUILDER_HPP
#define MESH_BUILDER_HPP

#include "Chunk.hpp"
#include "WorldGen.hpp"
#include "Block.hpp"
#include "MathUtils.hpp"
#include <vector>
#include <cstdint>
#include <algorithm>

class MeshBuilder {
private:
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
                        uint8_t emission = getBlockInfo(block).lightEmission;
                        if (emission > 0) {
                            chunk.light[getVoxelIndex(x, y, z)] = emission;
                        }
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

                    for (int f = 0; f < 6; ++f) {
                        int nx = x + faceOffsetDirs[f][0];
                        int ny = y + faceOffsetDirs[f][1];
                        int nz = z + faceOffsetDirs[f][2];

                        // Perform face culling
                        if (isSolidBlock(nx, ny, nz)) continue;

                        uint8_t texTileID = getBlockTextureIndex(blockType, f);
                        float tileU0 = (texTileID % 16) / 16.0f;
                        float tileV0 = (texTileID / 16) / 16.0f;
                        float tileSize = 1.0f / 16.0f;
                        bool flipGrassSideV = blockType == BLOCK_GRASS &&
                            f != DIR_POS_Y && f != DIR_NEG_Y;

                        uint8_t emission = getBlockInfo(blockType).lightEmission;
                        float blockEmissive = (emission > 0) ? (static_cast<float>(emission) / 15.0f) : getLightVal(nx, ny, nz);

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
                            vert.blockLight = blockEmissive;
                            vert.lodLevel = lodLvl;

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
