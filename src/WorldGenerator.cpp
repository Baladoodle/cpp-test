#include "WorldGenerator.hpp"
#include "LightingSystem.hpp"

static thread_local std::vector<uint8_t> paddedBlocks;
static thread_local std::vector<uint16_t> paddedLight;

static inline uint8_t getPaddedBlock(const Chunk& chunk, int x, int y, int z) {
    if (paddedBlocks.empty()) return chunk.getBlock(x, y, z);
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) return BLOCK_AIR;
    return paddedBlocks[getPaddedVoxelIndex(x, y, z)];
}

static inline uint16_t getPaddedLight(const Chunk& chunk, int x, int y, int z) {
    if (paddedLight.empty()) return chunk.getPaddedLight(x, y, z);
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) {
        return chunk.getPaddedLight(x, y, z);
    }
    return paddedLight[getPaddedVoxelIndex(x, y, z)];
}

static inline void setPaddedLight(int x, int y, int z, uint16_t l) {
    if (paddedLight.empty()) return;
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) return;
    paddedLight[getPaddedVoxelIndex(x, y, z)] = l;
}

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
        paddedBlocks.clear();
        paddedLight.clear();
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
    paddedBlocks.resize(PADDED_VOL);

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
                paddedBlocks[getPaddedVoxelIndex(x, y, z)] = block;
            }
        }
    }
    LightingSystem::propagateLocalLight3D(chunk);
    if (neighborhood) {
        std::copy(
            paddedBlocks.begin(),
            paddedBlocks.end(),
            neighborhood->blocks.begin()
        );
        std::copy(
            chunk.light,
            chunk.light + CHUNK_VOL,
            neighborhood->light.begin()
        );
    }
    chunk.isGenerated = true;
}

void WorldGenerator::generateVoxelData(Chunk& chunk, MeshingNeighborhood* neighborhood) {
    bool hasSolid = false;
    (void)hasSolid;
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

                float grassChance = 0.08f +
                    WorldGen::getTallGrassHabitatNoise(wx, wz) * 0.72f;
                if (WorldGen::getTallGrassCellNoise(wx, wz) > grassChance) {
                    continue;
                }
                chunk.setBlock(x, surfaceY + 1, z, BLOCK_TALL_GRASS);

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
    finalizeVoxelData(chunk, &densityGrid, neighborhood);
}
