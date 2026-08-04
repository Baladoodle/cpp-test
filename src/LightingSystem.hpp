#ifndef LIGHTING_SYSTEM_HPP
#define LIGHTING_SYSTEM_HPP

#include "Chunk.hpp"
#include <deque>
#include <memory>
#include <unordered_set>
#include <functional>

struct WorldLightNode {
    std::shared_ptr<Chunk> chunk;
    int8_t x;
    int8_t y;
    int8_t z;
};

class LightingSystem {
public:
    static void propagateLocalLight3D(Chunk& chunk);
    
    static void enqueueLightFace(
        const std::shared_ptr<Chunk>& chunk,
        int direction,
        std::deque<WorldLightNode>& lightQueue
    );

    static void enqueueBoundaryLight(
        const std::shared_ptr<Chunk>& chunk,
        std::deque<WorldLightNode>& lightQueue
    );

    static void propagateWorldLighting(
        std::deque<WorldLightNode>& lightQueue,
        std::unordered_set<Chunk*>& changedChunks,
        size_t maxNodesPerFrame,
        const std::function<std::shared_ptr<Chunk>(const Chunk&, int direction)>& getNeighborFunc
    );
};

#endif // LIGHTING_SYSTEM_HPP
