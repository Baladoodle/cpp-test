#ifndef CHUNK_STORE_HPP
#define CHUNK_STORE_HPP

#include "Chunk.hpp"
#include "IWorldQuery.hpp"
#include "WorldGenerator.hpp"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

constexpr int NUM_LODS = 7;

class ChunkStore : public IWorldQuery {
private:
    std::unordered_map<IVec3, std::shared_ptr<Chunk>, IVec3Hash> chunks[NUM_LODS];
    mutable std::mutex storeMutex;

public:
    ChunkStore() = default;
    ~ChunkStore() override = default;

    std::shared_ptr<Chunk> getChunk(int lod, const IVec3& pos) const;
    void insertChunk(int lod, const IVec3& pos, std::shared_ptr<Chunk> chunk);
    bool removeChunk(int lod, const IVec3& pos);
    bool hasChunk(int lod, const IVec3& pos) const;

    std::vector<std::shared_ptr<Chunk>> getAllChunks(int lod) const;
    void clear();

    bool isBlockSolidAt(int64_t wx, int64_t wy, int64_t wz) const override;
    BlockType getBlockAt(const IVec3& worldPos) const override;

    std::unordered_map<IVec3, std::shared_ptr<Chunk>, IVec3Hash>& getLODMap(int lod) {
        return chunks[lod];
    }
    const std::unordered_map<IVec3, std::shared_ptr<Chunk>, IVec3Hash>& getLODMap(int lod) const {
        return chunks[lod];
    }
};

#endif // CHUNK_STORE_HPP
