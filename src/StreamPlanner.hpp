#ifndef STREAM_PLANNER_HPP
#define STREAM_PLANNER_HPP

#include "ChunkStore.hpp"
#include "MathUtils.hpp"
#include <vector>
#include <array>

constexpr float SCREEN_SPACE_DIAMETER_THRESHOLD = 40.0f;
constexpr int LOD_RADII[NUM_LODS] = { 6, 8, 10, 12, 14, 16, 18 };

struct StreamTarget {
    IVec3 position;
    int lod;
    float priority;
};

class StreamPlanner {
public:
    static bool isChunkOutOfRange(const Chunk* chunk, const Vec3& camPos);
    
    static void getChunkBounds(const Chunk* chunk, Vec3 cameraPos, Vec3& minP, Vec3& maxP);
    
    static void selectHierarchicalNode(
        Chunk* chunk,
        const Frustum& frustum,
        Vec3 cameraPos,
        float projectionScale,
        const ChunkStore& store,
        std::vector<Chunk*>& selectedChunks
    );

    static std::vector<StreamTarget> computeStreamingPlan(const Vec3& camPos, const ChunkStore& store);
};

#endif // STREAM_PLANNER_HPP
