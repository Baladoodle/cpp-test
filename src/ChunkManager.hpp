#ifndef CHUNK_MANAGER_HPP
#define CHUNK_MANAGER_HPP

#include "Chunk.hpp"
#include "ChunkBorderRenderer.hpp"
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
#include <deque>
#include <iostream>
#include <cmath>

class ChunkManager {
public:
    static constexpr int NUM_LODS = 5; // LOD 0 through LOD 4 active
    const int LOD_RADII[NUM_LODS] = { 2, 2, 2, 2, 2 };
    // lower levels are requested only near the camera; coarse roots provide
    // fallback coverage while fine meshes are generated.
    static constexpr float LOD_MAX_DISTANCE[NUM_LODS] = {
        192.0f, 384.0f, 768.0f, 1536.0f, 10000.0f
    };

    struct FrameDiagnostics {
        uint64_t frameIndex = 0;
        size_t nodeCount = 0;
        size_t rootCount = 0;
        uint64_t candidateIndices = 0;
        uint32_t largestCandidate = 0;
        bool cpuTraversalUsed = false;
        bool commandPayloadValid = true;
        IndirectCommandDiagnostics emittedCommands;
    };

private:
    GeometryArena geometryArena;
    uint64_t sceneRevision = 1;
    std::unordered_map<IVec3, std::shared_ptr<Chunk>, IVec3Hash> chunks[NUM_LODS];
    std::vector<Chunk*> renderableChunks;
    std::unordered_set<Chunk*> renderableSet;
    std::vector<Chunk*> lastSelectedChunks;
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
    enum class WorkType : uint8_t {
        Generate,
        Mesh
    };

    struct GenerationTask {
        std::shared_ptr<Chunk> chunk;
        float priority; // normalized distance squared: distSq / (worldSize * worldSize)
        uint64_t sequence;
        uint64_t workToken;
        WorkType type;
        std::shared_ptr<MeshingNeighborhood> neighborhood;

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

    struct LightingReadyChunk {
        std::shared_ptr<Chunk> chunk;
        std::shared_ptr<MeshingNeighborhood> neighborhood;
        uint64_t workToken;
    };

    struct LightNode {
        std::shared_ptr<Chunk> chunk;
        int8_t x;
        int8_t y;
        int8_t z;
    };

    std::vector<LightingReadyChunk> lightingReadyQueue;
    std::mutex lightingMutex;
    std::deque<LightNode> lightQueue;
    std::unordered_map<Chunk*, std::shared_ptr<MeshingNeighborhood>> pendingNeighborhoods;
    static void cancelTaskIfCurrent(const GenerationTask& task) {
        if (!task.chunk) return;
        uint64_t currentToken = task.chunk->workToken.load();
        if (currentToken == task.workToken &&
            task.chunk->workToken.compare_exchange_strong(currentToken, currentToken + 1)) {
            task.chunk->isPendingWork.store(false);
            if (task.type == WorkType::Mesh) {
                task.chunk->isMeshQueued.store(false);
            }
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
    std::atomic<size_t> stagedMeshBytes{0};
    static constexpr size_t MAX_STAGED_MESH_BYTES = 256ull * 1024ull * 1024ull;

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
            
            if (chunk->isMeshUploaded.load() && !chunk->isEmpty && chunk->mesh.geometry.valid) {
                selectedChunks.push_back(chunk);
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

    float calculateTaskPriority(const Chunk& chunk, const Vec3& cameraPos) const {
        float centerX = static_cast<float>(chunk.worldMin.x) + chunk.worldSize * 0.5f;
        float centerY = static_cast<float>(chunk.worldMin.y) + chunk.worldSize * 0.5f;
        float centerZ = static_cast<float>(chunk.worldMin.z) + chunk.worldSize * 0.5f;
        float dx = centerX - cameraPos.x;
        float dy = centerY - cameraPos.y;
        float dz = centerZ - cameraPos.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        float worldSize = static_cast<float>(chunk.worldSize);
        return (distSq / (worldSize * worldSize));
    }

    void trimGenerationQueue(const Vec3& cameraPos) {
        std::vector<GenerationTask> kept;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (generateQueue.empty()) return;

            kept.reserve(generateQueue.size());
            while (!generateQueue.empty()) {
                GenerationTask task = generateQueue.top();
                generateQueue.pop();
                if (task.chunk && !isChunkOutsideGenerationWindow(task.chunk.get(), cameraPos)) {
                    task.priority = calculateTaskPriority(*task.chunk, cameraPos);
                    kept.push_back(std::move(task));
                } else if (task.chunk) {
                    cancelTaskIfCurrent(task);
                }
            }

            for (GenerationTask& task : kept) {
                generateQueue.push(std::move(task));
            }
        }
    }

    void enqueueGeneration(const std::shared_ptr<Chunk>& chunk, const Vec3& cameraPos) {
        if (!chunk) return;
        float normDistSq = calculateTaskPriority(*chunk, cameraPos);
        GenerationTask task{
            chunk,
            normDistSq,
            queueSequence.fetch_add(1),
            chunk->workToken.fetch_add(1) + 1,
            WorkType::Generate,
            nullptr
        };
        chunk->isPendingWork.store(true);
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (generateQueue.size() >= MAX_PENDING_GENERATION_TASKS) {
                std::vector<GenerationTask> kept;
                kept.reserve(generateQueue.size());
                while (!generateQueue.empty()) {
                    GenerationTask t = generateQueue.top();
                    generateQueue.pop();
                    if (t.chunk && !isChunkOutsideGenerationWindow(t.chunk.get(), cameraPos)) {
                        kept.push_back(std::move(t));
                    } else if (t.chunk) {
                        cancelTaskIfCurrent(t);
                    }
                }
                for (GenerationTask& t : kept) generateQueue.push(std::move(t));
            }
            if (generateQueue.size() < MAX_PENDING_GENERATION_TASKS) {
                generateQueue.push(std::move(task));
            } else {
                cancelTaskIfCurrent(task);
                return;
            }
        }
        cv.notify_one();
    }
    std::array<std::shared_ptr<Chunk>, 6> getSameLodNeighbors(const Chunk& chunk) const {
        std::array<std::shared_ptr<Chunk>, 6> neighbors{};
        const int dx[6] = { 1, -1,  0,  0,  0,  0 };
        const int dy[6] = { 0,  0,  1, -1,  0,  0 };
        const int dz[6] = { 0,  0, 0,  0,  1, -1 };
        for (int direction = 0; direction < 6; ++direction) {
            IVec3 neighborPos(
                chunk.chunkPos.x + dx[direction],
                chunk.chunkPos.y + dy[direction],
                chunk.chunkPos.z + dz[direction]
            );
            auto it = chunks[chunk.lod].find(neighborPos);
            if (it != chunks[chunk.lod].end()) {
                neighbors[direction] = it->second;
            }
        }
        return neighbors;
    }

    void enqueueLightFace(const std::shared_ptr<Chunk>& chunk, int direction) {
        if (!chunk || !chunk->isGenerated.load(std::memory_order_acquire)) return;
        for (int a = 0; a < CHUNK_SIZE; ++a) {
            for (int b = 0; b < CHUNK_SIZE; ++b) {
                int x = 0;
                int y = 0;
                int z = 0;
                switch (direction) {
                    case DIR_POS_X: x = CHUNK_SIZE - 1; y = a; z = b; break;
                    case DIR_NEG_X: x = 0; y = a; z = b; break;
                    case DIR_POS_Y: x = a; y = CHUNK_SIZE - 1; z = b; break;
                    case DIR_NEG_Y: x = a; y = 0; z = b; break;
                    case DIR_POS_Z: x = a; y = b; z = CHUNK_SIZE - 1; break;
                    case DIR_NEG_Z: x = a; y = b; z = 0; break;
                    default: continue;
                }
                if (chunk->getLight(x, y, z) != 0) {
                    lightQueue.push_back({
                        chunk,
                        static_cast<int8_t>(x),
                        static_cast<int8_t>(y),
                        static_cast<int8_t>(z)
                    });
                }
            }
        }
    }

    void enqueueBoundaryLight(const std::shared_ptr<Chunk>& chunk) {
        for (int direction = 0; direction < 6; ++direction) {
            enqueueLightFace(chunk, direction);
        }
    }

    void propagateWorldLighting(std::unordered_set<Chunk*>& changedChunks) {
        const int dx[6] = { 1, -1,  0,  0,  0,  0 };
        const int dy[6] = { 0,  0,  1, -1,  0,  0 };
        const int dz[6] = { 0,  0,  0,  0,  1, -1 };

        while (!lightQueue.empty()) {
            LightNode node = std::move(lightQueue.front());
            lightQueue.pop_front();
            if (!node.chunk ||
                !node.chunk->resident.load(std::memory_order_acquire) ||
                !node.chunk->isGenerated.load(std::memory_order_acquire)) {
                continue;
            }

            uint16_t currentLight = node.chunk->getLight(node.x, node.y, node.z);
            uint8_t cr = getLightR(currentLight);
            uint8_t cg = getLightG(currentLight);
            uint8_t cb = getLightB(currentLight);
            uint8_t csky = getLightSky(currentLight);
            if (cr == 0 && cg == 0 && cb == 0 && csky == 0) continue;

            for (int direction = 0; direction < 6; ++direction) {
                int nx = static_cast<int>(node.x) + dx[direction];
                int ny = static_cast<int>(node.y) + dy[direction];
                int nz = static_cast<int>(node.z) + dz[direction];
                std::shared_ptr<Chunk> neighbor = node.chunk;

                if (nx < 0 || nx >= CHUNK_SIZE ||
                    ny < 0 || ny >= CHUNK_SIZE ||
                    nz < 0 || nz >= CHUNK_SIZE) {
                    IVec3 neighborPos = node.chunk->chunkPos;
                    if (nx < 0) {
                        --neighborPos.x;
                        nx = CHUNK_SIZE - 1;
                    } else if (nx >= CHUNK_SIZE) {
                        ++neighborPos.x;
                        nx = 0;
                    } else if (ny < 0) {
                        --neighborPos.y;
                        ny = CHUNK_SIZE - 1;
                    } else if (ny >= CHUNK_SIZE) {
                        ++neighborPos.y;
                        ny = 0;
                    } else if (nz < 0) {
                        --neighborPos.z;
                        nz = CHUNK_SIZE - 1;
                    } else {
                        ++neighborPos.z;
                        nz = 0;
                    }
                    auto it = chunks[node.chunk->lod].find(neighborPos);
                    if (it == chunks[node.chunk->lod].end()) continue;
                    neighbor = it->second;
                    if (!neighbor ||
                        !neighbor->resident.load(std::memory_order_acquire) ||
                        !neighbor->isGenerated.load(std::memory_order_acquire)) {
                        continue;
                    }
                }

                if (!getBlockInfo(neighbor->getBlock(nx, ny, nz)).isTransparent) {
                    continue;
                }

                uint16_t neighborLight = neighbor->getLight(nx, ny, nz);
                uint8_t nr = getLightR(neighborLight);
                uint8_t ng = getLightG(neighborLight);
                uint8_t nb = getLightB(neighborLight);
                uint8_t nsky = getLightSky(neighborLight);
                uint8_t tr = cr > 1 ? cr - 1 : 0;
                uint8_t tg = cg > 1 ? cg - 1 : 0;
                uint8_t tb = cb > 1 ? cb - 1 : 0;
                uint8_t tsky = csky > 1 ? csky - 1 : 0;
                bool updated = false;
                if (tr > nr) { nr = tr; updated = true; }
                if (tg > ng) { ng = tg; updated = true; }
                if (tb > nb) { nb = tb; updated = true; }
                if (tsky > nsky) { nsky = tsky; updated = true; }
                if (!updated) continue;

                neighbor->setLight(nx, ny, nz, packLight(nr, ng, nb, nsky));
                changedChunks.insert(neighbor.get());
                lightQueue.push_back({
                    neighbor,
                    static_cast<int8_t>(nx),
                    static_cast<int8_t>(ny),
                    static_cast<int8_t>(nz)
                });
            }
        }
    }

    bool enqueueMesh(
        const std::shared_ptr<Chunk>& chunk,
        std::shared_ptr<MeshingNeighborhood> neighborhood,
        const Vec3& cameraPos
    ) {
        if (!chunk || !neighborhood ||
            !chunk->resident.load(std::memory_order_acquire) ||
            !chunk->isGenerated.load(std::memory_order_acquire) ||
            !chunk->isLightReady.load(std::memory_order_acquire)) {
            return false;
        }
        bool expected = false;
        if (!chunk->isMeshQueued.compare_exchange_strong(expected, true)) {
            return false;
        }

        GenerationTask task{
            chunk,
            calculateTaskPriority(*chunk, cameraPos),
            queueSequence.fetch_add(1),
            chunk->workToken.fetch_add(1) + 1,
            WorkType::Mesh,
            std::move(neighborhood)
        };
        chunk->isPendingWork.store(true);
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (generateQueue.size() >= MAX_PENDING_GENERATION_TASKS) {
                cancelTaskIfCurrent(task);
                return false;
            }
            generateQueue.push(std::move(task));
        }
        cv.notify_one();
        return true;
    }

    void processLighting() {
        std::vector<LightingReadyChunk> ready;
        {
            std::lock_guard<std::mutex> lock(lightingMutex);
            ready.swap(lightingReadyQueue);
        }

        std::vector<std::shared_ptr<Chunk>> candidates;
        std::unordered_set<Chunk*> candidateSet;
        auto addCandidate = [&](const std::shared_ptr<Chunk>& chunk) {
            if (chunk && candidateSet.insert(chunk.get()).second) {
                candidates.push_back(chunk);
            }
        };

        for (LightingReadyChunk& item : ready) {
            const std::shared_ptr<Chunk>& chunk = item.chunk;
            if (!chunk ||
                !chunk->resident.load(std::memory_order_acquire) ||
                chunk->workToken.load(std::memory_order_acquire) != item.workToken) {
                continue;
            }
            pendingNeighborhoods[chunk.get()] = std::move(item.neighborhood);
            chunk->isLightReady.store(false);
            chunk->meshDirty.store(true);
            enqueueBoundaryLight(chunk);
            std::array<std::shared_ptr<Chunk>, 6> neighbors =
                getSameLodNeighbors(*chunk);
            for (int direction = 0; direction < 6; ++direction) {
                std::shared_ptr<Chunk>& neighbor = neighbors[direction];
                if (neighbor && neighbor->isGenerated.load(std::memory_order_acquire)) {
                    enqueueLightFace(neighbor, direction ^ 1);
                }
            }
            addCandidate(chunk);
        }

        std::unordered_set<Chunk*> changedChunks;
        propagateWorldLighting(changedChunks);
        for (Chunk* rawChunk : changedChunks) {
            if (!rawChunk || !rawChunk->resident.load(std::memory_order_acquire)) continue;
            rawChunk->meshDirty.store(true);
            auto it = chunks[rawChunk->lod].find(rawChunk->chunkPos);
            if (it != chunks[rawChunk->lod].end() && it->second.get() == rawChunk) {
                addCandidate(it->second);
            }
        }

        for (const auto& entry : pendingNeighborhoods) {
            if (!entry.first) continue;
            auto it = chunks[entry.first->lod].find(entry.first->chunkPos);
            if (it != chunks[entry.first->lod].end() &&
                it->second.get() == entry.first) {
                addCandidate(it->second);
            }
        }
        // A neighbor can dirty a chunk while its previous mesh is already
        // staged. Keep the rebuild request alive until that upload completes.
        for (int lod = 0; lod < NUM_LODS; ++lod) {
            for (const auto& pair : chunks[lod]) {
                if (pair.second &&
                    pair.second->meshDirty.load(std::memory_order_acquire) &&
                    pair.second->isLightReady.load(std::memory_order_acquire)) {
                    addCandidate(pair.second);
                }
            }
        }

        for (const std::shared_ptr<Chunk>& chunk : candidates) {
            if (!chunk ||
                !chunk->resident.load(std::memory_order_acquire) ||
                !chunk->isGenerated.load(std::memory_order_acquire)) {
                continue;
            }
            chunk->isLightReady.store(true);
            if (!chunk->meshDirty.load() ||
                chunk->isMeshQueued.load() ||
                chunk->isMeshStaged.load()) {
                continue;
            }

            auto pending = pendingNeighborhoods.find(chunk.get());
            if (pending == pendingNeighborhoods.end()) {
                pending = pendingNeighborhoods.emplace(
                    chunk.get(),
                    std::make_shared<MeshingNeighborhood>()
                ).first;
            }
            std::array<std::shared_ptr<Chunk>, 6> neighbors =
                getSameLodNeighbors(*chunk);
            MeshBuilder::updateNeighborhood(
                *chunk,
                *pending->second,
                neighbors
            );
            if (enqueueMesh(chunk, pending->second, currentCamPos)) {
                chunk->meshDirty.store(false);
                pendingNeighborhoods.erase(pending);
            }
        }
    }


    void workerThreadFunc() {
        while (!stopThreads) {
            GenerationTask task{
                nullptr,
                0.0f,
                0,
                0,
                WorkType::Generate,
                nullptr
            };
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [this]() { return stopThreads || !generateQueue.empty(); });

                if (stopThreads) break;

                task = generateQueue.top();
                generateQueue.pop();
            }

            std::shared_ptr<Chunk> chunk = std::move(task.chunk);
            if (!chunk) continue;

            if (!chunk->resident.load(std::memory_order_acquire) ||
                chunk->workToken.load(std::memory_order_acquire) != task.workToken) {
                cancelTaskIfCurrent(task);
                continue;
            }

            Vec3 cameraSnapshot;
            {
                std::lock_guard<std::mutex> cameraLock(cameraMutex);
                cameraSnapshot = currentCamPos;
            }
            if (isChunkOutsideGenerationWindow(chunk.get(), cameraSnapshot)) {
                cancelTaskIfCurrent(task);
                continue;
            }

            if (task.type == WorkType::Generate) {
                if (chunk->isGenerated.load(std::memory_order_acquire)) {
                    if (chunk->workToken.load() == task.workToken) {
                        chunk->isPendingWork.store(false);
                    }
                    continue;
                }

                auto t0 = std::chrono::high_resolution_clock::now();
                auto neighborhood = std::make_shared<MeshingNeighborhood>();
                MeshBuilder::generateVoxelData(*chunk, neighborhood.get());
                auto t1 = std::chrono::high_resolution_clock::now();
                totalGenTimeUs += std::chrono::duration_cast<std::chrono::microseconds>(
                    t1 - t0
                ).count();
                chunksProcessed++;

                bool taskValid = chunk->resident.load(std::memory_order_acquire) &&
                    chunk->workToken.load(std::memory_order_acquire) == task.workToken;
                if (taskValid) {
                    std::lock_guard<std::mutex> lock(lightingMutex);
                    lightingReadyQueue.push_back({
                        chunk,
                        std::move(neighborhood),
                        task.workToken
                    });
                } else {
                    chunk->isGenerated.store(false);
                    chunk->isLightReady.store(false);
                }
            } else {
                if (!task.neighborhood ||
                    !chunk->isLightReady.load(std::memory_order_acquire)) {
                    cancelTaskIfCurrent(task);
                    continue;
                }

                auto t0 = std::chrono::high_resolution_clock::now();
                while (!stopThreads &&
                       stagedMeshBytes.load(std::memory_order_relaxed) >= MAX_STAGED_MESH_BYTES) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                MeshBuilder::buildMesh(*chunk, task.neighborhood.get());
                auto t1 = std::chrono::high_resolution_clock::now();
                totalMeshTimeUs += std::chrono::duration_cast<std::chrono::microseconds>(
                    t1 - t0
                ).count();

                bool taskValid = chunk->resident.load(std::memory_order_acquire) &&
                    chunk->workToken.load(std::memory_order_acquire) == task.workToken &&
                    chunk->isMeshStaged.load(std::memory_order_acquire);
                if (taskValid) {
                    size_t bytes = chunk->stagedVertices.size() * sizeof(VoxelVertex) +
                                   chunk->stagedIndices.size() * sizeof(uint32_t);
                    stagedMeshBytes.fetch_add(bytes, std::memory_order_relaxed);
                    std::lock_guard<std::mutex> lock(stagedMutex);
                    stagedMeshQueue.push_back(chunk);
                } else {
                    chunk->stagedVertices.clear();
                    chunk->stagedIndices.clear();
                    chunk->isMeshStaged.store(false);
                }

                if (chunk->workToken.load() == task.workToken) {
                    chunk->isMeshQueued.store(false);
                }
            }

            if (chunk->workToken.load() == task.workToken) {
                chunk->isPendingWork.store(false);
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
            auto& map = chunks[chunk->lod];
            auto it = map.find(chunk->chunkPos);
            bool stillOwned = (it != map.end() && it->second.get() == chunk.get() && chunk->resident.load(std::memory_order_acquire));
            if (!stillOwned || !chunk->isMeshStaged.load()) {
                size_t bytes = chunk->stagedVertices.size() * sizeof(VoxelVertex) +
                               chunk->stagedIndices.size() * sizeof(uint32_t);
                size_t cur = stagedMeshBytes.load(std::memory_order_relaxed);
                stagedMeshBytes.store(cur > bytes ? cur - bytes : 0, std::memory_order_relaxed);
                chunk->stagedVertices.clear();
                chunk->stagedIndices.clear();
                chunk->isMeshStaged.store(false);
                continue;
            }
            size_t meshBytes =
                chunk->stagedVertices.size() * sizeof(VoxelVertex) +
                chunk->stagedIndices.size() * sizeof(uint32_t);
            if (uploadedBytesThisFrame > 0 &&
                uploadedBytesThisFrame + meshBytes > MAX_GPU_UPLOAD_BYTES_PER_FRAME) {
                std::lock_guard<std::mutex> lock(stagedMutex);
                stagedMeshQueue.push_back(chunk);
                continue;
            }
            size_t cur = stagedMeshBytes.load(std::memory_order_relaxed);
            stagedMeshBytes.store(cur > meshBytes ? cur - meshBytes : 0, std::memory_order_relaxed);
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
            trimGenerationQueue(cameraPos);
            for (int lod = 0; lod < NUM_LODS; ++lod) {
                for (auto it = chunks[lod].begin(); it != chunks[lod].end(); ) {
                    Chunk* chunk = it->second.get();
                    if (isChunkOutOfRange(chunk, cameraPos)) {
                        chunk->resident.store(false, std::memory_order_release);
                        chunk->workToken.fetch_add(1, std::memory_order_acq_rel);
                        pendingNeighborhoods.erase(chunk);
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

        // queue coarse fallback roots first, then request nearby fine levels.
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

                        if (lod < NUM_LODS - 1) {
                            float centerX = static_cast<float>(cx * worldChunkSize) +
                                static_cast<float>(worldChunkSize) * 0.5f;
                            float centerY = static_cast<float>(cy * worldChunkSize) +
                                static_cast<float>(worldChunkSize) * 0.5f;
                            float centerZ = static_cast<float>(cz * worldChunkSize) +
                                static_cast<float>(worldChunkSize) * 0.5f;
                            float dx = centerX - cameraPos.x;
                            float dy = centerY - cameraPos.y;
                            float dz = centerZ - cameraPos.z;
                            float maxDistance = LOD_MAX_DISTANCE[lod];
                            if (dx * dx + dy * dy + dz * dz >
                                maxDistance * maxDistance) {
                                continue;
                            }
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
        processLighting();

    }

    bool render(
        const Frustum& frustum,
        Vec3 cameraPos,
        float verticalFovRadians,
        int viewportWidth,
        int viewportHeight,
        GLuint voxelShaderProgram,
        float projectionY,
        const Mat4& view,
        const Mat4& viewProjection
    ) {
        if (diagnosticsEnabled) {
            lastDiagnostics = FrameDiagnostics{};
            lastDiagnostics.frameIndex = diagnosticsFrameCounter++;
        }
        if (!geometryArena.isInitialized()) {
            std::cerr << "Geometry arena is not initialized.\n";
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
        std::sort(
            selectedChunks.begin(),
            selectedChunks.end(),
            [cameraPos](const Chunk* left, const Chunk* right) {
                auto distanceSquared = [cameraPos](const Chunk* chunk) {
                    float centerX = static_cast<float>(chunk->worldMin.x) +
                        static_cast<float>(chunk->worldSize) * 0.5f;
                    float centerY = static_cast<float>(chunk->worldMin.y) +
                        static_cast<float>(chunk->worldSize) * 0.5f;
                    float centerZ = static_cast<float>(chunk->worldMin.z) +
                        static_cast<float>(chunk->worldSize) * 0.5f;
                    float dx = centerX - cameraPos.x;
                    float dy = centerY - cameraPos.y;
                    float dz = centerZ - cameraPos.z;
                    return dx * dx + dy * dy + dz * dz;
                };
                return distanceSquared(left) < distanceSquared(right);
            }
        );
        lastSelectedChunks = selectedChunks;

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
            lastDiagnostics.cpuTraversalUsed = true;
            lastDiagnostics.commandPayloadValid = true;
        }

        geometryArena.uploadDrawData(drawMetadata, drawCommands);
        glUseProgram(voxelShaderProgram);
        geometryArena.drawIndirect(drawCommands.size());
        return true;
    }
    void renderChunkBorders(ChunkBorderRenderer& borderRenderer, const Vec3& cameraPos, const Mat4& projection, const Mat4& view) {
        borderRenderer.clear();
        std::unordered_set<const Chunk*> processed;

        for (const Chunk* chunk : lastSelectedChunks) {
            if (chunk && processed.insert(chunk).second) {
                borderRenderer.addChunkBorders(chunk, cameraPos);
            }
        }
        for (const auto& pair : chunks[0]) {
            if (pair.second && pair.second->isMeshUploaded.load() && processed.insert(pair.second.get()).second) {
                borderRenderer.addChunkBorders(pair.second.get(), cameraPos);
            }
        }

        borderRenderer.render(projection, view);
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
    // Helper for collision checking against solid voxels in world space:
    // reads generated LOD-0 chunk blocks first, falling back to procedural gen.
    bool isBlockSolidAt(int64_t wx, int64_t wy, int64_t wz) {
        int64_t cx = floorDiv(wx, CHUNK_SIZE);
        int64_t cy = floorDiv(wy, CHUNK_SIZE);
        int64_t cz = floorDiv(wz, CHUNK_SIZE);
        auto it = chunks[0].find(IVec3(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(cz)));
        if (it != chunks[0].end() && it->second && it->second->isGenerated.load(std::memory_order_acquire)) {
            int lx = static_cast<int>(wx - cx * CHUNK_SIZE);
            int ly = static_cast<int>(wy - cy * CHUNK_SIZE);
            int lz = static_cast<int>(wz - cz * CHUNK_SIZE);
            uint8_t b = it->second->getBlock(lx, ly, lz);
            return getBlockInfo(b).isSolid;
        }
        return getBlockInfo(WorldGen::getBlockAt(wx, wy, wz)).isSolid;
    }

    // Boundary source test at local X=31
    bool runBoundarySourceTest() {
        auto chunkA = std::make_shared<Chunk>(IVec3(0, 0, 0), 0);
        auto chunkB = std::make_shared<Chunk>(IVec3(1, 0, 0), 0);
        chunkA->isEmpty = false;
        chunkB->isEmpty = false;
        chunkA->isGenerated.store(true, std::memory_order_release);
        chunkB->isGenerated.store(true, std::memory_order_release);
        chunkA->resident.store(true, std::memory_order_release);
        chunkB->resident.store(true, std::memory_order_release);

        chunks[0][IVec3(0, 0, 0)] = chunkA;
        chunks[0][IVec3(1, 0, 0)] = chunkB;

        // Boundary source light at local X=31
        uint16_t srcLight = packLight(15, 15, 15, 15);
        chunkA->setLight(31, 16, 16, srcLight);

        enqueueLightFace(chunkA, DIR_POS_X);

        std::unordered_set<Chunk*> changedChunks;
        propagateWorldLighting(changedChunks);

        uint16_t bLight = chunkB->getLight(0, 16, 16);
        uint8_t skyB = getLightSky(bLight);
        uint8_t rB = getLightR(bLight);
        bool pass = (skyB >= 14) && changedChunks.count(chunkB.get());

        std::cout << "\n=== BOUNDARY SOURCE TEST AT LOCAL X=31 ===\n"
                  << "Boundary Source (Chunk 0,0,0 at X=31): Light RGB/Sky=" << static_cast<int>(getLightSky(srcLight)) << "\n"
                  << "Propagated Target (Chunk 1,0,0 at X=0): Sky Light=" << static_cast<int>(skyB)
                  << ", Red Light=" << static_cast<int>(rB) << "\n"
                  << "Boundary Test Result: " << (pass ? "PASS" : "FAIL") << "\n"
                  << "===========================================\n\n";

        // Cleanup test chunks
        chunks[0].erase(IVec3(0, 0, 0));
        chunks[0].erase(IVec3(1, 0, 0));
        return pass;
    }
};

#endif // CHUNK_MANAGER_HPP
