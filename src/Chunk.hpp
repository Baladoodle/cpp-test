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

inline uint8_t getLightR(uint16_t val) { return (val >> 12) & 0xF; }
inline uint8_t getLightG(uint16_t val) { return (val >> 8) & 0xF; }
inline uint8_t getLightB(uint16_t val) { return (val >> 4) & 0xF; }
inline uint8_t getLightSky(uint16_t val) { return val & 0xF; }

inline uint16_t packLight(uint8_t r, uint8_t g, uint8_t b, uint8_t sky) {
    return (static_cast<uint16_t>(r & 0xF) << 12) |
           (static_cast<uint16_t>(g & 0xF) << 8)  |
           (static_cast<uint16_t>(b & 0xF) << 4)  |
           (static_cast<uint16_t>(sky & 0xF));
}

struct Chunk {
    IVec3 chunkPos; // Chunk grid coordinates at this LOD level
    int lod;        // 0 to 6
    int scale;      // 1 << lod
    int worldSize;  // CHUNK_SIZE * scale
    IVec3 worldMin; // World origin position of this chunk

    uint8_t blocks[CHUNK_VOL];
    uint16_t light[CHUNK_VOL];
    bool isEmpty = true;
    std::atomic<bool> isGenerated{false};
    std::atomic<bool> isLightReady{false};
    std::atomic<bool> isMeshStaged{false};
    std::atomic<bool> isMeshUploaded{false};
    std::atomic<bool> isMeshQueued{false};
    std::atomic<bool> meshDirty{true};
    std::atomic<bool> isPendingWork{true};
    std::atomic<bool> resident{true};
    std::atomic<uint64_t> workToken{0};
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
        return getBlock(x, y, z);
    }

    inline void setBlock(int x, int y, int z, uint8_t b) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return;
        blocks[getVoxelIndex(x, y, z)] = b;
    }

    inline uint16_t getLight(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return 0;
        return light[getVoxelIndex(x, y, z)];
    }

    inline void setLight(int x, int y, int z, uint16_t l) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return;
        light[getVoxelIndex(x, y, z)] = l;
    }

    inline uint16_t getPaddedLight(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
            int bx = std::clamp(x, 0, CHUNK_SIZE - 1);
            int by = std::clamp(y, 0, CHUNK_SIZE - 1);
            int bz = std::clamp(z, 0, CHUNK_SIZE - 1);
            uint16_t bl = getLight(bx, by, bz);
            if (bl == 0 && y >= 0) return packLight(0, 0, 0, 15);
            return bl;
        }
        return getLight(x, y, z);
    }
};

#endif // CHUNK_HPP
