#ifndef CHUNK_MANAGER_HPP
#define CHUNK_MANAGER_HPP

#include "Chunk.hpp"
#include "MeshBuilder.hpp"
#include "MathUtils.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <algorithm>
#include <iostream>
#include <cmath>

class ChunkManager {
public:
    static constexpr int NUM_LODS = 5; // LOD 0 through LOD 4 active
    const int LOD_RADII[NUM_LODS] = { 4, 4, 4, 4, 4 }; // Preserve the existing radius for each tier

private:
    std::unordered_map<IVec3, std::unique_ptr<Chunk>, IVec3Hash> chunks[NUM_LODS];
    std::vector<Chunk*> renderableChunks;

    // worker queue structures
    struct GenerationTask {
        Chunk* chunk;
        float distanceSquared;
        uint64_t sequence;

        bool operator<(const GenerationTask& other) const {
            if (distanceSquared == other.distanceSquared) {
                return sequence > other.sequence;
            }
            return distanceSquared > other.distanceSquared;
        }
    };

    std::vector<std::thread> workers;
    std::priority_queue<GenerationTask> generateQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<bool> stopThreads{false};
    std::atomic<uint64_t> queueSequence{0};
public:
    std::atomic<uint64_t> chunksProcessed{0};
    std::atomic<uint64_t> totalGenTimeUs{0};
    std::atomic<uint64_t> totalMeshTimeUs{0};
private:
    Vec3 currentCamPos{0, 0, 0};
    std::mutex cameraMutex;
    inline static int64_t floorDiv(int64_t a, int64_t b) {
        int64_t res = a / b;
        int64_t rem = a % b;
        if (rem != 0 && ((a < 0) ^ (b < 0))) {
            res--;
        }
        return res;
    }

    bool shouldSkipCoarseChunk(int lod, int64_t cx, int64_t cy, int64_t cz, const Vec3& camPos) const {
        if (lod <= 0) return false;

        int scale = 1 << lod;
        int worldChunkSize = CHUNK_SIZE * scale;

        int64_t minWX = cx * worldChunkSize;
        int64_t maxWX = minWX + worldChunkSize;
        int64_t minWY = cy * worldChunkSize;
        int64_t maxWY = minWY + worldChunkSize;
        int64_t minWZ = cz * worldChunkSize;
        int64_t maxWZ = minWZ + worldChunkSize;

        int prevScale = 1 << (lod - 1);
        int prevWorldChunkSize = CHUNK_SIZE * prevScale;
        int prevRadius = LOD_RADII[lod - 1];

        int64_t prevCamCX = static_cast<int64_t>(std::floor(camPos.x / prevWorldChunkSize));
        int64_t prevCamCY = static_cast<int64_t>(std::floor(camPos.y / prevWorldChunkSize));
        int64_t prevCamCZ = static_cast<int64_t>(std::floor(camPos.z / prevWorldChunkSize));

        int64_t prevMinX = (prevCamCX - prevRadius) * prevWorldChunkSize;
        int64_t prevMaxX = (prevCamCX + prevRadius + 1) * prevWorldChunkSize;
        int64_t prevMinY = (prevCamCY - prevRadius) * prevWorldChunkSize;
        int64_t prevMaxY = (prevCamCY + prevRadius + 1) * prevWorldChunkSize;
        int64_t prevMinZ = (prevCamCZ - prevRadius) * prevWorldChunkSize;
        int64_t prevMaxZ = (prevCamCZ + prevRadius + 1) * prevWorldChunkSize;

        return (minWX >= prevMinX && maxWX <= prevMaxX &&
                minWY >= prevMinY && maxWY <= prevMaxY &&
                minWZ >= prevMinZ && maxWZ <= prevMaxZ);
    }
    bool isRegionCoveredByReadyFinerChunks(
        int lod,
        int64_t cx,
        int64_t cy,
        int64_t cz,
        const Vec3& camPos
    ) const {
        IVec3 chunkPos(cx, cy, cz);
        auto it = chunks[lod].find(chunkPos);

        // LOD 0 is the terminal coverage tier: an uploaded mesh, including
        // an uploaded empty chunk, means the region has been resolved.
        if (lod == 0) {
            return it != chunks[0].end() &&
                   it->second &&
                   it->second->isMeshUploaded.load();
        }

        // If this tier is active at this position, it must be uploaded before
        // it can replace its coarser parent.
        if (!shouldSkipCoarseChunk(lod, cx, cy, cz, camPos)) {
            return it != chunks[lod].end() &&
                   it->second &&
                   it->second->isMeshUploaded.load();
        }

        // This chunk is intentionally skipped by the loader, so resolve its
        // coverage through the next finer tier. This keeps higher-LOD
        // transitions correct even where intermediate children are themselves
        // replaced by finer LODs.
        int fineLod = lod - 1;
        int fineWorldChunkSize = CHUNK_SIZE * (1 << fineLod);
        int64_t worldMinX = cx * CHUNK_SIZE * (1 << lod);
        int64_t worldMinY = cy * CHUNK_SIZE * (1 << lod);
        int64_t worldMinZ = cz * CHUNK_SIZE * (1 << lod);
        int64_t baseCX = floorDiv(worldMinX, fineWorldChunkSize);
        int64_t baseCY = floorDiv(worldMinY, fineWorldChunkSize);
        int64_t baseCZ = floorDiv(worldMinZ, fineWorldChunkSize);

        for (int dz = 0; dz < 2; ++dz) {
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    if (!isRegionCoveredByReadyFinerChunks(
                            fineLod,
                            baseCX + dx,
                            baseCY + dy,
                            baseCZ + dz,
                            camPos)) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    bool isCoarseChunkCoveredByReadyFineChunks(const Chunk* coarseChunk, const Vec3& camPos) const {
        if (!coarseChunk || coarseChunk->lod <= 0) return false;

        return shouldSkipCoarseChunk(
            coarseChunk->lod,
            coarseChunk->chunkPos.x,
            coarseChunk->chunkPos.y,
            coarseChunk->chunkPos.z,
            camPos
        ) && isRegionCoveredByReadyFinerChunks(
            coarseChunk->lod,
            coarseChunk->chunkPos.x,
            coarseChunk->chunkPos.y,
            coarseChunk->chunkPos.z,
            camPos
        );
    }

    bool isCoarserParentRequiredForHandoff(const Chunk* fineChunk, const Vec3& camPos) const {
        if (!fineChunk || fineChunk->lod >= NUM_LODS - 1) return false;

        int parentLod = fineChunk->lod + 1;
        int parentWorldChunkSize = CHUNK_SIZE * (1 << parentLod);
        int64_t parentCX = floorDiv(fineChunk->worldMin.x, parentWorldChunkSize);
        int64_t parentCY = floorDiv(fineChunk->worldMin.y, parentWorldChunkSize);
        int64_t parentCZ = floorDiv(fineChunk->worldMin.z, parentWorldChunkSize);

        int64_t camCX = static_cast<int64_t>(std::floor(camPos.x / parentWorldChunkSize));
        int64_t camCY = static_cast<int64_t>(std::floor(camPos.y / parentWorldChunkSize));
        int64_t camCZ = static_cast<int64_t>(std::floor(camPos.z / parentWorldChunkSize));
        int parentRadius = LOD_RADII[parentLod];

        if (std::abs(parentCX - camCX) > parentRadius ||
            std::abs(parentCY - camCY) > parentRadius ||
            std::abs(parentCZ - camCZ) > parentRadius) {
            return false;
        }

        // A parent inside the finer shell is intentionally skipped; it cannot
        // be the fallback for this child during an outward transition.
        return !shouldSkipCoarseChunk(parentLod, parentCX, parentCY, parentCZ, camPos);
    }

    bool isCoarserParentReadyForHandoff(const Chunk* fineChunk, const Vec3& camPos) const {
        if (!isCoarserParentRequiredForHandoff(fineChunk, camPos)) return false;

        int parentLod = fineChunk->lod + 1;
        int parentWorldChunkSize = CHUNK_SIZE * (1 << parentLod);
        IVec3 parentPos(
            floorDiv(fineChunk->worldMin.x, parentWorldChunkSize),
            floorDiv(fineChunk->worldMin.y, parentWorldChunkSize),
            floorDiv(fineChunk->worldMin.z, parentWorldChunkSize)
        );

        auto it = chunks[parentLod].find(parentPos);
        if (it == chunks[parentLod].end() || !it->second) return false;

        const Chunk* parent = it->second.get();
        return parent->isMeshUploaded.load() && !parent->isEmpty;
    }

    bool isChunkOutOfRange(const Chunk* chunk, const Vec3& camPos) const {
        if (!chunk) return true;
        int lod = chunk->lod;
        int scale = 1 << lod;
        int worldChunkSize = CHUNK_SIZE * scale;
        int radius = LOD_RADII[lod] + 1; // Margin

        int64_t camCX = static_cast<int64_t>(std::floor(camPos.x / worldChunkSize));
        int64_t camCY = static_cast<int64_t>(std::floor(camPos.y / worldChunkSize));
        int64_t camCZ = static_cast<int64_t>(std::floor(camPos.z / worldChunkSize));

        if (std::abs(chunk->chunkPos.x - camCX) > radius ||
            std::abs(chunk->chunkPos.y - camCY) > radius ||
            std::abs(chunk->chunkPos.z - camCZ) > radius) {
            // During an outward transition, retain visible fine geometry
            // until the required coarser fallback has a renderable mesh.
            if (!chunk->isEmpty &&
                isCoarserParentRequiredForHandoff(chunk, camPos) &&
                !isCoarserParentReadyForHandoff(chunk, camPos)) {
                return false;
            }
            return true;
        }

        if (lod > 0 && shouldSkipCoarseChunk(lod, chunk->chunkPos.x, chunk->chunkPos.y, chunk->chunkPos.z, camPos)) {
            if (isCoarseChunkCoveredByReadyFineChunks(chunk, camPos)) {
                return true;
            }
        }

        return false;
    }

    void workerThreadFunc() {
        while (!stopThreads) {
            GenerationTask task{ nullptr, 0.0f, 0 };
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [this]() { return stopThreads || !generateQueue.empty(); });

                if (stopThreads) break;

                task = generateQueue.top();
                generateQueue.pop();
            }

            Chunk* chunk = task.chunk;

            if (chunk) {
                Vec3 cameraSnapshot;
                {
                    std::lock_guard<std::mutex> cameraLock(cameraMutex);
                    cameraSnapshot = currentCamPos;
                }
                if (isChunkOutOfRange(chunk, cameraSnapshot)) {
                    chunk->isPendingWork.store(false);
                    continue;
                }

                if (!chunk->isGenerated.load()) {
                    auto t0 = std::chrono::high_resolution_clock::now();
                    MeshBuilder::generateVoxelData(*chunk);
                    auto t1 = std::chrono::high_resolution_clock::now();
                    MeshBuilder::buildMesh(*chunk);
                    auto t2 = std::chrono::high_resolution_clock::now();
                    totalGenTimeUs += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
                    totalMeshTimeUs += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                    chunksProcessed++;
                }
                chunk->isPendingWork.store(false);
            }
        }
    }

public:
    ChunkManager() {
        unsigned int threadCount = std::thread::hardware_concurrency();
        if (threadCount < 2) threadCount = 2;
        if (threadCount > 6) threadCount = 6;

        for (unsigned int i = 0; i < threadCount; ++i) {
            workers.emplace_back(&ChunkManager::workerThreadFunc, this);
        }

        renderableChunks.reserve(1024);
    }

    ~ChunkManager() {
        stopThreads = true;
        cv.notify_all();
        for (auto& t : workers) {
            if (t.joinable()) t.join();
        }

        // Clean up GL meshes
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            for (auto& pair : chunks[lod]) {
                if (pair.second) pair.second->mesh.cleanUp();
            }
            chunks[lod].clear();
        }
        renderableChunks.clear();
    }

    void update(Vec3 cameraPos) {
        {
            std::lock_guard<std::mutex> cameraLock(cameraMutex);
            currentCamPos = cameraPos;
        }
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            for (auto& pair : chunks[lod]) {
                Chunk* chunk = pair.second.get();
                if (!chunk) continue;

                if (chunk->isMeshStaged.load() && !chunk->isMeshUploaded.load()) {
                    chunk->mesh.upload(chunk->stagedVertices, chunk->stagedIndices);
                    chunk->stagedVertices.clear();
                    chunk->stagedVertices.shrink_to_fit();
                    chunk->stagedIndices.clear();
                    chunk->stagedIndices.shrink_to_fit();

                    chunk->isMeshUploaded.store(true);
                    if (!chunk->isEmpty) {
                        renderableChunks.push_back(chunk);
                    }
                }
            }
        }

        // 2. Thread-Safe Dynamic Chunk Unloading & Memory Pruning
        std::unordered_set<Chunk*> erasedChunks;
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            for (auto it = chunks[lod].begin(); it != chunks[lod].end(); ) {
                Chunk* chunk = it->second.get();
                if (isChunkOutOfRange(chunk, cameraPos) && !chunk->isPendingWork.load()) {
                    erasedChunks.insert(chunk);
                    chunk->mesh.cleanUp();
                    it = chunks[lod].erase(it);
                } else {
                    ++it;
                }
            }
        }

        // 3. Compact renderableChunks vector unconditionally to prevent memory bloat
        renderableChunks.erase(
            std::remove_if(renderableChunks.begin(), renderableChunks.end(),
                [&erasedChunks, this, cameraPos](Chunk* chunk) {
                    return !chunk || erasedChunks.count(chunk) > 0 || !chunk->isMeshUploaded.load() || chunk->isEmpty || isChunkOutOfRange(chunk, cameraPos);
                }),
            renderableChunks.end()
        );
        // 3.5 Update coarse chunk coverage flags
        for (Chunk* chunk : renderableChunks) {
            if (chunk && chunk->lod > 0) {
                bool insideFineRegion = shouldSkipCoarseChunk(
                    chunk->lod,
                    chunk->chunkPos.x,
                    chunk->chunkPos.y,
                    chunk->chunkPos.z,
                    cameraPos
                );

                // Keep the coarse fallback visible until its finer coverage is ready.
                // This flag is also used to gate the finer tier in render(), so
                // setting it from geometry alone would expose partial child sets.
                chunk->isFullyCovered = insideFineRegion &&
                    isCoarseChunkCoveredByReadyFineChunks(chunk, cameraPos);
            }
        }

        // 4. Queue new chunks for loading around camera position (Concentric Seamless 3D LOD Shells)
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            int scale = 1 << lod;
            int worldChunkSize = CHUNK_SIZE * scale;

            int64_t camCX = static_cast<int64_t>(std::floor(cameraPos.x / worldChunkSize));
            int64_t camCY = static_cast<int64_t>(std::floor(cameraPos.y / worldChunkSize));
            int64_t camCZ = static_cast<int64_t>(std::floor(cameraPos.z / worldChunkSize));

            int radius = LOD_RADII[lod];

            for (int64_t cz = camCZ - radius; cz <= camCZ + radius; ++cz) {
                for (int64_t cy = camCY - radius; cy <= camCY + radius; ++cy) {
                    for (int64_t cx = camCX - radius; cx <= camCX + radius; ++cx) {
                        if (shouldSkipCoarseChunk(lod, cx, cy, cz, cameraPos)) {
                            continue;
                        }

                        IVec3 cpos(cx, cy, cz);
                        auto it = chunks[lod].find(cpos);
                        if (it == chunks[lod].end()) {
                            auto newChunk = std::make_unique<Chunk>(cpos, lod);
                            Chunk* ptr = newChunk.get();
                            ptr->isPendingWork.store(true);
                            chunks[lod][cpos] = std::move(newChunk);

                            float centerX = static_cast<float>(ptr->worldMin.x) + ptr->worldSize * 0.5f;
                            float centerY = static_cast<float>(ptr->worldMin.y) + ptr->worldSize * 0.5f;
                            float centerZ = static_cast<float>(ptr->worldMin.z) + ptr->worldSize * 0.5f;
                            float dx = centerX - cameraPos.x;
                            float dy = centerY - cameraPos.y;
                            float dz = centerZ - cameraPos.z;
                            GenerationTask task{
                                ptr,
                                dx * dx + dy * dy + dz * dz,
                                queueSequence.fetch_add(1)
                            };
                            {
                                std::lock_guard<std::mutex> lock(queueMutex);
                                generateQueue.push(task);
                            }
                            cv.notify_one();
                        }
                    }
                }
            }
        }
    }

    void render(const Frustum& frustum, Vec3 cameraPos, GLuint shaderProgram, GLint uChunkMinLoc) {
        int renderedChunks = 0;
        float camX = cameraPos.x;
        float camY = cameraPos.y;
        float camZ = cameraPos.z;

        for (Chunk* chunk : renderableChunks) {
            if (!chunk || !chunk->mesh.uploaded) continue;

            if (chunk->lod > 0 && chunk->isFullyCovered) {
                continue;
            }

            if (chunk->lod < NUM_LODS - 1) {
                int parentLod = chunk->lod + 1;
                int parentWorldChunkSize = CHUNK_SIZE * (1 << parentLod);
                int64_t parentCX = floorDiv(chunk->worldMin.x, parentWorldChunkSize);
                int64_t parentCY = floorDiv(chunk->worldMin.y, parentWorldChunkSize);
                int64_t parentCZ = floorDiv(chunk->worldMin.z, parentWorldChunkSize);
                IVec3 parentPos(parentCX, parentCY, parentCZ);

                auto it = chunks[parentLod].find(parentPos);
                if (it != chunks[parentLod].end()) {
                    Chunk* coarseChunk = it->second.get();
                    if (coarseChunk &&
                        coarseChunk->isMeshUploaded.load() &&
                        !coarseChunk->isEmpty &&
                        !coarseChunk->isFullyCovered) {
                        continue;
                    }
                }
            }
            float minX = static_cast<float>(chunk->worldMin.x) - camX;
            float minY = static_cast<float>(chunk->worldMin.y) - camY;
            float minZ = static_cast<float>(chunk->worldMin.z) - camZ;
            float wSize = static_cast<float>(chunk->worldSize);

            Vec3 minP(minX, minY, minZ);
            Vec3 maxP(minX + wSize, minY + wSize, minZ + wSize);

            // Frustum Culling
            if (frustum.intersectsAABB(minP, maxP)) {
                glUniform3f(uChunkMinLoc, minX, minY, minZ);
                chunk->mesh.draw();
                renderedChunks++;
                }
        }
        glBindVertexArray(0);
    }

    void getStats(int& outTotalChunks, int& outUploadedMeshes, int& outPendingTasks) {
        outTotalChunks = 0;
        outUploadedMeshes = static_cast<int>(renderableChunks.size());
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            outTotalChunks += static_cast<int>(chunks[lod].size());
        }
        std::lock_guard<std::mutex> lock(queueMutex);
        outPendingTasks = static_cast<int>(generateQueue.size());
    }

    // Helper for collision checking against solid voxels in world space
    bool isBlockSolidAt(int64_t wx, int64_t wy, int64_t wz) {
        return getBlockInfo(WorldGen::getBlockAt(wx, wy, wz)).isSolid;
    }
};

#endif // CHUNK_MANAGER_HPP
