#ifndef CHUNK_MANAGER_HPP
#define CHUNK_MANAGER_HPP

#include "Chunk.hpp"
#include "MeshBuilder.hpp"
#include "VoxelMip.hpp"
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

    struct FrameDiagnostics {
        uint64_t frameIndex = 0;
        size_t nodeCount = 0;
        size_t rootCount = 0;
        uint64_t candidateIndices = 0;
        uint32_t largestCandidate = 0;
        bool gpuTraversalVerified = false;
        bool commandPayloadValid = true;
        IndirectCommandDiagnostics emittedCommands;
    };

private:
    GeometryArena geometryArena;
    VoxelMipStore mipStore;
    uint64_t sceneRevision = 1;
    std::unordered_map<IVec3, std::shared_ptr<Chunk>, IVec3Hash> chunks[NUM_LODS];
    std::vector<Chunk*> renderableChunks;
    std::unordered_set<Chunk*> renderableSet;
    bool gpuTraversalProbed = false;
    bool diagnosticsEnabled = false;
    uint64_t diagnosticsFrameCounter = 0;
    bool diagnosticsCommandFailureReported = false;
    FrameDiagnostics lastDiagnostics;
    static constexpr float SCREEN_SPACE_DIAMETER_THRESHOLD = 64.0f;
    static constexpr size_t MAX_GPU_UPLOAD_BYTES_PER_FRAME = 16ull * 1024ull * 1024ull;
    static constexpr size_t MAX_PENDING_GENERATION_TASKS = 4096;

    inline static int64_t floorDiv(int64_t a, int64_t b) {
        int64_t res = a / b;
        int64_t rem = a % b;
        if (rem != 0 && ((a < 0) ^ (b < 0))) {
            res -= 1;
        }
        return res;
    }
    struct GenerationTask {
        std::shared_ptr<Chunk> chunk;
        float priority; // normalized distance squared: distSq / (worldSize * worldSize)
        uint64_t sequence;
        uint64_t workToken;

        bool operator<(const GenerationTask& other) const {
            if (priority == other.priority) {
                return sequence > other.sequence;
            }
            return priority > other.priority;
        }
    };

    std::vector<std::thread> workers;
    std::priority_queue<GenerationTask> generateQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<bool> stopThreads{false};
    std::atomic<uint64_t> queueSequence{0};

    static void cancelTaskIfCurrent(const GenerationTask& task) {
        if (!task.chunk) return;
        uint64_t currentToken = task.chunk->workToken.load();
        if (currentToken == task.workToken &&
            task.chunk->workToken.compare_exchange_strong(currentToken, currentToken + 1)) {
            task.chunk->mipRemeshQueued.store(false);
            task.chunk->isPendingWork.store(false);
        }
    }
public:
    std::atomic<uint64_t> chunksProcessed{0};
    std::atomic<uint64_t> totalGenTimeUs{0};
    std::atomic<uint64_t> totalMeshTimeUs{0};
private:
    Vec3 currentCamPos{0, 0, 0};
    std::mutex cameraMutex;

    static void getChunkBounds(const Chunk* chunk, Vec3 cameraPos, Vec3& minP, Vec3& maxP) {
        minP = Vec3(
            static_cast<float>(chunk->worldMin.x) - cameraPos.x,
            static_cast<float>(chunk->worldMin.y) - cameraPos.y,
            static_cast<float>(chunk->worldMin.z) - cameraPos.z
        );
        float size = static_cast<float>(chunk->worldSize);
        maxP = minP + Vec3(size);
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

    // Worker cancellation only needs the conservative shell test. It must not
    // inspect the main-thread chunk maps while a task is being discarded.
    bool isChunkOutsideGenerationWindow(const Chunk* chunk, const Vec3& camPos) const {
        if (!chunk) return true;
        int worldChunkSize = chunk->worldSize;
        int radius = LOD_RADII[chunk->lod] + 1;
        int64_t camCX = static_cast<int64_t>(std::floor(camPos.x / worldChunkSize));
        int64_t camCY = static_cast<int64_t>(std::floor(camPos.y / worldChunkSize));
        int64_t camCZ = static_cast<int64_t>(std::floor(camPos.z / worldChunkSize));
        return std::abs(chunk->chunkPos.x - camCX) > radius ||
               std::abs(chunk->chunkPos.y - camCY) > radius ||
               std::abs(chunk->chunkPos.z - camCZ) > radius;
    }

    void trimGenerationQueue(const Vec3& cameraPos) {
        std::vector<GenerationTask> kept;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (generateQueue.size() <= MAX_PENDING_GENERATION_TASKS) return;

            kept.reserve(generateQueue.size());
            while (!generateQueue.empty()) {
                GenerationTask task = generateQueue.top();
                generateQueue.pop();
                if (task.chunk && !isChunkOutsideGenerationWindow(task.chunk.get(), cameraPos)) {
                    kept.push_back(std::move(task));
                } else if (task.chunk) {
                    cancelTaskIfCurrent(task);
                }
            }
        }

        std::sort(kept.begin(), kept.end(), [](const GenerationTask& a, const GenerationTask& b) {
            if (a.priority == b.priority) return a.sequence < b.sequence;
            return a.priority < b.priority;
        });
        if (kept.size() > MAX_PENDING_GENERATION_TASKS) {
            for (size_t i = MAX_PENDING_GENERATION_TASKS; i < kept.size(); ++i) {
                cancelTaskIfCurrent(kept[i]);
            }
            kept.resize(MAX_PENDING_GENERATION_TASKS);
        }

        std::lock_guard<std::mutex> lock(queueMutex);
        for (GenerationTask& task : kept) generateQueue.push(std::move(task));
    }

    void enqueueGeneration(const std::shared_ptr<Chunk>& chunk, const Vec3& cameraPos) {
        if (!chunk) return;
        float centerX = static_cast<float>(chunk->worldMin.x) + chunk->worldSize * 0.5f;
        float centerY = static_cast<float>(chunk->worldMin.y) + chunk->worldSize * 0.5f;
        float centerZ = static_cast<float>(chunk->worldMin.z) + chunk->worldSize * 0.5f;
        float dx = centerX - cameraPos.x;
        float dy = centerY - cameraPos.y;
        float dz = centerZ - cameraPos.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        float worldSize = static_cast<float>(chunk->worldSize);
        float normDistSq = distSq / (worldSize * worldSize);
        GenerationTask task{
            chunk,
            normDistSq,
            queueSequence.fetch_add(1),
            chunk->workToken.fetch_add(1) + 1
        };
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (generateQueue.size() >= MAX_PENDING_GENERATION_TASKS) {
                cancelTaskIfCurrent(task);
                return;
            }
            generateQueue.push(task);
        }
        cv.notify_one();
    }

    void workerThreadFunc() {
        while (!stopThreads) {
            GenerationTask task{ nullptr, 0.0f, 0, 0 };
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [this]() { return stopThreads || !generateQueue.empty(); });

                if (stopThreads) break;

                task = generateQueue.top();
                generateQueue.pop();
            }

            std::shared_ptr<Chunk> chunk = std::move(task.chunk);

            if (chunk) {
                if (chunk->workToken.load() != task.workToken) continue;
                Vec3 cameraSnapshot;
                {
                    std::lock_guard<std::mutex> cameraLock(cameraMutex);
                    cameraSnapshot = currentCamPos;
                }
                if (isChunkOutsideGenerationWindow(chunk.get(), cameraSnapshot)) {
                    cancelTaskIfCurrent(task);
                    continue;
                }

                if (!chunk->isGenerated.load()) {
                    auto t0 = std::chrono::high_resolution_clock::now();
                    uint64_t mipRevision = 0;
                    bool loadedFromMip = chunk->lod > 0 &&
                        mipStore.readCompleteSection(
                            chunk->lod,
                            chunk->chunkPos,
                            chunk->blocks,
                            &mipRevision
                        );
                    if (loadedFromMip) {
                        MeshBuilder::finalizeVoxelData(*chunk);
                        chunk->mipRevision.store(mipRevision);
                    } else {
                        MeshBuilder::generateVoxelData(*chunk);
                        chunk->mipRevision.store(0);
                    }
                    auto t1 = std::chrono::high_resolution_clock::now();
                    MeshBuilder::buildMesh(*chunk);
                    auto t2 = std::chrono::high_resolution_clock::now();
                    totalGenTimeUs += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
                    totalMeshTimeUs += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                    mipStore.publishSection(chunk->lod, chunk->chunkPos, chunk->blocks);
                    chunk->mipRemeshQueued.store(false);
                    chunksProcessed++;
                }
                if (chunk->workToken.load() == task.workToken) {
                    chunk->mipRemeshQueued.store(false);
                    chunk->isPendingWork.store(false);
                }
            }
        }
    }

public:
    ChunkManager(int viewportWidth = 1280, int viewportHeight = 720) {
        if (!geometryArena.initialize()) {
            std::cerr << "Failed to initialize the shared voxel geometry arena.\n";
        }

        unsigned int threadCount = std::thread::hardware_concurrency();
        if (threadCount < 2) threadCount = 2;
        if (threadCount > 8) threadCount = 8;
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
                if (pair.second) {
                    geometryArena.release(pair.second->mesh.geometry);
                    pair.second->mesh.cleanUp();
                }
            }
            chunks[lod].clear();
        }
        renderableChunks.clear();
        renderableSet.clear();
    }

    void update(Vec3 cameraPos) {
        geometryArena.waitForSubmittedWork();
        {
            std::lock_guard<std::mutex> cameraLock(cameraMutex);
            currentCamPos = cameraPos;
        }
        trimGenerationQueue(cameraPos);

        // Child completion can replace an already-generated coarse section.
        // Keep its current GPU mesh as a fallback while the worker rebuilds
        // the section from the now-complete parent mip.
        for (const VoxelMipStore::CompletedSection& completed : mipStore.drainCompleted()) {
            if (completed.lod <= 0 || completed.lod >= NUM_LODS) continue;
            auto it = chunks[completed.lod].find(completed.chunkPos);
            if (it == chunks[completed.lod].end() || !it->second) continue;

            Chunk* chunk = it->second.get();
            if (chunk->isPendingWork.load()) {
                mipStore.requeueCompleted(completed);
                continue;
            }
            if (chunk->mipRemeshQueued.exchange(true)) continue;
            uint64_t currentRevision = chunk->mipRevision.load();
            if (completed.revision <= currentRevision ||
                !mipStore.readCompleteSection(chunk->lod, chunk->chunkPos, chunk->blocks)) {
                chunk->mipRemeshQueued.store(false);
                continue;
            }

            chunk->isGenerated.store(false);
            chunk->isMeshStaged.store(false);
            chunk->stagedVertices.clear();
            chunk->stagedIndices.clear();
            chunk->isPendingWork.store(true);
            enqueueGeneration(it->second, cameraPos);
        }

        // Uploading many completed meshes in one main-thread pass can stall
        // the renderer even when generation itself happened off-thread. Keep
        // this upload queue bounded; already resident meshes remain drawable
        // while the rest wait for a later frame.
        size_t uploadedBytesThisFrame = 0;
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            for (auto& pair : chunks[lod]) {
                Chunk* chunk = pair.second.get();
                if (!chunk) continue;

                if (chunk->isMeshStaged.load()) {
                    size_t meshBytes =
                        chunk->stagedVertices.size() * sizeof(VoxelVertex) +
                        chunk->stagedIndices.size() * sizeof(uint32_t);
                    if (uploadedBytesThisFrame > 0 &&
                        uploadedBytesThisFrame + meshBytes > MAX_GPU_UPLOAD_BYTES_PER_FRAME) {
                        continue;
                    }

                    GeometryHandle oldGeometry = chunk->mesh.geometry;
                    GeometryHandle geometry;
                    if (meshBytes > 0) {
                        geometry = geometryArena.upload(chunk->stagedVertices, chunk->stagedIndices);
                        if (!geometry.valid) {
                            continue;
                        }
                    }
                    chunk->mesh.attach(geometry);
                    geometryArena.release(oldGeometry);
                    ++sceneRevision;
                    chunk->stagedVertices.clear();
                    chunk->stagedVertices.shrink_to_fit();
                    chunk->stagedIndices.clear();
                    chunk->stagedIndices.shrink_to_fit();
                    chunk->isMeshStaged.store(false);

                    uploadedBytesThisFrame += meshBytes;

                    chunk->isMeshUploaded.store(true);
                    if (!chunk->isEmpty && renderableSet.insert(chunk).second) {
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
                if (isChunkOutOfRange(chunk, cameraPos)) {
                    erasedChunks.insert(chunk);
                    geometryArena.release(chunk->mesh.geometry);
                    ++sceneRevision;
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
        renderableSet.clear();
        for (Chunk* chunk : renderableChunks) {
            if (chunk) renderableSet.insert(chunk);
        }

        // A section can leave the render list when a finer replacement is
        // ready and later become the fallback again after that replacement is
        // evicted. Re-admit uploaded sections without duplicating entries.
        // Duplicate pointers corrupt the GPU parent/child graph and make
        // traversal cost grow every time a mip section is remeshed.
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            for (auto& pair : chunks[lod]) {
                Chunk* chunk = pair.second.get();
                if (!chunk || chunk->isEmpty || !chunk->isMeshUploaded.load() ||
                    isChunkOutOfRange(chunk, cameraPos)) {
                    continue;
                }
                if (renderableSet.insert(chunk).second) {
                    renderableChunks.push_back(chunk);
                }
            }
        }

        // Mark a coarse mesh as fully replaceable only after all finer
        // sections covering its region are uploaded. The render path uses
        // this for a bounded, resident-chunk handoff instead of recursively
        // walking every virtual root every frame.
        for (Chunk* chunk : renderableChunks) {
            if (!chunk || chunk->lod <= 0) continue;
            bool insideFineRegion = shouldSkipCoarseChunk(
                chunk->lod,
                chunk->chunkPos.x,
                chunk->chunkPos.y,
                chunk->chunkPos.z,
                cameraPos
            );
            chunk->isFullyCovered = insideFineRegion &&
                isCoarseChunkCoveredByReadyFineChunks(chunk, cameraPos);
        }

        // A worker may have discarded a queued section after the camera moved
        // past it. Keep unfinished sections retryable when they come back into
        // the active shell instead of leaving a permanent hole in the map.
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            for (auto& pair : chunks[lod]) {
                Chunk* chunk = pair.second.get();
                if (!chunk || chunk->isGenerated.load() || chunk->isPendingWork.load()) continue;
                if (isChunkOutOfRange(chunk, cameraPos)) continue;
                if (chunk->mipRemeshQueued.exchange(true)) continue;
                chunk->isPendingWork.store(true);
                enqueueGeneration(pair.second, cameraPos);
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
                            auto newChunk = std::make_shared<Chunk>(cpos, lod);
                            newChunk->isPendingWork.store(true);
                            chunks[lod][cpos] = newChunk;

                            enqueueGeneration(newChunk, cameraPos);
                        }
                    }
                }
            }
        }

        mipStore.prune(cameraPos);
    }

    bool render(
        const Frustum& frustum,
        Vec3 cameraPos,
        float verticalFovRadians,
        int viewportWidth,
        int viewportHeight,
        GLuint voxelShaderProgram,
        GLuint visibilityProgram,
        float projectionY,
        const Mat4& view,
        const Mat4& viewProjection
    ) {
        if (diagnosticsEnabled) {
            lastDiagnostics = FrameDiagnostics{};
            lastDiagnostics.frameIndex = diagnosticsFrameCounter++;
        }
        if (!geometryArena.isInitialized()) {
            std::cerr << "GPU traversal unavailable: geometry arena is not initialized.\n";
            return false;
        }
        if (visibilityProgram == 0) {
            return false;
        }

        // The window's framebuffer can be smaller than its logical size on a
        // scaled Wayland/Xwayland desktop. Keep the Hi-Z resources matched to

        std::vector<TraversalNodeGpu> nodes;
        std::vector<std::vector<uint32_t>> childLists;
        std::vector<int32_t> parentIndices;
        std::unordered_map<Chunk*, uint32_t> nodeIndices;
        nodes.reserve(renderableChunks.size());
        childLists.reserve(renderableChunks.size());
        parentIndices.reserve(renderableChunks.size());

        for (Chunk* chunk : renderableChunks) {
            if (!chunk || !chunk->mesh.uploaded || !chunk->mesh.geometry.valid) continue;
            TraversalNodeGpu node;
            Vec3 minP, maxP;
            getChunkBounds(chunk, cameraPos, minP, maxP);
            node.chunkMinLod[0] = minP.x;
            node.chunkMinLod[1] = minP.y;
            node.chunkMinLod[2] = minP.z;
            node.chunkMinLod[3] = static_cast<float>(chunk->lod);
            node.sectionBounds[0] = static_cast<float>(chunk->worldSize);
            node.topology[2] = chunk->isFullyCovered ? 1u : 0u;
            node.topology[3] = 1u;
            node.draw0[0] = chunk->mesh.geometry.indexCount;
            node.draw0[1] = 1u;
            node.draw0[2] = static_cast<uint32_t>(chunk->mesh.geometry.indexOffset);
            node.draw0[3] = static_cast<uint32_t>(chunk->mesh.geometry.baseVertex);
            node.draw1[0] = static_cast<uint32_t>(nodes.size());
            uint32_t nodeIndex = static_cast<uint32_t>(nodes.size());
            nodeIndices[chunk] = nodeIndex;
            nodes.push_back(node);
            childLists.emplace_back();
            parentIndices.push_back(-1);
        }

        for (Chunk* chunk : renderableChunks) {
            auto childIt = nodeIndices.find(chunk);
            // LOD4 is the coarsest tier and has no parent tier. Guard the
            // array access here; once the first LOD4 mesh arrived this used
            // chunks[5], corrupting the map lookup and crashing the renderer.
            if (childIt == nodeIndices.end() || !chunk ||
                chunk->lod <= 0 || chunk->lod >= NUM_LODS - 1) continue;
            int parentLod = chunk->lod + 1;
            int parentWorldChunkSize = CHUNK_SIZE * (1 << parentLod);
            IVec3 parentPos(
                floorDiv(chunk->worldMin.x, parentWorldChunkSize),
                floorDiv(chunk->worldMin.y, parentWorldChunkSize),
                floorDiv(chunk->worldMin.z, parentWorldChunkSize)
            );
            auto parentIt = chunks[parentLod].find(parentPos);
            if (parentIt == chunks[parentLod].end() || !parentIt->second) continue;
            auto parentNodeIt = nodeIndices.find(parentIt->second.get());
            if (parentNodeIt == nodeIndices.end()) continue;
            parentIndices[childIt->second] = static_cast<int32_t>(parentNodeIt->second);
            childLists[parentNodeIt->second].push_back(childIt->second);
        }

        std::vector<uint32_t> roots;
        std::vector<uint32_t> childLinks;
        roots.reserve(nodes.size());
        for (uint32_t i = 0; i < nodes.size(); ++i) {
            if (parentIndices[i] < 0) {
                roots.push_back(i);
            }
            nodes[i].topology[0] = static_cast<uint32_t>(childLinks.size());
            nodes[i].topology[1] = static_cast<uint32_t>(childLists[i].size());
            nodes[i].topology[2] = nodes[i].topology[2] && !childLists[i].empty() ? 1u : 0u;
            childLinks.insert(childLinks.end(), childLists[i].begin(), childLists[i].end());
        }

        if (diagnosticsEnabled) {
            lastDiagnostics.nodeCount = nodes.size();
            lastDiagnostics.rootCount = roots.size();
            for (const TraversalNodeGpu& node : nodes) {
                lastDiagnostics.candidateIndices += node.draw0[0];
                lastDiagnostics.largestCandidate = std::max(lastDiagnostics.largestCandidate, node.draw0[0]);
            }
        }

        geometryArena.uploadTraversalData(nodes, roots, childLinks);
        geometryArena.dispatchTraversal(
            visibilityProgram,
            view,
            viewProjection,
            projectionY,
            viewportWidth,
            viewportHeight,
            SCREEN_SPACE_DIAMETER_THRESHOLD,
            nodes.size(),
            roots.size()
        );

        // Validate the first populated traversal result once. This is a
        // startup guard for drivers that accept the compute shader but fail
        // to execute its SSBO/indirect-command path correctly. GPU traversal
        // is mandatory, so a failed probe is fatal to this render session.
        if (!gpuTraversalProbed) {
            bool visibleGeometryPotential = false;
            for (Chunk* chunk : renderableChunks) {
                if (!chunk || !chunk->mesh.uploaded || !chunk->mesh.geometry.valid) continue;
                Vec3 minP, maxP;
                getChunkBounds(chunk, cameraPos, minP, maxP);
                if (frustum.intersectsAABB(minP, maxP)) {
                    visibleGeometryPotential = true;
                    break;
                }
            }
            if (visibleGeometryPotential) {
                gpuTraversalProbed = true;
                if (!geometryArena.hasNonZeroIndirectCommand(nodes.size())) {
                    std::cerr << "GPU traversal produced no visible commands; refusing CPU traversal fallback.\n";
                    return false;
                }
            }
        }
        if (diagnosticsEnabled) {
            bool commandPayloadValid = geometryArena.validateIndirectCommands(
                nodes,
                !diagnosticsCommandFailureReported
            );
            lastDiagnostics.commandPayloadValid = commandPayloadValid;
            if (!commandPayloadValid) {
                diagnosticsCommandFailureReported = true;
                // Keep the diagnostic loop alive, but never submit an
                // untrusted indirect command buffer to the driver.
                return true;
            }
        }
        glUseProgram(voxelShaderProgram);
        geometryArena.drawIndirect(nodes.size());
        if (diagnosticsEnabled) lastDiagnostics.gpuTraversalVerified = gpuTraversalProbed;
        return true;
    }

    void markGpuWorkSubmitted() {
        geometryArena.markSubmitted();
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

    void setDiagnosticsEnabled(bool enabled) {
        diagnosticsEnabled = enabled;
        diagnosticsCommandFailureReported = false;
    }

    const FrameDiagnostics& getFrameDiagnostics() const {
        return lastDiagnostics;
    }

    void sampleGpuCommandDiagnostics() {
        if (!diagnosticsEnabled) return;
        lastDiagnostics.emittedCommands = geometryArena.inspectIndirectCommands(lastDiagnostics.nodeCount);
    }

    // Helper for collision checking against solid voxels in world space
    bool isBlockSolidAt(int64_t wx, int64_t wy, int64_t wz) {
        return getBlockInfo(WorldGen::getBlockAt(wx, wy, wz)).isSolid;
    }
};

#endif // CHUNK_MANAGER_HPP
