#include "Mesher.hpp"
#include "Block.hpp"
#include <algorithm>

float Mesher::calculateAO(bool side1, bool side2, bool corner) {
    if (side1 && side2) return 0.25f;
    int count = (side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0);
    return 1.0f - (count * 0.22f);
}

ChunkBuildOutput Mesher::buildMesh(Chunk& chunk, const MeshingNeighborhood* neighborhood) {
    chunk.stagedIndices.clear();
    chunk.stagedVertices.reserve(8192);
    chunk.stagedIndices.reserve(12288);
    if (chunk.isEmpty) {
        chunk.isMeshStaged = true;
        return { chunk.stagedVertices, chunk.stagedIndices };
    }

    int scale = chunk.scale;
    float fScale = static_cast<float>(scale);
    int64_t wmx = chunk.worldMin.x;
    int64_t wmy = chunk.worldMin.y;
    int64_t wmz = chunk.worldMin.z;

    auto getNeighborhoodBlock = [&](int x, int y, int z) -> uint8_t {
        return neighborhood
            ? neighborhood->block(x, y, z)
            : chunk.getPaddedBlock(x, y, z);
    };
    auto getNeighborhoodLight = [&](int x, int y, int z) -> uint16_t {
        return neighborhood
            ? neighborhood->lightAt(x, y, z)
            : chunk.getPaddedLight(x, y, z);
    };
    auto isSolidBlock = [&](int x, int y, int z) -> bool {
        const BlockInfo& info = getBlockInfo(getNeighborhoodBlock(x, y, z));
        return info.isSolid && !info.isTransparent;
    };
    auto getVertexAO = [&](int face, const Vec3& cornerPos, int nx, int ny, int nz) -> float {
        int dx = (cornerPos.x > 0.5f) ? 1 : -1;
        int dy = (cornerPos.y > 0.5f) ? 1 : -1;
        int dz = (cornerPos.z > 0.5f) ? 1 : -1;

        bool s1 = false, s2 = false, corner = false;
        if (face == DIR_POS_X || face == DIR_NEG_X) {
            s1 = isSolidBlock(nx, ny + dy, nz);
            s2 = isSolidBlock(nx, ny, nz + dz);
            corner = isSolidBlock(nx, ny + dy, nz + dz);
        } else if (face == DIR_POS_Y || face == DIR_NEG_Y) {
            s1 = isSolidBlock(nx + dx, ny, nz);
            s2 = isSolidBlock(nx, ny, nz + dz);
            corner = isSolidBlock(nx + dx, ny, nz + dz);
        } else {
            s1 = isSolidBlock(nx + dx, ny, nz);
            s2 = isSolidBlock(nx, ny + dy, nz);
            corner = isSolidBlock(nx + dx, ny + dy, nz);
        }

        return calculateAO(s1, s2, corner);
    };

    auto isGreedyOpaqueBlock = [&](uint8_t block) -> bool {
        const BlockInfo& info = getBlockInfo(block);
        return block != BLOCK_AIR &&
            info.isSolid && !info.isTransparent &&
            !isAnyLeaf(block) &&
            block != BLOCK_TALL_GRASS &&
            block != BLOCK_TALL_GRASS_TOP;
    };

    const Vec3 faceNormals[6] = {
        Vec3(1, 0, 0), Vec3(-1, 0, 0),
        Vec3(0, 1, 0), Vec3(0, -1, 0),
        Vec3(0, 0, 1), Vec3(0, 0, -1)
    };

    const int faceOffsetDirs[6][3] = {
        { 1,  0,  0}, {-1,  0,  0},
        { 0,  1,  0}, { 0, -1,  0},
        { 0,  0,  1}, { 0,  0, -1}
    };

    const Vec3 faceCorners[6][4] = {
        { Vec3(1,0,0), Vec3(1,1,0), Vec3(1,1,1), Vec3(1,0,1) },
        { Vec3(0,0,1), Vec3(0,1,1), Vec3(0,1,0), Vec3(0,0,0) },
        { Vec3(0,1,1), Vec3(1,1,1), Vec3(1,1,0), Vec3(0,1,0) },
        { Vec3(0,0,0), Vec3(1,0,0), Vec3(1,0,1), Vec3(0,0,1) },
        { Vec3(1,0,1), Vec3(1,1,1), Vec3(0,1,1), Vec3(0,0,1) },
        { Vec3(0,0,0), Vec3(0,1,0), Vec3(1,1,0), Vec3(1,0,0) }
    };

    const float faceUVs[4][2] = {
        {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}
    };

    auto appendGrassQuad = [&](uint8_t grassTexTile, float heightScale,
                               float windBase, float windTip,
                               float blockRelX, float blockRelY, float blockRelZ,
                               const Vec3& bottomA, const Vec3& bottomB, const Vec3& normal) {
        const uint8_t texTileID = grassTexTile;
        const Vec3 up(0.0f, fScale * heightScale, 0.0f);
        const Vec3 positions[4] = {
            bottomA, bottomA + up, bottomB + up, bottomB
        };
        const Vec3 reversePositions[4] = {
            bottomA, bottomB, bottomB + up, bottomA + up
        };
        const float windWeights[4] = { windBase, windTip, windTip, windBase };
        const float uv[4][2] = {
            {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}
        };
        const float reverseUV[4][2] = {
            {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}
        };

        auto appendWinding = [&](const Vec3* quad, const Vec3& quadNormal,
                                 const float quadUV[4][2],
                                 const float quadWindWeights[4]) {
            uint32_t baseIdx = static_cast<uint32_t>(chunk.stagedVertices.size());
            for (int c = 0; c < 4; ++c) {
                VoxelVertex vert{};
                vert.x = blockRelX + quad[c].x * fScale;
                vert.y = blockRelY + quad[c].y;
                vert.z = blockRelZ + quad[c].z * fScale;
                vert.nx = quadNormal.x;
                vert.ny = quadNormal.y;
                vert.nz = quadNormal.z;
                vert.u = quadUV[c][0];
                vert.v = quadUV[c][1];
                vert.texIndex = static_cast<float>(texTileID);
                vert.ao = 1.0f;
                int plantX = std::clamp(
                    static_cast<int>(blockRelX / fScale),
                    0,
                    CHUNK_SIZE - 1
                );
                int plantY = std::clamp(
                    static_cast<int>((blockRelY + 0.05f) / fScale),
                    0,
                    CHUNK_SIZE - 1
                );
                int plantZ = std::clamp(
                    static_cast<int>(blockRelZ / fScale),
                    0,
                    CHUNK_SIZE - 1
                );
                uint16_t plantL = neighborhood
                    ? neighborhood->lightAt(plantX, plantY, plantZ)
                    : chunk.getLight(plantX, plantY, plantZ);
                vert.lightR = static_cast<float>(getLightR(plantL)) / 15.0f;
                vert.lightG = static_cast<float>(getLightG(plantL)) / 15.0f;
                vert.lightB = static_cast<float>(getLightB(plantL)) / 15.0f;
                vert.skyLight = static_cast<float>(getLightSky(plantL)) / 15.0f;
                vert.windWeight = quadWindWeights[c];
                chunk.stagedVertices.push_back(vert);
            }

            chunk.stagedIndices.push_back(baseIdx + 0);
            chunk.stagedIndices.push_back(baseIdx + 1);
            chunk.stagedIndices.push_back(baseIdx + 2);
            chunk.stagedIndices.push_back(baseIdx + 0);
            chunk.stagedIndices.push_back(baseIdx + 2);
            chunk.stagedIndices.push_back(baseIdx + 3);
        };

        const float reverseWindWeights[4] = {
            windBase, windBase, windTip, windTip
        };
        appendWinding(positions, normal, uv, windWeights);
        appendWinding(reversePositions, -normal, reverseUV, reverseWindWeights);
    };

    {
        auto faceCell = [&](int face, int slice, int u, int v) -> IVec3 {
            switch (face) {
                case DIR_POS_X:
                case DIR_NEG_X: return IVec3(slice, v, u);
                case DIR_POS_Y:
                case DIR_NEG_Y: return IVec3(u, slice, v);
                default: return IVec3(u, v, slice);
            }
        };

        auto facePosition = [&](int face, int slice, int u, int v) -> Vec3 {
            switch (face) {
                case DIR_POS_X: return Vec3(slice + 1.0f, v, u);
                case DIR_NEG_X: return Vec3(slice, v, u);
                case DIR_POS_Y: return Vec3(u, slice + 1.0f, v);
                case DIR_NEG_Y: return Vec3(u, slice, v);
                case DIR_POS_Z: return Vec3(u, v, slice + 1.0f);
                default: return Vec3(u, v, slice);
            }
        };

        auto localFaceCorner = [&](int face, int localU, int localV) -> Vec3 {
            float normalSide = (face % 2 == 0) ? 1.0f : 0.0f;
            switch (face) {
                case DIR_POS_X:
                case DIR_NEG_X: return Vec3(normalSide, localV, localU);
                case DIR_POS_Y:
                case DIR_NEG_Y: return Vec3(localU, normalSide, localV);
                default: return Vec3(localU, localV, normalSide);
            }
        };

        const int cornerU[6][4] = {
            {0, 0, 1, 1}, {1, 1, 0, 0},
            {0, 1, 1, 0}, {0, 1, 1, 0},
            {1, 1, 0, 0}, {0, 0, 1, 1}
        };
        const int cornerV[6][4] = {
            {0, 1, 1, 0}, {0, 1, 1, 0},
            {1, 1, 0, 0}, {0, 0, 1, 1},
            {0, 1, 1, 0}, {0, 1, 1, 0}
        };

        auto appendGreedyQuad = [&](uint8_t blockType, int face, int slice,
                                    int u0, int v0, int width, int height) {
            uint8_t texTileID = getBlockTextureIndex(blockType, face);
            uint32_t baseIdx = static_cast<uint32_t>(chunk.stagedVertices.size());
            float aoVals[4];

            for (int c = 0; c < 4; ++c) {
                int uBoundary = u0 + (cornerU[face][c] ? width : 0);
                int vBoundary = v0 + (cornerV[face][c] ? height : 0);
                Vec3 position = facePosition(face, slice, uBoundary, vBoundary);

                int cellU = cornerU[face][c] ? uBoundary - 1 : uBoundary;
                int cellV = cornerV[face][c] ? vBoundary - 1 : vBoundary;
                IVec3 cell = faceCell(face, slice, cellU, cellV);
                Vec3 cornerPos = localFaceCorner(
                    face,
                    cornerU[face][c],
                    cornerV[face][c]
                );
                int nx = static_cast<int>(cell.x) + faceOffsetDirs[face][0];
                int ny = static_cast<int>(cell.y) + faceOffsetDirs[face][1];
                int nz = static_cast<int>(cell.z) + faceOffsetDirs[face][2];
                float ao = getVertexAO(face, cornerPos, nx, ny, nz);
                aoVals[c] = ao;

                uint16_t light = getNeighborhoodLight(nx, ny, nz);
                const BlockInfo& info = getBlockInfo(blockType);
                float lightR = info.lightR > 0 ? static_cast<float>(info.lightR) / 15.0f : static_cast<float>(getLightR(light)) / 15.0f;
                float lightG = info.lightG > 0 ? static_cast<float>(info.lightG) / 15.0f : static_cast<float>(getLightG(light)) / 15.0f;
                float lightB = info.lightB > 0 ? static_cast<float>(info.lightB) / 15.0f : static_cast<float>(getLightB(light)) / 15.0f;
                float skyLight = (info.lightR > 0 || info.lightG > 0 || info.lightB > 0)
                    ? 0.0f
                    : static_cast<float>(getLightSky(light)) / 15.0f;

                VoxelVertex vert{};
                vert.x = position.x * fScale;
                vert.y = position.y * fScale;
                vert.z = position.z * fScale;
                vert.nx = faceNormals[face].x;
                vert.ny = faceNormals[face].y;
                vert.nz = faceNormals[face].z;
                vert.u = static_cast<float>(cornerU[face][c] * width);
                vert.v = static_cast<float>(cornerV[face][c] * height);
                vert.texIndex = static_cast<float>(texTileID);
                vert.ao = ao;
                vert.lightR = lightR;
                vert.lightG = lightG;
                vert.lightB = lightB;
                vert.skyLight = skyLight;
                vert.windWeight = 0.0f;
                chunk.stagedVertices.push_back(vert);
            }

            if (aoVals[0] + aoVals[2] < aoVals[1] + aoVals[3]) {
                chunk.stagedIndices.push_back(baseIdx + 0);
                chunk.stagedIndices.push_back(baseIdx + 1);
                chunk.stagedIndices.push_back(baseIdx + 3);
                chunk.stagedIndices.push_back(baseIdx + 1);
                chunk.stagedIndices.push_back(baseIdx + 2);
                chunk.stagedIndices.push_back(baseIdx + 3);
            } else {
                chunk.stagedIndices.push_back(baseIdx + 0);
                chunk.stagedIndices.push_back(baseIdx + 1);
                chunk.stagedIndices.push_back(baseIdx + 2);
                chunk.stagedIndices.push_back(baseIdx + 0);
                chunk.stagedIndices.push_back(baseIdx + 2);
                chunk.stagedIndices.push_back(baseIdx + 3);
            }
        };

        struct GreedyCell {
            uint8_t block = BLOCK_AIR;
            uint16_t light = 0;
            uint8_t aoSignature = 0;

            bool operator==(const GreedyCell& other) const {
                return block == other.block &&
                    light == other.light &&
                    aoSignature == other.aoSignature;
            }
        };

        auto calculateAoSignature = [&](const IVec3& cell, int face) -> uint8_t {
            uint8_t signature = 0;
            int nx = static_cast<int>(cell.x) + faceOffsetDirs[face][0];
            int ny = static_cast<int>(cell.y) + faceOffsetDirs[face][1];
            int nz = static_cast<int>(cell.z) + faceOffsetDirs[face][2];
            for (int c = 0; c < 4; ++c) {
                Vec3 cornerPos = faceCorners[face][c];
                float ao = getVertexAO(face, cornerPos, nx, ny, nz);
                uint8_t value = ao <= 0.3f
                    ? 3
                    : (ao < 0.7f ? 2 : (ao < 0.9f ? 1 : 0));
                signature |= static_cast<uint8_t>(value << (c * 2));
            }
            return signature;
        };

        std::vector<GreedyCell> mask(CHUNK_SIZE * CHUNK_SIZE);
        for (int face = 0; face < 6; ++face) {
            for (int slice = 0; slice < CHUNK_SIZE; ++slice) {
                std::fill(mask.begin(), mask.end(), GreedyCell{});

                for (int v = 0; v < CHUNK_SIZE; ++v) {
                    for (int u = 0; u < CHUNK_SIZE; ++u) {
                        IVec3 cell = faceCell(face, slice, u, v);
                        uint8_t block = chunk.getBlock(
                            static_cast<int>(cell.x),
                            static_cast<int>(cell.y),
                            static_cast<int>(cell.z)
                        );
                        if (!isGreedyOpaqueBlock(block)) continue;

                        int nx = static_cast<int>(cell.x) + faceOffsetDirs[face][0];
                        int ny = static_cast<int>(cell.y) + faceOffsetDirs[face][1];
                        int nz = static_cast<int>(cell.z) + faceOffsetDirs[face][2];
                        if (!isSolidBlock(nx, ny, nz)) {
                            mask[v * CHUNK_SIZE + u] = {
                                block,
                                getNeighborhoodLight(nx, ny, nz),
                                calculateAoSignature(cell, face)
                            };
                        }
                    }
                }

                for (int v = 0; v < CHUNK_SIZE; ++v) {
                    for (int u = 0; u < CHUNK_SIZE; ++u) {
                        GreedyCell cell = mask[v * CHUNK_SIZE + u];
                        if (cell.block == BLOCK_AIR) continue;

                        int width = 1;
                        while (u + width < CHUNK_SIZE &&
                               mask[v * CHUNK_SIZE + u + width] == cell) {
                            ++width;
                        }

                        int height = 1;
                        bool canGrow = true;
                        while (v + height < CHUNK_SIZE && canGrow) {
                            for (int du = 0; du < width; ++du) {
                                if (!(mask[(v + height) * CHUNK_SIZE + u + du] == cell)) {
                                    canGrow = false;
                                    break;
                                }
                            }
                            if (canGrow) ++height;
                        }

                        for (int dv = 0; dv < height; ++dv) {
                            for (int du = 0; du < width; ++du) {
                                mask[(v + dv) * CHUNK_SIZE + u + du] = GreedyCell{};
                            }
                        }
                        appendGreedyQuad(cell.block, face, slice, u, v, width, height);
                    }
                }
            }
        }
    }

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                uint8_t blockType = chunk.getBlock(x, y, z);
                if (blockType == BLOCK_AIR) continue;
                if (chunk.lod >= 2 &&
                    (blockType == BLOCK_TALL_GRASS ||
                     blockType == BLOCK_TALL_GRASS_TOP)) {
                    continue;
                }
                if (isGreedyOpaqueBlock(blockType)) {
                    continue;
                }

                float relX = static_cast<float>(x * scale);
                float relY = static_cast<float>(y * scale);
                float relZ = static_cast<float>(z * scale);

                uint8_t leafVariant = 0;
                if (isAnyLeaf(blockType)) {
                    uint64_t leafHash =
                        static_cast<uint64_t>(wmx + x * scale) * 0x9E3779B185EBCA87ULL ^
                        static_cast<uint64_t>(wmy + y * scale) * 0xC2B2AE3D27D4EB4FULL ^
                        static_cast<uint64_t>(wmz + z * scale) * 0xBF58476D1CE4E5B9ULL;
                    leafHash ^= leafHash >> 30;
                    leafHash *= 0x94D049BB133111EBULL;
                    leafHash ^= leafHash >> 27;
                    leafVariant = static_cast<uint8_t>(leafHash % 6ULL);
                }

                if (blockType == BLOCK_TALL_GRASS || blockType == BLOCK_TALL_GRASS_TOP) {
                    uint64_t grassHash = static_cast<uint64_t>(wmx + x * scale) *
                        0x9E3779B185EBCA87ULL ^
                        static_cast<uint64_t>(wmz + z * scale) *
                        0xC2B2AE3D27D4EB4FULL;
                    grassHash ^= grassHash >> 30;
                    grassHash *= 0xBF58476D1CE4E5B9ULL;
                    grassHash ^= grassHash >> 27;
                    bool upperHalf = blockType == BLOCK_TALL_GRASS_TOP;
                    bool rareTwoTallLower = blockType == BLOCK_TALL_GRASS &&
                        chunk.getBlock(x, y + 1, z) == BLOCK_TALL_GRASS_TOP;
                    bool twoTallPlant = rareTwoTallLower ||
                        (upperHalf && y > 0 &&
                            chunk.getBlock(x, y - 1, z) == BLOCK_TALL_GRASS);
                    uint8_t grassVariant = static_cast<uint8_t>((grassHash >> 8) & 1ULL);
                    if (!twoTallPlant && grassHash % 15ULL == 0ULL) {
                        grassVariant = 2;
                    }
                    uint8_t grassTexTile = upperHalf
                        ? static_cast<uint8_t>(32 + grassVariant)
                        : static_cast<uint8_t>((rareTwoTallLower ? 35 : 29) + grassVariant);
                    float heightScale = 1.0f;
                    float inset = 0.08f +
                        static_cast<float>((grassHash >> 16) & 0x1f) / 31.0f * 0.06f;
                    float baseY = relY - 0.02f;
                    float windBase = upperHalf ? 0.5f : 0.0f;
                    float windTip = upperHalf ? 1.0f :
                        (rareTwoTallLower ? 0.5f : 1.0f);
                    appendGrassQuad(
                        grassTexTile, heightScale, windBase, windTip, relX, baseY, relZ,
                        Vec3(inset, 0.0f, inset),
                        Vec3(1.0f - inset, 0.0f, 1.0f - inset),
                        Vec3(1.0f, 0.0f, -1.0f).normalized()
                    );
                    appendGrassQuad(
                        grassTexTile, heightScale, windBase, windTip, relX, baseY, relZ,
                        Vec3(inset, 0.0f, 1.0f - inset),
                        Vec3(1.0f - inset, 0.0f, inset),
                        Vec3(-1.0f, 0.0f, -1.0f).normalized()
                    );
                    continue;
                }

                for (int f = 0; f < 6; ++f) {
                    int nx = x + faceOffsetDirs[f][0];
                    int ny = y + faceOffsetDirs[f][1];
                    int nz = z + faceOffsetDirs[f][2];

                    if (isSolidBlock(nx, ny, nz)) continue;

                    uint8_t texTileID = getBlockTextureIndex(blockType, f);
                    if (isAnyLeaf(blockType)) {
                        texTileID = getLeafTextureIndex(blockType, leafVariant);
                    }
                    bool flipGrassSideV = blockType == BLOCK_GRASS &&
                        f != DIR_POS_Y && f != DIR_NEG_Y;

                    const BlockInfo& bInfo = getBlockInfo(blockType);
                    float fR = 0.0f, fG = 0.0f, fB = 0.0f, fSky = 0.0f;
                    if (bInfo.lightR > 0 || bInfo.lightG > 0 || bInfo.lightB > 0) {
                        fR = static_cast<float>(bInfo.lightR) / 15.0f;
                        fG = static_cast<float>(bInfo.lightG) / 15.0f;
                        fB = static_cast<float>(bInfo.lightB) / 15.0f;
                        fSky = 0.0f;
                    } else {
                        uint16_t lVal = getNeighborhoodLight(nx, ny, nz);
                        fR = static_cast<float>(getLightR(lVal)) / 15.0f;
                        fG = static_cast<float>(getLightG(lVal)) / 15.0f;
                        fB = static_cast<float>(getLightB(lVal)) / 15.0f;
                        fSky = static_cast<float>(getLightSky(lVal)) / 15.0f;
                    }
                    float ao[4];
                    for (int c = 0; c < 4; ++c) {
                        Vec3 cornerPos = faceCorners[f][c];
                        ao[c] = getVertexAO(f, cornerPos, nx, ny, nz);
                    }

                    uint32_t baseIdx = static_cast<uint32_t>(chunk.stagedVertices.size());

                    for (int c = 0; c < 4; ++c) {
                        Vec3 p = faceCorners[f][c];
                        VoxelVertex vert;
                        vert.x = relX + p.x * fScale;
                        vert.y = relY + p.y * fScale;
                        vert.z = relZ + p.z * fScale;
                        vert.nx = faceNormals[f].x;
                        vert.ny = faceNormals[f].y;
                        vert.nz = faceNormals[f].z;
                        vert.u = faceUVs[c][0];
                        vert.v = flipGrassSideV ? 1.0f - faceUVs[c][1] : faceUVs[c][1];
                        vert.texIndex = static_cast<float>(texTileID);
                        vert.ao = ao[c];
                        vert.lightR = fR;
                        vert.lightG = fG;
                        vert.lightB = fB;
                        vert.skyLight = fSky;
                        vert.windWeight = 0.0f;

                        chunk.stagedVertices.push_back(vert);
                    }

                    if (ao[0] + ao[2] < ao[1] + ao[3]) {
                        chunk.stagedIndices.push_back(baseIdx + 0);
                        chunk.stagedIndices.push_back(baseIdx + 1);
                        chunk.stagedIndices.push_back(baseIdx + 3);
                        chunk.stagedIndices.push_back(baseIdx + 1);
                        chunk.stagedIndices.push_back(baseIdx + 2);
                        chunk.stagedIndices.push_back(baseIdx + 3);
                    } else {
                        chunk.stagedIndices.push_back(baseIdx + 0);
                        chunk.stagedIndices.push_back(baseIdx + 1);
                        chunk.stagedIndices.push_back(baseIdx + 2);
                        chunk.stagedIndices.push_back(baseIdx + 0);
                        chunk.stagedIndices.push_back(baseIdx + 2);
                        chunk.stagedIndices.push_back(baseIdx + 3);
                    }
                }
            }
        }
    }

    chunk.isMeshStaged = true;
    return { chunk.stagedVertices, chunk.stagedIndices };
}
