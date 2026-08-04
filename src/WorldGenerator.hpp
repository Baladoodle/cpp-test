#ifndef WORLD_GENERATOR_HPP
#define WORLD_GENERATOR_HPP

#include "Chunk.hpp"
#include "WorldGen.hpp"
#include "Block.hpp"
#include "MathUtils.hpp"
#include "MeshingNeighborhood.hpp"

class WorldGenerator {
public:
    static uint8_t getBlockAt(int64_t wx, int64_t wy, int64_t wz, int scale = 1) {
        return WorldGen::getBlockAt(wx, wy, wz, scale);
    }
    static float getDensity(int64_t wx, int64_t wy, int64_t wz, int scale = 1) {
        return WorldGen::getDensity(wx, wy, wz, scale);
    }
    static int64_t getSurfaceYAt(int64_t wx, int64_t wz, int64_t minY = -1000, int64_t maxY = 700) {
        return WorldGen::getSurfaceYAt(wx, wz, minY, maxY);
    }
    static void generateVoxelData(Chunk& chunk, MeshingNeighborhood* neighborhood = nullptr);
};

#endif // WORLD_GENERATOR_HPP
