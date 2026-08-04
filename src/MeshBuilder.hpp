#ifndef MESH_BUILDER_HPP
#define MESH_BUILDER_HPP

#include "Chunk.hpp"
#include "MeshingNeighborhood.hpp"
#include "WorldGenerator.hpp"
#include "LightingSystem.hpp"
#include "Mesher.hpp"
#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>

class MeshBuilder {
public:
    static float calculateAO(bool side1, bool side2, bool corner) {
        return Mesher::calculateAO(side1, side2, corner);
    }

    static void generateVoxelData(Chunk& chunk, MeshingNeighborhood* neighborhood = nullptr) {
        WorldGenerator::generateVoxelData(chunk, neighborhood);
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
        Mesher::buildMesh(chunk, neighborhood);
    }
};

#endif // MESH_BUILDER_HPP
