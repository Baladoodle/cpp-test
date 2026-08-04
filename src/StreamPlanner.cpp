#include "StreamPlanner.hpp"
#include <cmath>
#include <algorithm>

bool StreamPlanner::isChunkOutOfRange(const Chunk* chunk, const Vec3& camPos) {
    if (!chunk) return true;

    int lod = chunk->lod;
    int worldChunkSize = CHUNK_SIZE * (1 << lod);
    int64_t camCX = static_cast<int64_t>(std::floor(camPos.x / worldChunkSize));
    int64_t camCY = static_cast<int64_t>(std::floor(camPos.y / worldChunkSize));
    int64_t camCZ = static_cast<int64_t>(std::floor(camPos.z / worldChunkSize));

    int radius = LOD_RADII[lod] + 2;

    int64_t dx = std::abs(chunk->chunkPos.x - camCX);
    int64_t dy = std::abs(chunk->chunkPos.y - camCY);
    int64_t dz = std::abs(chunk->chunkPos.z - camCZ);

    return dx > radius || dy > radius || dz > radius;
}

void StreamPlanner::getChunkBounds(const Chunk* chunk, Vec3 cameraPos, Vec3& minP, Vec3& maxP) {
    minP = Vec3(
        static_cast<float>(chunk->worldMin.x) - cameraPos.x,
        static_cast<float>(chunk->worldMin.y) - cameraPos.y,
        static_cast<float>(chunk->worldMin.z) - cameraPos.z
    );
    float size = static_cast<float>(chunk->worldSize);
    maxP = minP + Vec3(size);
}

void StreamPlanner::selectHierarchicalNode(
    Chunk* chunk,
    const Frustum& frustum,
    Vec3 cameraPos,
    float projectionScale,
    const ChunkStore& store,
    std::vector<Chunk*>& selectedChunks
) {
    if (!chunk) return;

    Vec3 minP, maxP;
    getChunkBounds(chunk, cameraPos, minP, maxP);

    if (!frustum.intersectsAABB(minP, maxP)) {
        return;
    }

    float dx = std::max(0.0f, std::max(minP.x, -maxP.x));
    float dy = std::max(0.0f, std::max(minP.y, -maxP.y));
    float dz = std::max(0.0f, std::max(minP.z, -maxP.z));
    float dist = std::max(0.1f, std::sqrt(dx * dx + dy * dy + dz * dz));

    float geometricError = static_cast<float>(1 << chunk->lod);
    float pixelError = geometricError * projectionScale / dist;

    float threshold = SCREEN_SPACE_DIAMETER_THRESHOLD;
    bool wantsChildren = (chunk->lod > 0) && (pixelError > threshold);

    bool allChildrenReady = false;
    if (wantsChildren) {
        allChildrenReady = true;
        int childLod = chunk->lod - 1;
        int childScale = 1 << childLod;
        int childWorldChunkSize = CHUNK_SIZE * childScale;
        int64_t baseCX = floorDiv(chunk->worldMin.x, childWorldChunkSize);
        int64_t baseCY = floorDiv(chunk->worldMin.y, childWorldChunkSize);
        int64_t baseCZ = floorDiv(chunk->worldMin.z, childWorldChunkSize);

        for (int dz = 0; dz < 2 && allChildrenReady; ++dz) {
            for (int dy = 0; dy < 2 && allChildrenReady; ++dy) {
                for (int dx = 0; dx < 2 && allChildrenReady; ++dx) {
                    IVec3 childPos(baseCX + dx, baseCY + dy, baseCZ + dz);
                    std::shared_ptr<Chunk> child = store.getChunk(childLod, childPos);
                    if (!child || !child->isAtLeast(ChunkState::Uploaded)) {
                        allChildrenReady = false;
                    }
                }
            }
        }
    }

    if (wantsChildren && allChildrenReady) {
        int childLod = chunk->lod - 1;
        int childScale = 1 << childLod;
        int childWorldChunkSize = CHUNK_SIZE * childScale;
        int64_t baseCX = floorDiv(chunk->worldMin.x, childWorldChunkSize);
        int64_t baseCY = floorDiv(chunk->worldMin.y, childWorldChunkSize);
        int64_t baseCZ = floorDiv(chunk->worldMin.z, childWorldChunkSize);

        for (int dz = 0; dz < 2; ++dz) {
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    IVec3 childPos(baseCX + dx, baseCY + dy, baseCZ + dz);
                    std::shared_ptr<Chunk> child = store.getChunk(childLod, childPos);
                    if (child) {
                        selectHierarchicalNode(child.get(), frustum, cameraPos, projectionScale, store, selectedChunks);
                    }
                }
            }
        }
    } else {
        if (chunk->isAtLeast(ChunkState::Uploaded) && !chunk->isEmpty && chunk->mesh.geometry.valid) {
            selectedChunks.push_back(chunk);
        }
    }
}

std::vector<StreamTarget> StreamPlanner::computeStreamingPlan(const Vec3& camPos, const ChunkStore& /*store*/) {
    std::vector<StreamTarget> plan;
    for (int lod = 0; lod < NUM_LODS; ++lod) {
        int worldChunkSize = CHUNK_SIZE * (1 << lod);
        int64_t camCX = static_cast<int64_t>(std::floor(camPos.x / worldChunkSize));
        int64_t camCY = static_cast<int64_t>(std::floor(camPos.y / worldChunkSize));
        int64_t camCZ = static_cast<int64_t>(std::floor(camPos.z / worldChunkSize));
        int radius = LOD_RADII[lod];

        for (int dz = -radius; dz <= radius; ++dz) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    IVec3 pos(camCX + dx, camCY + dy, camCZ + dz);
                    float distSq = static_cast<float>(dx * dx + dy * dy + dz * dz);
                    float prio = static_cast<float>(lod * 1000) + distSq;
                    plan.push_back({ pos, lod, prio });
                }
            }
        }
    }
    return plan;
}
