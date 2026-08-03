#ifndef VOXEL_MIP_HPP
#define VOXEL_MIP_HPP

#include "Chunk.hpp"
#include <array>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

// CPU-side storage for the coarse voxel pyramid. Each entry is one 32^3
// section, matching Chunk's addressing at every LOD. Fine sections publish
// into their parent; a parent becomes readable only after all eight children
// have contributed their 16^3 half of the parent section.
class VoxelMipStore {
public:
    static constexpr int NUM_LEVELS = 5;

    struct CompletedSection {
        int lod = 0;
        IVec3 chunkPos;
        uint64_t revision = 0;
    };

private:
    struct MipSection {
        std::array<uint8_t, CHUNK_VOL> blocks{};
        uint8_t childMask = 0;
        uint64_t revision = 0;
    };

    std::unordered_map<IVec3, MipSection, IVec3Hash> levels[NUM_LEVELS];
    std::vector<CompletedSection> completed;
    mutable std::mutex mutex;

    static int64_t floorDiv(int64_t value, int64_t divisor) {
        int64_t result = value / divisor;
        int64_t remainder = value % divisor;
        if (remainder != 0 && ((value < 0) ^ (divisor < 0))) {
            --result;
        }
        return result;
    }

    static uint8_t chooseRepresentative(const uint8_t* childBlocks, int x, int y, int z) {
        int counts[BLOCK_COUNT] = {};
        int totalSolid = 0;
        for (int dz = 0; dz < 2; ++dz) {
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    int index = getVoxelIndex(x * 2 + dx, y * 2 + dy, z * 2 + dz);
                    uint8_t block = childBlocks[index];
                    if (block < BLOCK_COUNT) {
                        ++counts[block];
                        if (getBlockInfo(block).isSolid) ++totalSolid;
                    }
                }
            }
        }

        uint8_t best = BLOCK_AIR;
        if (totalSolid >= 2) {
            for (uint8_t block = 1; block < BLOCK_COUNT; ++block) {
                if (getBlockInfo(block).isSolid && counts[block] > counts[best]) {
                    best = block;
                }
            }
        }
        if (best == BLOCK_AIR) {
            for (uint8_t block = 1; block < BLOCK_COUNT; ++block) {
                if (counts[block] > counts[best]) {
                    best = block;
                }
            }
        }
        return best;
    }

    static uint8_t childBit(const IVec3& childPos, const IVec3& parentPos) {
        int x = static_cast<int>(childPos.x - parentPos.x * 2);
        int y = static_cast<int>(childPos.y - parentPos.y * 2);
        int z = static_cast<int>(childPos.z - parentPos.z * 2);
        return static_cast<uint8_t>(x | (y << 1) | (z << 2));
    }

    void queueCompletedUnlocked(int lod, const IVec3& pos, uint64_t revision) {
        for (auto& item : completed) {
            if (item.lod == lod && item.chunkPos == pos) {
                item.revision = std::max(item.revision, revision);
                return;
            }
        }
        completed.push_back({lod, pos, revision});
    }

    void publishParentUnlocked(int lod, const IVec3& childPos, const uint8_t* childBlocks) {
        if (lod >= NUM_LEVELS - 1) return;

        IVec3 parentPos(
            floorDiv(childPos.x, 2),
            floorDiv(childPos.y, 2),
            floorDiv(childPos.z, 2)
        );
        MipSection& parent = levels[lod + 1][parentPos];
        uint8_t bit = static_cast<uint8_t>(1u << childBit(childPos, parentPos));
        bool wasComplete = parent.childMask == 0xFF;
        if (parent.childMask == 0) {
            parent.blocks.fill(BLOCK_AIR);
        }

        bool contentChanged = false;
        for (int z = 0; z < CHUNK_SIZE / 2; ++z) {
            for (int y = 0; y < CHUNK_SIZE / 2; ++y) {
                for (int x = 0; x < CHUNK_SIZE / 2; ++x) {
                    uint8_t block = chooseRepresentative(childBlocks, x, y, z);
                    int parentX = static_cast<int>((childPos.x - parentPos.x * 2) * (CHUNK_SIZE / 2)) + x;
                    int parentY = static_cast<int>((childPos.y - parentPos.y * 2) * (CHUNK_SIZE / 2)) + y;
                    int parentZ = static_cast<int>((childPos.z - parentPos.z * 2) * (CHUNK_SIZE / 2)) + z;
                    uint8_t& dest = parent.blocks[getVoxelIndex(parentX, parentY, parentZ)];
                    if (dest != block) {
                        dest = block;
                        contentChanged = true;
                    }
                }
            }
        }

        parent.childMask = static_cast<uint8_t>(parent.childMask | bit);
        bool isComplete = parent.childMask == 0xFF;
        bool becameComplete = isComplete && !wasComplete;
        if (isComplete && (becameComplete || contentChanged)) {
            ++parent.revision;
            queueCompletedUnlocked(lod + 1, parentPos, parent.revision);
            std::array<uint8_t, CHUNK_VOL> completedBlocks = parent.blocks;
            publishParentUnlocked(lod + 1, parentPos, completedBlocks.data());
        }
    }

public:
    void publishSection(int lod, const IVec3& chunkPos, const uint8_t* blocks) {
        if (lod < 0 || lod >= NUM_LEVELS || !blocks) return;
        std::lock_guard<std::mutex> lock(mutex);
        publishParentUnlocked(lod, chunkPos, blocks);
    }

    bool readCompleteSection(
        int lod,
        const IVec3& chunkPos,
        uint8_t* blocks,
        uint64_t* revision = nullptr
    ) const {
        if (lod <= 0 || lod >= NUM_LEVELS || !blocks) return false;
        std::lock_guard<std::mutex> lock(mutex);
        auto it = levels[lod].find(chunkPos);
        if (it == levels[lod].end() || it->second.childMask != 0xFF) return false;
        std::memcpy(blocks, it->second.blocks.data(), CHUNK_VOL * sizeof(uint8_t));
        if (revision) *revision = it->second.revision;
        return true;
    }
    uint8_t readVoxel(int lod, const IVec3& chunkPos, int localX, int localY, int localZ) const {
        if (lod < 0 || lod >= NUM_LEVELS) return BLOCK_AIR;

        int64_t cx = chunkPos.x;
        int64_t cy = chunkPos.y;
        int64_t cz = chunkPos.z;

        if (localX < 0) { cx -= 1; localX += CHUNK_SIZE; }
        else if (localX >= CHUNK_SIZE) { cx += 1; localX -= CHUNK_SIZE; }

        if (localY < 0) { cy -= 1; localY += CHUNK_SIZE; }
        else if (localY >= CHUNK_SIZE) { cy += 1; localY -= CHUNK_SIZE; }

        if (localZ < 0) { cz -= 1; localZ += CHUNK_SIZE; }
        else if (localZ >= CHUNK_SIZE) { cz += 1; localZ -= CHUNK_SIZE; }

        std::lock_guard<std::mutex> lock(mutex);
        IVec3 targetPos(cx, cy, cz);
        auto it = levels[lod].find(targetPos);
        if (it != levels[lod].end()) {
            return it->second.blocks[getVoxelIndex(localX, localY, localZ)];
        }
        return BLOCK_AIR;
    }

    std::vector<CompletedSection> drainCompleted() {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<CompletedSection> result;
        result.swap(completed);
        return result;
    }

    void requeueCompleted(const CompletedSection& section) {
        std::lock_guard<std::mutex> lock(mutex);
        queueCompletedUnlocked(section.lod, section.chunkPos, section.revision);
    }

    void prune(const Vec3& cameraPos) {
        constexpr size_t MAX_SECTIONS_PER_LEVEL = 1024;
        std::lock_guard<std::mutex> lock(mutex);
        for (int lod = 1; lod < NUM_LEVELS; ++lod) {
            if (levels[lod].size() <= MAX_SECTIONS_PER_LEVEL) continue;

            struct DistEntry {
                IVec3 pos;
                float distSq;
            };
            std::vector<DistEntry> entries;
            entries.reserve(levels[lod].size());

            float sectionSize = static_cast<float>(CHUNK_SIZE * (1 << lod));
            for (auto it = levels[lod].begin(); it != levels[lod].end(); ++it) {
                float centerX = static_cast<float>(it->first.x) * sectionSize + sectionSize * 0.5f;
                float centerY = static_cast<float>(it->first.y) * sectionSize + sectionSize * 0.5f;
                float centerZ = static_cast<float>(it->first.z) * sectionSize + sectionSize * 0.5f;
                float dx = centerX - cameraPos.x;
                float dy = centerY - cameraPos.y;
                float dz = centerZ - cameraPos.z;
                entries.push_back({it->first, dx * dx + dy * dy + dz * dz});
            }

            size_t removeCount = levels[lod].size() - MAX_SECTIONS_PER_LEVEL;
            std::nth_element(
                entries.begin(),
                entries.begin() + removeCount,
                entries.end(),
                [](const DistEntry& a, const DistEntry& b) { return a.distSq > b.distSq; }
            );

            for (size_t i = 0; i < removeCount; ++i) {
                levels[lod].erase(entries[i].pos);
            }
        }
    }
};

#endif // VOXEL_MIP_HPP
