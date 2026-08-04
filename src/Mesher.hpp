#ifndef MESHER_HPP
#define MESHER_HPP

#include "Chunk.hpp"
#include "MeshingNeighborhood.hpp"
#include "Mesh.hpp"
#include <vector>
#include <cstdint>
#include <memory>

struct ChunkBuildInput {
    IVec3 position;
    int lod = 0;
    std::shared_ptr<Chunk> chunk;
    std::unique_ptr<MeshingNeighborhood> neighborhood;
};

struct ChunkBuildOutput {
    std::vector<VoxelVertex> vertices;
    std::vector<uint32_t> indices;
};

class Mesher {
public:
    static float calculateAO(bool side1, bool side2, bool corner);
    static ChunkBuildOutput buildMesh(Chunk& chunk, const MeshingNeighborhood* neighborhood);
};

#endif // MESHER_HPP
