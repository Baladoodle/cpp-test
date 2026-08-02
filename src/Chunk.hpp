#ifndef CHUNK_HPP
#define CHUNK_HPP

#include <vector>
#include <cstdint>
#include <memory>
#include <atomic>
#include "MathUtils.hpp"
#include "Block.hpp"
#include "Mesh.hpp"

constexpr int CHUNK_SIZE = 32;
constexpr int CHUNK_VOL = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
constexpr int PADDED_SIZE = CHUNK_SIZE + 2;
constexpr int PADDED_VOL = PADDED_SIZE * PADDED_SIZE * PADDED_SIZE;

inline int getVoxelIndex(int x, int y, int z) {
    return (y * CHUNK_SIZE + z) * CHUNK_SIZE + x;
}

inline int getPaddedVoxelIndex(int x, int y, int z) {
    return ((y + 1) * PADDED_SIZE + (z + 1)) * PADDED_SIZE + (x + 1);
}

struct Chunk {
    IVec3 chunkPos; // Chunk grid coordinates at this LOD level
    int lod;        // 0 to 6
    int scale;      // 1 << lod
    int worldSize;  // CHUNK_SIZE * scale
    IVec3 worldMin; // World origin position of this chunk

    uint8_t blocks[CHUNK_VOL];
    uint8_t light[CHUNK_VOL];
    std::vector<uint8_t> paddedBlocks;

    bool isEmpty = true;
    std::atomic<bool> isGenerated{false};
    std::atomic<bool> isMeshStaged{false};
    std::atomic<bool> isMeshUploaded{false};
    std::atomic<bool> isPendingWork{true};
    bool isFullyCovered = false;

    // Staging mesh data built on background thread
    std::vector<VoxelVertex> stagedVertices;
    std::vector<uint32_t> stagedIndices;

    // Renderable OpenGL mesh (uploaded on main thread)
    Mesh mesh;

    Chunk(IVec3 cpos, int lodLevel) : chunkPos(cpos), lod(lodLevel) {
        scale = 1 << lod;
        worldSize = CHUNK_SIZE * scale;
        worldMin = IVec3(cpos.x * worldSize, cpos.y * worldSize, cpos.z * worldSize);
        std::fill(blocks, blocks + CHUNK_VOL, 0);
        std::fill(light, light + CHUNK_VOL, 0);
    }

    inline uint8_t getBlock(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return BLOCK_AIR;
        return blocks[getVoxelIndex(x, y, z)];
    }

    inline uint8_t getPaddedBlock(int x, int y, int z) const {
        if (paddedBlocks.empty()) return getBlock(x, y, z);
        if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) return BLOCK_AIR;
        return paddedBlocks[getPaddedVoxelIndex(x, y, z)];
    }

    inline void setBlock(int x, int y, int z, uint8_t b) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return;
        blocks[getVoxelIndex(x, y, z)] = b;
    }

    inline uint8_t getLight(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return 0;
        return light[getVoxelIndex(x, y, z)];
    }
};

#endif // CHUNK_HPP
