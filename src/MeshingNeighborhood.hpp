#ifndef MESHING_NEIGHBORHOOD_HPP
#define MESHING_NEIGHBORHOOD_HPP

#include "Chunk.hpp"
#include <array>
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

#endif // MESHING_NEIGHBORHOOD_HPP
