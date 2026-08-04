#include "LightingSystem.hpp"
#include "Block.hpp"
#include <algorithm>

struct LocalLightNode {
    int8_t x, y, z;
};

void LightingSystem::propagateLocalLight3D(Chunk& chunk) {
    if (chunk.isEmpty) return;

    std::vector<uint16_t> paddedLight(PADDED_VOL, 0);
    std::vector<LocalLightNode> lightQueue;
    lightQueue.reserve(4096);

    auto getBlock = [&](int x, int y, int z) -> uint8_t {
        return chunk.getBlock(x, y, z);
    };

    auto setPaddedLight = [&](int x, int y, int z, uint16_t l) {
        if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) return;
        paddedLight[getPaddedVoxelIndex(x, y, z)] = l;
    };

    auto getPaddedLight = [&](int x, int y, int z) -> uint16_t {
        if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) return 0;
        return paddedLight[getPaddedVoxelIndex(x, y, z)];
    };

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            uint8_t skyVal = 15;
            for (int y = CHUNK_SIZE - 1; y >= 0; --y) {
                uint8_t block = getBlock(x, y, z);
                const BlockInfo& info = getBlockInfo(block);
                if (!info.isTransparent) skyVal = 0;

                uint8_t r = info.lightR;
                uint8_t g = info.lightG;
                uint8_t b = info.lightB;
                if (r > 0 || g > 0 || b > 0 || skyVal > 0) {
                    setPaddedLight(x, y, z, packLight(r, g, b, skyVal));
                    lightQueue.push_back({
                        static_cast<int8_t>(x),
                        static_cast<int8_t>(y),
                        static_cast<int8_t>(z)
                    });
                }
            }
        }
    }

    const int dx[6] = { 1, -1,  0,  0,  0,  0 };
    const int dy[6] = { 0,  0,  1, -1,  0,  0 };
    const int dz[6] = { 0,  0,  0,  0,  1, -1 };

    size_t head = 0;
    while (head < lightQueue.size()) {
        LocalLightNode curr = lightQueue[head++];
        uint16_t currLight = getPaddedLight(curr.x, curr.y, curr.z);
        uint8_t cr = getLightR(currLight);
        uint8_t cg = getLightG(currLight);
        uint8_t cb = getLightB(currLight);
        uint8_t csky = getLightSky(currLight);

        for (int i = 0; i < 6; ++i) {
            int nx = curr.x + dx[i];
            int ny = curr.y + dy[i];
            int nz = curr.z + dz[i];
            if (nx < 0 || nx >= CHUNK_SIZE ||
                ny < 0 || ny >= CHUNK_SIZE ||
                nz < 0 || nz >= CHUNK_SIZE) {
                continue;
            }

            uint8_t nBlock = getBlock(nx, ny, nz);
            if (!getBlockInfo(nBlock).isTransparent) continue;

            uint16_t nLight = getPaddedLight(nx, ny, nz);
            uint8_t nr = getLightR(nLight);
            uint8_t ng = getLightG(nLight);
            uint8_t nb = getLightB(nLight);
            uint8_t nsky = getLightSky(nLight);

            uint8_t tr = (cr > 1) ? cr - 1 : 0;
            uint8_t tg = (cg > 1) ? cg - 1 : 0;
            uint8_t tb = (cb > 1) ? cb - 1 : 0;
            uint8_t tsky = (csky > 1) ? csky - 1 : 0;

            bool updated = false;
            if (tr > nr) { nr = tr; updated = true; }
            if (tg > ng) { ng = tg; updated = true; }
            if (tb > nb) { nb = tb; updated = true; }
            if (tsky > nsky) { nsky = tsky; updated = true; }

            if (updated) {
                setPaddedLight(nx, ny, nz, packLight(nr, ng, nb, nsky));
                lightQueue.push_back({
                    static_cast<int8_t>(nx),
                    static_cast<int8_t>(ny),
                    static_cast<int8_t>(nz)
                });
            }
        }
    }

    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                chunk.setLight(x, y, z, getPaddedLight(x, y, z));
            }
        }
    }
}

void LightingSystem::enqueueLightFace(
    const std::shared_ptr<Chunk>& chunk,
    int direction,
    std::deque<WorldLightNode>& lightQueue
) {
    if (!chunk || !chunk->isGenerated.load(std::memory_order_acquire)) return;
    for (int a = 0; a < CHUNK_SIZE; ++a) {
        for (int b = 0; b < CHUNK_SIZE; ++b) {
            int x = 0, y = 0, z = 0;
            switch (direction) {
                case 0: x = CHUNK_SIZE - 1; y = a; z = b; break; // +X
                case 1: x = 0;              y = a; z = b; break; // -X
                case 2: x = a; y = CHUNK_SIZE - 1; z = b; break; // +Y
                case 3: x = a; y = 0;              z = b; break; // -Y
                case 4: x = a; y = b; z = CHUNK_SIZE - 1; break; // +Z
                case 5: x = a; y = b; z = 0;              break; // -Z
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

void LightingSystem::enqueueBoundaryLight(
    const std::shared_ptr<Chunk>& chunk,
    std::deque<WorldLightNode>& lightQueue
) {
    for (int direction = 0; direction < 6; ++direction) {
        enqueueLightFace(chunk, direction, lightQueue);
    }
}

void LightingSystem::propagateWorldLighting(
    std::deque<WorldLightNode>& lightQueue,
    std::unordered_set<Chunk*>& changedChunks,
    size_t maxNodesPerFrame,
    const std::function<std::shared_ptr<Chunk>(const Chunk&, int direction)>& getNeighborFunc
) {
    const int dx[6] = { 1, -1,  0,  0,  0,  0 };
    const int dy[6] = { 0,  0,  1, -1,  0,  0 };
    const int dz[6] = { 0,  0,  0,  0,  1, -1 };

    size_t processedNodes = 0;
    while (!lightQueue.empty() && processedNodes < maxNodesPerFrame) {
        ++processedNodes;
        WorldLightNode node = std::move(lightQueue.front());
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
            int nx = node.x + dx[direction];
            int ny = node.y + dy[direction];
            int nz = node.z + dz[direction];
            std::shared_ptr<Chunk> neighbor = node.chunk;

            if (nx < 0 || nx >= CHUNK_SIZE ||
                ny < 0 || ny >= CHUNK_SIZE ||
                nz < 0 || nz >= CHUNK_SIZE) {
                neighbor = getNeighborFunc(*node.chunk, direction);
                if (!neighbor ||
                    !neighbor->resident.load(std::memory_order_acquire) ||
                    !neighbor->isGenerated.load(std::memory_order_acquire)) {
                    continue;
                }
                nx = (nx + CHUNK_SIZE) % CHUNK_SIZE;
                ny = (ny + CHUNK_SIZE) % CHUNK_SIZE;
                nz = (nz + CHUNK_SIZE) % CHUNK_SIZE;
            }

            uint8_t nBlock = neighbor->getBlock(nx, ny, nz);
            if (!getBlockInfo(nBlock).isTransparent) continue;

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

            if (updated) {
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
}