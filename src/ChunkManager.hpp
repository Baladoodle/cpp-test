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
    static constexpr float SCREEN_SPACE_DIAMETER_THRESHOLD = 2.5f;
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
    IVec3 lastCamChunkPos[NUM_LODS] = {
        IVec3(-999999, -999999, -999999),
        IVec3(-999999, -999999, -999999),
        IVec3(-999999, -999999, -999999),
        IVec3(-999999, -999999, -999999),
        IVec3(-999999, -999999, -999999)
    };
    std::vector<std::shared_ptr<Chunk>> stagedMeshQueue;
    std::mutex stagedMutex;

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
        return false;
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
    void selectHierarchicalNode(
        Chunk* chunk,
        const Frustum& frustum,
        Vec3 cameraPos,
        float projectionScale,
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

        float threshold = chunk->wasSplitLastFrame ? (SCREEN_SPACE_DIAMETER_THRESHOLD * 0.625f)
                                                   : SCREEN_SPACE_DIAMETER_THRESHOLD;
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
                        auto it = chunks[childLod].find(childPos);
                        if (it == chunks[childLod].end() || !it->second ||
                            !it->second->isMeshUploaded.load()) {
                            allChildrenReady = false;
                        }
                    }
                }
            }
        }

        if (wantsChildren && allChildrenReady) {
            chunk->wasSplitLastFrame = true;
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
                        auto it = chunks[childLod].find(childPos);
                        if (it != chunks[childLod].end() && it->second) {
                            selectHierarchicalNode(it->second.get(), frustum, cameraPos, projectionScale, selectedChunks);
                        }
                    }
                }
            }
        } else {
            chunk->wasSplitLastFrame = false;
            if (chunk->isMeshUploaded.load() && !chunk->isEmpty && chunk->mesh.geometry.valid) {
                selectedChunks.push_back(chunk);
            }
            if (wantsChildren && !allChildrenReady) {
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
                            auto it = chunks[childLod].find(childPos);
                            if (it == chunks[childLod].end()) {
                                auto newChunk = std::make_shared<Chunk>(childPos, childLod);
                                newChunk->isPendingWork.store(true);
                                chunks[childLod][childPos] = newChunk;
                                enqueueGeneration(newChunk, cameraPos);
                            } else if (!it->second->isGenerated.load() && !it->second->isPendingWork.load()) {
                                it->second->isPendingWork.store(true);
                                enqueueGeneration(it->second, cameraPos);
                            }
                        }
                    }
                }
            }
        }
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
                        MeshBuilder::finalizeVoxelData(*chunk, nullptr, &mipStore);
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
                    mipStore.publishSection(chunk->lod, chunk->chunkPos, chunk->blocks);
                    chunk->mipRemeshQueued.store(false);
                    chunksProcessed++;
                    if (chunk->isMeshStaged.load()) {
                        std::lock_guard<std::mutex> lock(stagedMutex);
                        stagedMeshQueue.push_back(chunk);
                    }
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
                    geometryArena.releaseImmediate(pair.second->mesh.geometry);
                    pair.second->mesh.cleanUp();
                }
            }
            chunks[lod].clear();
        }
        renderableChunks.clear();
        renderableSet.clear();
    }

    void update(Vec3 cameraPos) {
        geometryArena.advanceFrame();
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
        std::vector<std::shared_ptr<Chunk>> stagedToUpload;
        {
            std::lock_guard<std::mutex> lock(stagedMutex);
            stagedToUpload.swap(stagedMeshQueue);
        }

        size_t uploadedBytesThisFrame = 0;
        for (auto& chunk : stagedToUpload) {
            if (!chunk || !chunk->isMeshStaged.load()) continue;
            size_t meshBytes =
                chunk->stagedVertices.size() * sizeof(VoxelVertex) +
                chunk->stagedIndices.size() * sizeof(uint32_t);
            if (uploadedBytesThisFrame > 0 &&
                uploadedBytesThisFrame + meshBytes > MAX_GPU_UPLOAD_BYTES_PER_FRAME) {
                std::lock_guard<std::mutex> lock(stagedMutex);
                stagedMeshQueue.push_back(chunk);
                continue;
            }

            GeometryHandle oldGeometry = chunk->mesh.geometry;
            GeometryHandle geometry;
            if (meshBytes > 0) {
                geometry = geometryArena.upload(chunk->stagedVertices, chunk->stagedIndices);
                if (!geometry.valid) continue;
            }
            chunk->mesh.attach(geometry);
            geometryArena.release(oldGeometry);
            ++sceneRevision;
            chunk->stagedVertices.clear();
            chunk->stagedIndices.clear();
            chunk->isMeshStaged.store(false);
            chunk->isMeshUploaded.store(true);
            uploadedBytesThisFrame += meshBytes;
        }

        bool cameraCellChanged = false;
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            int scale = 1 << lod;
            int worldChunkSize = CHUNK_SIZE * scale;
            int64_t camCX = static_cast<int64_t>(std::floor(cameraPos.x / worldChunkSize));
            int64_t camCY = static_cast<int64_t>(std::floor(cameraPos.y / worldChunkSize));
            int64_t camCZ = static_cast<int64_t>(std::floor(cameraPos.z / worldChunkSize));
            IVec3 camCell(camCX, camCY, camCZ);
            if (camCell != lastCamChunkPos[lod]) {
                cameraCellChanged = true;
                break;
            }
        }

        if (cameraCellChanged) {
            for (int lod = 0; lod < NUM_LODS; ++lod) {
                for (auto it = chunks[lod].begin(); it != chunks[lod].end(); ) {
                    Chunk* chunk = it->second.get();
                    if (isChunkOutOfRange(chunk, cameraPos)) {
                        geometryArena.release(chunk->mesh.geometry);
                        ++sceneRevision;
                        chunk->mesh.cleanUp();
                        it = chunks[lod].erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        // 4. Queue new chunks for loading around camera position (Concentric Seamless 3D LOD Shells)
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            int scale = 1 << lod;
            int worldChunkSize = CHUNK_SIZE * scale;

            int64_t camCX = static_cast<int64_t>(std::floor(cameraPos.x / worldChunkSize));
            int64_t camCY = static_cast<int64_t>(std::floor(cameraPos.y / worldChunkSize));
            int64_t camCZ = static_cast<int64_t>(std::floor(cameraPos.z / worldChunkSize));

            IVec3 camCell(camCX, camCY, camCZ);
            if (camCell == lastCamChunkPos[lod]) continue;
            lastCamChunkPos[lod] = camCell;
            int radius = LOD_RADII[lod];

            for (int64_t cz = camCZ - radius; cz <= camCZ + radius; ++cz) {
                for (int64_t cy = camCY - radius; cy <= camCY + radius; ++cy) {
                    for (int64_t cx = camCX - radius; cx <= camCX + radius; ++cx) {

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

        float projectionScale = 0.5f * projectionY * static_cast<float>(viewportHeight);
        std::vector<Chunk*> selectedChunks;
        selectedChunks.reserve(512);

        for (auto& pair : chunks[NUM_LODS - 1]) {
            if (pair.second) {
                selectHierarchicalNode(pair.second.get(), frustum, cameraPos, projectionScale, selectedChunks);
            }
        }

        std::vector<SectionGpuMetadata> drawMetadata;
        std::vector<DrawElementsIndirectCommand> drawCommands;
        drawMetadata.reserve(selectedChunks.size());
        drawCommands.reserve(selectedChunks.size());

        for (size_t i = 0; i < selectedChunks.size(); ++i) {
            Chunk* chunk = selectedChunks[i];
            SectionGpuMetadata meta;
            Vec3 minP, maxP;
            getChunkBounds(chunk, cameraPos, minP, maxP);
            meta.chunkMinLod[0] = minP.x;
            meta.chunkMinLod[1] = minP.y;
            meta.chunkMinLod[2] = minP.z;
            meta.chunkMinLod[3] = static_cast<float>(chunk->lod);
            meta.sectionBounds[0] = static_cast<float>(chunk->worldSize);
            meta.sectionBounds[1] = static_cast<float>(1 << chunk->lod);

            DrawElementsIndirectCommand cmd;
            cmd.count = chunk->mesh.geometry.indexCount;
            cmd.instanceCount = 1;
            cmd.firstIndex = static_cast<uint32_t>(chunk->mesh.geometry.indexOffset);
            cmd.baseVertex = static_cast<int32_t>(chunk->mesh.geometry.baseVertex);
            cmd.baseInstance = static_cast<uint32_t>(i);

            drawMetadata.push_back(meta);
            drawCommands.push_back(cmd);
        }

        if (diagnosticsEnabled) {
            lastDiagnostics.nodeCount = drawCommands.size();
            lastDiagnostics.rootCount = chunks[NUM_LODS - 1].size();
            for (const auto& cmd : drawCommands) {
                lastDiagnostics.candidateIndices += cmd.count;
                lastDiagnostics.largestCandidate = std::max(lastDiagnostics.largestCandidate, cmd.count);
            }
            lastDiagnostics.gpuTraversalVerified = true;
            lastDiagnostics.commandPayloadValid = true;
        }

        geometryArena.uploadDrawData(drawMetadata, drawCommands);
        glUseProgram(voxelShaderProgram);
        geometryArena.drawIndirect(drawCommands.size());
        return true;
    }

    void markGpuWorkSubmitted() {
        geometryArena.markSubmitted();
    }


    void getStats(int& outTotalChunks, int& outUploadedMeshes, int& outPendingTasks) {
        outTotalChunks = 0;
        outUploadedMeshes = 0;
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            outTotalChunks += static_cast<int>(chunks[lod].size());
            for (const auto& pair : chunks[lod]) {
                if (pair.second && pair.second->isMeshUploaded.load() && !pair.second->isEmpty) {
                    ++outUploadedMeshes;
                }
            }
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
