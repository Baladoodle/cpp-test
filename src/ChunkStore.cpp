#include "ChunkStore.hpp"
#include <cmath>

std::shared_ptr<Chunk> ChunkStore::getChunk(int lod, const IVec3& pos) const {
    if (lod < 0 || lod >= NUM_LODS) return nullptr;
    std::lock_guard<std::mutex> lock(storeMutex);
    auto it = chunks[lod].find(pos);
    if (it != chunks[lod].end()) return it->second;
    return nullptr;
}

void ChunkStore::insertChunk(int lod, const IVec3& pos, std::shared_ptr<Chunk> chunk) {
    if (lod < 0 || lod >= NUM_LODS) return;
    std::lock_guard<std::mutex> lock(storeMutex);
    chunks[lod][pos] = std::move(chunk);
}

bool ChunkStore::removeChunk(int lod, const IVec3& pos) {
    if (lod < 0 || lod >= NUM_LODS) return false;
    std::lock_guard<std::mutex> lock(storeMutex);
    return chunks[lod].erase(pos) > 0;
}

bool ChunkStore::hasChunk(int lod, const IVec3& pos) const {
    if (lod < 0 || lod >= NUM_LODS) return false;
    std::lock_guard<std::mutex> lock(storeMutex);
    return chunks[lod].find(pos) != chunks[lod].end();
}

std::vector<std::shared_ptr<Chunk>> ChunkStore::getAllChunks(int lod) const {
    std::vector<std::shared_ptr<Chunk>> result;
    if (lod < 0 || lod >= NUM_LODS) return result;
    std::lock_guard<std::mutex> lock(storeMutex);
    result.reserve(chunks[lod].size());
    for (const auto& pair : chunks[lod]) {
        result.push_back(pair.second);
    }
    return result;
}

void ChunkStore::clear() {
    std::lock_guard<std::mutex> lock(storeMutex);
    for (int lod = 0; lod < NUM_LODS; ++lod) {
        chunks[lod].clear();
    }
}

bool ChunkStore::isBlockSolidAt(int64_t wx, int64_t wy, int64_t wz) const {
    int64_t cx = floorDiv(wx, CHUNK_SIZE);
    int64_t cy = floorDiv(wy, CHUNK_SIZE);
    int64_t cz = floorDiv(wz, CHUNK_SIZE);
    std::shared_ptr<Chunk> chunk = getChunk(0, IVec3(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(cz)));
    if (chunk && chunk->isAtLeast(ChunkState::LightPending)) {
        int lx = static_cast<int>(wx - cx * CHUNK_SIZE);
        int ly = static_cast<int>(wy - cy * CHUNK_SIZE);
        int lz = static_cast<int>(wz - cz * CHUNK_SIZE);
        uint8_t block = chunk->getBlock(lx, ly, lz);
        const BlockInfo& info = getBlockInfo(block);
        return info.isSolid && !info.isTransparent;
    }
    float density = WorldGenerator::getDensity(wx, wy, wz, 1);
    if (density <= 0.0f) return false;
    uint8_t block = WorldGenerator::getBlockAt(wx, wy, wz, 1);
    const BlockInfo& info = getBlockInfo(block);
    return info.isSolid && !info.isTransparent;
}

BlockType ChunkStore::getBlockAt(const IVec3& worldPos) const {
    int64_t wx = worldPos.x;
    int64_t wy = worldPos.y;
    int64_t wz = worldPos.z;
    int64_t cx = floorDiv(wx, CHUNK_SIZE);
    int64_t cy = floorDiv(wy, CHUNK_SIZE);
    int64_t cz = floorDiv(wz, CHUNK_SIZE);
    std::shared_ptr<Chunk> chunk = getChunk(0, IVec3(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(cz)));
    if (chunk && chunk->isAtLeast(ChunkState::LightPending)) {
        int lx = static_cast<int>(wx - cx * CHUNK_SIZE);
        int ly = static_cast<int>(wy - cy * CHUNK_SIZE);
        int lz = static_cast<int>(wz - cz * CHUNK_SIZE);
        return static_cast<BlockType>(chunk->getBlock(lx, ly, lz));
    }
    return static_cast<BlockType>(WorldGenerator::getBlockAt(wx, wy, wz, 1));
}
