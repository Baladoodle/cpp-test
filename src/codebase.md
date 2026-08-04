# Codebase Summary

Merged `31` files from `src`.

## Table of Contents

- [Block.hpp](#blockhpp) (`101` lines)
- [Camera.hpp](#camerahpp) (`62` lines)
- [Chunk.hpp](#chunkhpp) (`128` lines)
- [ChunkBorderRenderer.hpp](#chunkborderrendererhpp) (`196` lines)
- [ChunkManager.hpp](#chunkmanagerhpp) (`1258` lines)
- [ChunkStore.cpp](#chunkstorecpp) (`83` lines)
- [ChunkStore.hpp](#chunkstorehpp) (`42` lines)
- [HUD.hpp](#hudhpp) (`310` lines)
- [IWorldQuery.hpp](#iworldqueryhpp) (`15` lines)
- [LightingSystem.cpp](#lightingsystemcpp) (`233` lines)
- [LightingSystem.hpp](#lightingsystemhpp) (`40` lines)
- [Main.cpp](#maincpp) (`758` lines)
- [MathUtils.hpp](#mathutilshpp) (`217` lines)
- [Mesh.hpp](#meshhpp) (`528` lines)
- [MeshBuilder.hpp](#meshbuilderhpp) (`112` lines)
- [Mesher.cpp](#meshercpp) (`532` lines)
- [Mesher.hpp](#mesherhpp) (`29` lines)
- [MeshingNeighborhood.hpp](#meshingneighborhoodhpp) (`34` lines)
- [Physics.hpp](#physicshpp) (`142` lines)
- [Renderer.cpp](#renderercpp) (`52` lines)
- [Renderer.hpp](#rendererhpp) (`42` lines)
- [Shader.hpp](#shaderhpp) (`170` lines)
- [SimplexNoise.cpp](#simplexnoisecpp) (`44` lines)
- [SimplexNoise.hpp](#simplexnoisehpp) (`120` lines)
- [Skybox.hpp](#skyboxhpp) (`147` lines)
- [StreamPlanner.cpp](#streamplannercpp) (`131` lines)
- [StreamPlanner.hpp](#streamplannerhpp) (`36` lines)
- [TextureAtlas.hpp](#textureatlashpp) (`564` lines)
- [WorldGen.hpp](#worldgenhpp) (`1166` lines)
- [WorldGenerator.cpp](#worldgeneratorcpp) (`412` lines)
- [WorldGenerator.hpp](#worldgeneratorhpp) (`24` lines)

---

## Block.hpp

**Path:** `Block.hpp` | **Lines:** 101 | **Size:** 3422 bytes

```cpp
#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <cstdint>

enum BlockType : uint8_t {
    BLOCK_AIR = 0,
    BLOCK_GRASS = 1,
    BLOCK_DIRT = 2,
    BLOCK_STONE = 3,
    BLOCK_GLOW_CRYSTAL = 4,
    BLOCK_OAK_LOG = 5,
    BLOCK_LEAVES = 6,
    BLOCK_SAND = 7,
    BLOCK_SKY_QUARTZ = 8,
    BLOCK_WATER = 9,
    BLOCK_LEAVES_LIGHT = 10,
    BLOCK_LEAVES_DARK = 11,
    BLOCK_LEAVES_WARM = 12,
    BLOCK_TALL_GRASS = 13,
    BLOCK_TALL_GRASS_TOP = 14,
    BLOCK_DEEP_STONE = 15,
    BLOCK_COUNT
};

enum Direction : uint8_t {
    DIR_POS_X = 0,
    DIR_NEG_X = 1,
    DIR_POS_Y = 2,
    DIR_NEG_Y = 3,
    DIR_POS_Z = 4,
    DIR_NEG_Z = 5
};

struct BlockInfo {
    const char* name;
    bool isSolid;
    bool isTransparent;
    uint8_t lightR;
    uint8_t lightG;
    uint8_t lightB;
    uint8_t topTex;
    uint8_t sideTex;
    uint8_t bottomTex;
};

inline const BlockInfo& getBlockInfo(uint8_t type) {
    static const BlockInfo infos[BLOCK_COUNT] = {
        // name, solid, transparent, lightR, lightG, lightB, topTex, sideTex, bottomTex
        { "Air",            false, true,  0,  0,  0,  0,  0,  0 },
        { "Grass",          true,  false, 0,  0,  0,  0,  1,  2 }, // 0=GrassTop, 1=GrassSide, 2=Dirt
        { "Dirt",           true,  false, 0,  0,  0,  2,  2,  2 },
        { "Stone",          true,  false, 0,  0,  0,  3,  3,  3 },
        { "Glow Crystal",   true,  false, 15, 13, 6,  4,  4,  4 }, // Warm Golden Glow
        { "Oak Log",        true,  false, 0,  0,  0,  6,  5,  6 }, // 5=LogSide, 6=LogTop
        { "Leaves",         true,  false, 0,  0,  0,  7,  7,  7 },
        { "Sand",           true,  false, 0,  0,  0,  8,  8,  8 },
        { "Sky Quartz",     true,  false, 0,  0,  0, 59,  9,  9 },
        { "Water",          false, true,  0,  0,  0, 10, 10, 10 },
        { "Light Oak Leaves",true,  false, 0,  0,  0, 13, 13, 13 },
        { "Dark Oak Leaves", true,  false, 0,  0,  0, 19, 19, 19 },
        { "Warm Oak Leaves", true,  false, 0,  0,  0, 23, 23, 23 },
        { "Tall Grass",      false, true,  0,  0,  0, 29, 29, 29 },
        { "Tall Grass Top",  false, true,  0,  0,  0, 32, 32, 32 },
        { "Deep Stone",      true,  false, 0,  0,  0, 58, 58, 58 }
    };
    if (type >= BLOCK_COUNT) return infos[0];
    return infos[type];
}

inline bool isAnyLeaf(uint8_t type) {
    return type == BLOCK_LEAVES || type == BLOCK_LEAVES_LIGHT ||
           type == BLOCK_LEAVES_DARK || type == BLOCK_LEAVES_WARM;
}

inline uint8_t getBlockTextureIndex(uint8_t blockType, uint8_t direction) {
    const BlockInfo& info = getBlockInfo(blockType);
    if (direction == DIR_POS_Y) return info.topTex;
    if (direction == DIR_NEG_Y) return info.bottomTex;
    return info.sideTex;
}

// Dummy leaf art uses several masks per palette. Keep the mapping here so
// replacing the procedural atlas with real assets later does not touch mesh
// generation or worldgen.
inline uint8_t getLeafTextureIndex(uint8_t blockType, uint8_t variant) {
    static constexpr uint8_t leafTiles[4][6] = {
        { 7, 38, 39, 40, 41, 42 },
        { 13, 43, 44, 45, 46, 47 },
        { 19, 48, 49, 50, 51, 52 },
        { 23, 53, 54, 55, 56, 57 }
    };

    int palette = 0;
    if (blockType == BLOCK_LEAVES_LIGHT) palette = 1;
    else if (blockType == BLOCK_LEAVES_DARK) palette = 2;
    else if (blockType == BLOCK_LEAVES_WARM) palette = 3;
    return leafTiles[palette][variant % 6];
}

#endif // BLOCK_HPP
```

## Camera.hpp

**Path:** `Camera.hpp` | **Lines:** 62 | **Size:** 1691 bytes

```cpp
#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "MathUtils.hpp"
#include <cmath>
#include <algorithm>

class Camera {
public:
    Vec3 position;
    Vec3 front;
    Vec3 up;
    Vec3 right;
    Vec3 worldUp;

    float yaw;
    float pitch;

    float fov;
    float mouseSensitivity;

    Camera(Vec3 startPos = Vec3(0.0f, 60.0f, 0.0f), Vec3 startUp = Vec3(0.0f, 1.0f, 0.0f), float startYaw = -90.0f, float startPitch = 0.0f)
        : position(startPos), worldUp(startUp), yaw(startYaw), pitch(startPitch), fov(70.0f), mouseSensitivity(0.1f) {
        updateCameraVectors();
    }

    Mat4 getViewMatrix() const {
        // Pure orientation matrix (view relative to camera position)
        return Mat4::lookAt(Vec3(0, 0, 0), front, up);
    }

    Mat4 getProjectionMatrix(float aspectRatio, float nearVal = 0.1f, float farVal = 20000.0f) const {
        return Mat4::perspective(fov * DEG2RAD, aspectRatio, nearVal, farVal);
    }

    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw   += xoffset;
        pitch += yoffset;

        if (constrainPitch) {
            pitch = std::clamp(pitch, -89.0f, 89.0f);
        }

        updateCameraVectors();
    }

    void updateCameraVectors() {
        Vec3 f;
        f.x = std::cos(yaw * DEG2RAD) * std::cos(pitch * DEG2RAD);
        f.y = std::sin(pitch * DEG2RAD);
        f.z = std::sin(yaw * DEG2RAD) * std::cos(pitch * DEG2RAD);
        front = f.normalized();

        right = Vec3::cross(front, worldUp).normalized();
        up    = Vec3::cross(right, front).normalized();
    }
};

#endif // CAMERA_HPP
```

## Chunk.hpp

**Path:** `Chunk.hpp` | **Lines:** 128 | **Size:** 4954 bytes

```cpp
#ifndef CHUNK_HPP
#define CHUNK_HPP

#include <vector>
#include <cstdint>
#include <memory>
#include <atomic>
#include "MathUtils.hpp"
#include "Block.hpp"
#include "Mesh.hpp"

constexpr int CHUNK_SIZE = 32;
constexpr int CHUNK_VOL = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
constexpr int PADDED_SIZE = CHUNK_SIZE + 2;
constexpr int PADDED_VOL = PADDED_SIZE * PADDED_SIZE * PADDED_SIZE;

inline int getVoxelIndex(int x, int y, int z) {
    return (y * CHUNK_SIZE + z) * CHUNK_SIZE + x;
}
inline int getPaddedVoxelIndex(int x, int y, int z) {
    return ((y + 1) * PADDED_SIZE + (z + 1)) * PADDED_SIZE + (x + 1);
}

inline uint8_t getLightR(uint16_t val) { return (val >> 12) & 0xF; }
inline uint8_t getLightG(uint16_t val) { return (val >> 8) & 0xF; }
inline uint8_t getLightB(uint16_t val) { return (val >> 4) & 0xF; }
inline uint8_t getLightSky(uint16_t val) { return val & 0xF; }

inline uint16_t packLight(uint8_t r, uint8_t g, uint8_t b, uint8_t sky) {
    return (static_cast<uint16_t>(r & 0xF) << 12) |
           (static_cast<uint16_t>(g & 0xF) << 8)  |
           (static_cast<uint16_t>(b & 0xF) << 4)  |
           (static_cast<uint16_t>(sky & 0xF));
}

enum class ChunkState : uint8_t {
    Unloaded,      // Memory allocated, no voxel data
    Generating,    // Background thread is running WorldGen density sampling
    LightPending,  // Voxel data ready, awaiting lighting propagation
    MeshQueued,    // Neighborhood ready, mesh task is scheduled
    MeshStaged,    // CPU vertices/indices built and queued for GPU upload
    Uploaded,      // Geometry loaded in GeometryArena, ready to render
    Evicting       // Stale chunk scheduled for memory reclamation
};

struct Chunk {
    IVec3 chunkPos; // Chunk grid coordinates at this LOD level
    int lod;        // 0 to 6
    int scale;      // 1 << lod
    int worldSize;  // CHUNK_SIZE * scale
    IVec3 worldMin; // World origin position of this chunk

    uint8_t blocks[CHUNK_VOL];
    uint16_t light[CHUNK_VOL];
    bool isEmpty = true;
    std::atomic<ChunkState> state{ChunkState::Unloaded};
    std::atomic<bool> isGenerated{false};
    std::atomic<bool> isLightReady{false};
    std::atomic<bool> isMeshStaged{false};
    std::atomic<bool> isMeshUploaded{false};
    std::atomic<bool> isMeshQueued{false};

    ChunkState getState() const { return state.load(std::memory_order_acquire); }
    void setState(ChunkState s) { state.store(s, std::memory_order_release); }
    bool isAtLeast(ChunkState s) const {
        return static_cast<uint8_t>(getState()) >= static_cast<uint8_t>(s);
    }
    bool transitionTo(ChunkState expected, ChunkState next) {
        ChunkState exp = expected;
        return state.compare_exchange_strong(exp, next, std::memory_order_acq_rel);
    }
    std::atomic<bool> meshDirty{true};
    std::atomic<bool> isPendingWork{true};
    std::atomic<bool> resident{true};
    std::atomic<uint64_t> workToken{0};
    // Staging mesh data built on background thread
    std::vector<VoxelVertex> stagedVertices;
    std::vector<uint32_t> stagedIndices;

    // Renderable OpenGL mesh (uploaded on main thread)
    Mesh mesh;

    Chunk(IVec3 cpos, int lodLevel) : chunkPos(cpos), lod(lodLevel) {
        scale = 1 << lod;
        worldSize = CHUNK_SIZE * scale;
        worldMin = IVec3(cpos.x * worldSize, cpos.y * worldSize, cpos.z * worldSize);
        std::fill(blocks, blocks + CHUNK_VOL, 0);
        std::fill(light, light + CHUNK_VOL, 0);
    }

    inline uint8_t getBlock(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return BLOCK_AIR;
        return blocks[getVoxelIndex(x, y, z)];
    }

    inline uint8_t getPaddedBlock(int x, int y, int z) const {
        return getBlock(x, y, z);
    }

    inline void setBlock(int x, int y, int z, uint8_t b) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return;
        blocks[getVoxelIndex(x, y, z)] = b;
    }

    inline uint16_t getLight(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return 0;
        return light[getVoxelIndex(x, y, z)];
    }

    inline void setLight(int x, int y, int z, uint16_t l) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return;
        light[getVoxelIndex(x, y, z)] = l;
    }

    inline uint16_t getPaddedLight(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
            int bx = std::clamp(x, 0, CHUNK_SIZE - 1);
            int by = std::clamp(y, 0, CHUNK_SIZE - 1);
            int bz = std::clamp(z, 0, CHUNK_SIZE - 1);
            uint16_t bl = getLight(bx, by, bz);
            if (bl == 0 && y >= 0) return packLight(0, 0, 0, 15);
            return bl;
        }
        return getLight(x, y, z);
    }
};

#endif // CHUNK_HPP
```

## ChunkBorderRenderer.hpp

**Path:** `ChunkBorderRenderer.hpp` | **Lines:** 196 | **Size:** 6827 bytes

```cpp
#ifndef CHUNK_BORDER_RENDERER_HPP
#define CHUNK_BORDER_RENDERER_HPP

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <epoxy/gl.h>
#include <GLFW/glfw3.h>
#include "Shader.hpp"
#include "MathUtils.hpp"
#include "Chunk.hpp"

struct LineVertex {
    Vec3 pos;   // Position relative to camera
    Vec4 color; // RGBA color
};

class ChunkBorderRenderer {
private:
    GLuint vao = 0;
    GLuint vbo = 0;
    Shader shader;
    std::vector<LineVertex> lineVertices;

public:
    void init() {
        const char* vShaderSrc = R"(
            #version 430 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec4 aColor;

            out vec4 vColor;

            uniform mat4 uProjection;
            uniform mat4 uView;

            void main() {
                vColor = aColor;
                gl_Position = uProjection * uView * vec4(aPos, 1.0);
            }
        )";

        const char* fShaderSrc = R"(
            #version 430 core
            in vec4 vColor;
            out vec4 FragColor;

            void main() {
                FragColor = vColor;
            }
        )";

        shader.compile(vShaderSrc, fShaderSrc);

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        // Position attribute (location = 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, pos));

        // Color attribute (location = 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, color));

        glBindVertexArray(0);
    }

    ~ChunkBorderRenderer() {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
    }

    void clear() {
        lineVertices.clear();
    }

    // Add a single line segment
    void addLine(const Vec3& p1, const Vec3& p2, const Vec4& color) {
        lineVertices.push_back({ p1, color });
        lineVertices.push_back({ p2, color });
    }

    // Add a 3D bounding box
    void addBox(const Vec3& minP, const Vec3& maxP, const Vec4& color) {
        Vec3 c0(minP.x, minP.y, minP.z);
        Vec3 c1(maxP.x, minP.y, minP.z);
        Vec3 c2(maxP.x, minP.y, maxP.z);
        Vec3 c3(minP.x, minP.y, maxP.z);

        Vec3 c4(minP.x, maxP.y, minP.z);
        Vec3 c5(maxP.x, maxP.y, minP.z);
        Vec3 c6(maxP.x, maxP.y, maxP.z);
        Vec3 c7(minP.x, maxP.y, maxP.z);

        // Bottom face
        addLine(c0, c1, color);
        addLine(c1, c2, color);
        addLine(c2, c3, color);
        addLine(c3, c0, color);

        // Top face
        addLine(c4, c5, color);
        addLine(c5, c6, color);
        addLine(c6, c7, color);
        addLine(c7, c4, color);

        // Vertical edges
        addLine(c0, c4, color);
        addLine(c1, c5, color);
        addLine(c2, c6, color);
        addLine(c3, c7, color);
    }

    // Add 32x32 chunk borders for a given chunk
    void addChunkBorders(const Chunk* chunk, const Vec3& cameraPos) {
        if (!chunk) return;

        Vec3 minP(
            static_cast<float>(chunk->worldMin.x) - cameraPos.x,
            static_cast<float>(chunk->worldMin.y) - cameraPos.y,
            static_cast<float>(chunk->worldMin.z) - cameraPos.z
        );
        float size = static_cast<float>(chunk->worldSize);
        Vec3 maxP = minP + Vec3(size);

        // Colors per LOD
        Vec4 outerColor;
        switch (chunk->lod) {
            case 0:  outerColor = Vec4(1.0f, 0.85f, 0.1f, 1.0f); break; // Gold / Yellow
            case 1:  outerColor = Vec4(0.0f, 0.85f, 1.0f, 1.0f); break; // Cyan
            case 2:  outerColor = Vec4(0.2f, 0.9f, 0.3f, 1.0f);  break; // Green
            case 3:  outerColor = Vec4(0.9f, 0.3f, 0.9f, 1.0f);  break; // Magenta
            default: outerColor = Vec4(1.0f, 0.5f, 0.1f, 1.0f);  break; // Orange
        }

        // Draw the outer bounding box of this chunk
        addBox(minP, maxP, outerColor);

        // For coarse LODs (worldSize > 32), draw internal 32x32 sub-grid lines
        // so every 32x32 chunk boundary is rendered!
        if (chunk->worldSize > CHUNK_SIZE) {
            Vec4 subGridColor = Vec4(1.0f, 0.85f, 0.1f, 0.6f); // Semi-transparent yellow for 32x32 grid lines
            for (int stepX = CHUNK_SIZE; stepX < chunk->worldSize; stepX += CHUNK_SIZE) {
                float x = minP.x + static_cast<float>(stepX);
                // Vertical slice lines along X
                addLine(Vec3(x, minP.y, minP.z), Vec3(x, maxP.y, minP.z), subGridColor);
                addLine(Vec3(x, minP.y, maxP.z), Vec3(x, maxP.y, maxP.z), subGridColor);
                addLine(Vec3(x, minP.y, minP.z), Vec3(x, minP.y, maxP.z), subGridColor);
                addLine(Vec3(x, maxP.y, minP.z), Vec3(x, maxP.y, maxP.z), subGridColor);
            }
            for (int stepY = CHUNK_SIZE; stepY < chunk->worldSize; stepY += CHUNK_SIZE) {
                float y = minP.y + static_cast<float>(stepY);
                // Horizontal slice lines along Y
                addLine(Vec3(minP.x, y, minP.z), Vec3(maxP.x, y, minP.z), subGridColor);
                addLine(Vec3(minP.x, y, maxP.z), Vec3(maxP.x, y, maxP.z), subGridColor);
                addLine(Vec3(minP.x, y, minP.z), Vec3(minP.x, y, maxP.z), subGridColor);
                addLine(Vec3(maxP.x, y, minP.z), Vec3(maxP.x, y, maxP.z), subGridColor);
            }
            for (int stepZ = CHUNK_SIZE; stepZ < chunk->worldSize; stepZ += CHUNK_SIZE) {
                float z = minP.z + static_cast<float>(stepZ);
                // Slice lines along Z
                addLine(Vec3(minP.x, minP.y, z), Vec3(maxP.x, minP.y, z), subGridColor);
                addLine(Vec3(minP.x, maxP.y, z), Vec3(maxP.x, maxP.y, z), subGridColor);
                addLine(Vec3(minP.x, minP.y, z), Vec3(minP.x, maxP.y, z), subGridColor);
                addLine(Vec3(maxP.x, minP.y, z), Vec3(maxP.x, maxP.y, z), subGridColor);
            }
        }
    }

    void render(const Mat4& projection, const Mat4& view) {
        if (lineVertices.empty()) return;

        shader.use();
        shader.setMat4("uProjection", projection);
        shader.setMat4("uView", view);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(LineVertex), lineVertices.data(), GL_DYNAMIC_DRAW);

        glLineWidth(2.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVertices.size()));

        glBindVertexArray(0);
        glLineWidth(1.0f);
    }
};

#endif // CHUNK_BORDER_RENDERER_HPP
```

## ChunkManager.hpp

**Path:** `ChunkManager.hpp` | **Lines:** 1258 | **Size:** 51290 bytes

```cpp
#ifndef CHUNK_MANAGER_HPP
#define CHUNK_MANAGER_HPP

#include "IWorldQuery.hpp"
#include "ChunkStore.hpp"
#include "StreamPlanner.hpp"
#include "Renderer.hpp"
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

class ChunkManager : public IWorldQuery {
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
    ChunkStore chunkStore;
    Renderer renderer;
    GeometryArena& geometryArena = renderer.getGeometryArena();
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
    static constexpr size_t MAX_LIGHT_NODES_PER_FRAME = 8192;

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
        (void)lod; (void)cx; (void)cy; (void)cz; (void)camPos;
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

        size_t processedNodes = 0;
        while (!lightQueue.empty() &&
               processedNodes < MAX_LIGHT_NODES_PER_FRAME) {
            ++processedNodes;
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
                chunk->setState(ChunkState::Generating);
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
                    chunk->setState(ChunkState::LightPending);
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
                    chunk->setState(ChunkState::MeshStaged);
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
        (void)viewportWidth; (void)viewportHeight;
        if (!renderer.initialize()) {
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
        renderer.advanceFrame();
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
                        chunk->setState(ChunkState::Evicting);
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
        (void)verticalFovRadians; (void)viewportWidth; (void)voxelShaderProgram; (void)view; (void)viewProjection;
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
        geometryArena.drawIndirect(geometryArena.getLastDrawCommandCount());
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
        renderer.markSubmitted();
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

    bool isBlockSolidAt(int64_t wx, int64_t wy, int64_t wz) const override {
        return chunkStore.isBlockSolidAt(wx, wy, wz);
    }

    BlockType getBlockAt(const IVec3& worldPos) const override {
        return chunkStore.getBlockAt(worldPos);
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
```

## ChunkStore.cpp

**Path:** `ChunkStore.cpp` | **Lines:** 83 | **Size:** 3299 bytes

```cpp
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
```

## ChunkStore.hpp

**Path:** `ChunkStore.hpp` | **Lines:** 42 | **Size:** 1242 bytes

```cpp
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
```

## HUD.hpp

**Path:** `HUD.hpp` | **Lines:** 310 | **Size:** 12337 bytes

```cpp
#ifndef HUD_HPP
#define HUD_HPP

#include "Shader.hpp"
#include "MathUtils.hpp"
#include <vector>
#include <string>
#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

class HUD {
private:
    GLuint fontTex = 0;
    GLuint vao = 0, vbo = 0;
    Shader shader;

    // Embedded 8x8 bitmap font data (Basic ASCII 32..127)
    // 96 characters * 8 bytes
    static const uint8_t font8x8[96][8];

public:
    HUD() = default;
    ~HUD() {
        if (fontTex != 0) glDeleteTextures(1, &fontTex);
        if (vao != 0) glDeleteVertexArrays(1, &vao);
        if (vbo != 0) glDeleteBuffers(1, &vbo);
    }

    HUD(const HUD&) = delete;
    HUD& operator=(const HUD&) = delete;

    HUD(HUD&& other) noexcept
        : fontTex(other.fontTex), vao(other.vao), vbo(other.vbo), shader(std::move(other.shader)) {
        other.fontTex = 0;
        other.vao = 0;
        other.vbo = 0;
    }

    HUD& operator=(HUD&& other) noexcept {
        if (this != &other) {
            if (fontTex != 0) glDeleteTextures(1, &fontTex);
            if (vao != 0) glDeleteVertexArrays(1, &vao);
            if (vbo != 0) glDeleteBuffers(1, &vbo);
            fontTex = other.fontTex;
            vao = other.vao;
            vbo = other.vbo;
            shader = std::move(other.shader);
            other.fontTex = 0;
            other.vao = 0;
            other.vbo = 0;
        }
        return *this;
    }
    void init() {
        // Generate font texture (128x64 atlas = 16x8 characters)
        std::vector<uint8_t> pixels(128 * 64 * 4, 0);

        for (int c = 0; c < 96; ++c) {
            int charX = (c % 16) * 8;
            int charY = (c / 16) * 8;

            for (int py = 0; py < 8; ++py) {
                uint8_t row = font8x8[c][py];
                for (int px = 0; px < 8; ++px) {
                    if (row & (1 << (7 - px))) {
                        int ax = charX + px;
                        int ay = charY + py;
                        int idx = (ay * 128 + ax) * 4;
                        pixels[idx + 0] = 255;
                        pixels[idx + 1] = 255;
                        pixels[idx + 2] = 255;
                        pixels[idx + 3] = 255;
                    }
                }
            }
        }

        glGenTextures(1, &fontTex);
        glBindTexture(GL_TEXTURE_2D, fontTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // 2D Text Shaders
        const char* vShader = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTex;
            layout (location = 2) in vec4 aColor;

            out vec2 vTex;
            out vec4 vColor;
            uniform vec2 uScreenSize;

            void main() {
                // Map pixel pos to NDC (-1..1)
                vec2 ndc = (aPos / uScreenSize) * 2.0 - 1.0;
                ndc.y = -ndc.y; // Flip Y
                gl_Position = vec4(ndc, 0.0, 1.0);
                vTex = aTex;
                vColor = aColor;
            }
        )";

        const char* fShader = R"(
            #version 330 core
            in vec2 vTex;
            in vec4 vColor;
            out vec4 FragColor;
            uniform sampler2D uFontTex;

            void main() {
                float a = texture(uFontTex, vTex).a;
                if (a < 0.1) discard;
                FragColor = vColor;
            }
        )";

        shader.compile(vShader, fShader);

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        // Position (2f), Tex (2f), Color (4f) = 8 floats per vertex
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));

        glBindVertexArray(0);
    }

    void renderText(const std::string& text, float x, float y, float scale, Vec3 color, float screenW, float screenH) {
        if (text.empty()) return;

        std::vector<float> verts;
        verts.reserve(text.size() * 48);
        float curX = x;
        float curY = y;
        float charW = 8.0f * scale;
        float charH = 8.0f * scale;

        for (char c : text) {
            if (c == '\n') {
                curX = x;
                curY += charH + 4.0f * scale;
                continue;
            }

            int ascii = static_cast<unsigned char>(c);
            if (ascii < 32 || ascii > 127) ascii = '?';
            int idx = ascii - 32;

            float u0 = (idx % 16) / 16.0f;
            float v0 = (idx / 16) / 8.0f;
            float u1 = u0 + (1.0f / 16.0f);
            float v1 = v0 + (1.0f / 8.0f);

            // Quad 2D vertices (2 triangles)
            float x0 = curX, y0 = curY;
            float x1 = curX + charW, y1 = curY + charH;

            // V0 (top-left)
            verts.insert(verts.end(), { x0, y0, u0, v0, color.x, color.y, color.z, 1.0f });
            // V1 (bottom-left)
            verts.insert(verts.end(), { x0, y1, u0, v1, color.x, color.y, color.z, 1.0f });
            // V2 (top-right)
            verts.insert(verts.end(), { x1, y0, u1, v0, color.x, color.y, color.z, 1.0f });

            // V3 (top-right)
            verts.insert(verts.end(), { x1, y0, u1, v0, color.x, color.y, color.z, 1.0f });
            // V4 (bottom-left)
            verts.insert(verts.end(), { x0, y1, u0, v1, color.x, color.y, color.z, 1.0f });
            // V5 (bottom-right)
            verts.insert(verts.end(), { x1, y1, u1, v1, color.x, color.y, color.z, 1.0f });

            curX += charW;
        }

        if (verts.empty()) return;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        shader.use();
        shader.setVec2("uScreenSize", screenW, screenH);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fontTex);
        shader.setInt("uFontTex", 0);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);

        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 8));

        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
    }
};

// Embedded bitmap font data
inline const uint8_t HUD::font8x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 ' '
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // 33 '!'
    {0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00}, // 34 '"'
    {0x66,0x66,0xFF,0x66,0xFF,0x66,0x66,0x00}, // 35 '#'
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // 36 '$'
    {0x62,0x66,0x0C,0x18,0x30,0x66,0x46,0x00}, // 37 '%'
    {0x3C,0x66,0x3C,0x38,0x67,0x66,0x3F,0x00}, // 38 '&'
    {0x06,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // 39 '\''
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // 40 '('
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // 41 ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // 42 '*'
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // 43 '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // 44 ','
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // 45 '-'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // 46 '.'
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // 47 '/'
    {0x3E,0x63,0x6B,0x6B,0x6B,0x63,0x3E,0x00}, // 48 '0'
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 49 '1'
    {0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0x00}, // 50 '2'
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // 51 '3'
    {0x0E,0x1E,0x36,0x66,0x7F,0x06,0x0F,0x00}, // 52 '4'
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // 53 '5'
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, // 54 '6'
    {0x7E,0x66,0x06,0x0C,0x18,0x18,0x18,0x00}, // 55 '7'
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 56 '8'
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, // 57 '9'
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // 58 ':'
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // 59 ';'
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // 60 '<'
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // 61 '='
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // 62 '>'
    {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}, // 63 '?'
    {0x3E,0x63,0x6F,0x69,0x6F,0x60,0x3E,0x00}, // 64 '@'
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, // 65 'A'
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // 66 'B'
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // 67 'C'
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // 68 'D'
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00}, // 69 'E'
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00}, // 70 'F'
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3B,0x00}, // 71 'G'
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // 72 'H'
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 73 'I'
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, // 74 'J'
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // 75 'K'
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // 76 'L'
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // 77 'M'
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // 78 'N'
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 79 'O'
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // 80 'P'
    {0x3C,0x66,0x66,0x66,0x66,0x3C,0x0E,0x00}, // 81 'Q'
    {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00}, // 82 'R'
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // 83 'S'
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 84 'T'
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 85 'U'
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // 86 'V'
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 87 'W'
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // 88 'X'
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // 89 'Y'
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // 90 'Z'
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // 91 '['
    {0x40,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // 92 '\'
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // 93 ']'
    {0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00}, // 94 '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // 95 '_'
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // 96 '`'
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, // 97 'a'
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // 98 'b'
    {0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00}, // 99 'c'
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, // 100 'd'
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, // 101 'e'
    {0x1C,0x30,0x78,0x30,0x30,0x30,0x30,0x00}, // 102 'f'
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x7C}, // 103 'g'
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, // 104 'h'
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // 105 'i'
    {0x06,0x00,0x06,0x06,0x06,0x06,0x66,0x3C}, // 106 'j'
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, // 107 'k'
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 108 'l'
    {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, // 109 'm'
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, // 110 'n'
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // 111 'o'
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, // 112 'p'
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, // 113 'q'
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, // 114 'r'
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, // 115 's'
    {0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00}, // 116 't'
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, // 117 'u'
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, // 118 'v'
    {0x00,0x00,0x63,0x63,0x6B,0x7F,0x36,0x00}, // 119 'w'
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, // 120 'x'
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x7C}, // 121 'y'
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, // 122 'z'
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // 123 '{'
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 124 '|'
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // 125 '}'
    {0x3B,0x6E,0x00,0x00,0x00,0x00,0x00,0x00}, // 126 '~'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}  // 127
};

#endif // HUD_HPP
```

## IWorldQuery.hpp

**Path:** `IWorldQuery.hpp` | **Lines:** 15 | **Size:** 360 bytes

```cpp
#ifndef IWORLD_QUERY_HPP
#define IWORLD_QUERY_HPP

#include "MathUtils.hpp"
#include "Block.hpp"
#include <cstdint>

class IWorldQuery {
public:
    virtual ~IWorldQuery() = default;
    virtual bool isBlockSolidAt(int64_t wx, int64_t wy, int64_t wz) const = 0;
    virtual BlockType getBlockAt(const IVec3& worldPos) const = 0;
};

#endif // IWORLD_QUERY_HPP
```

## LightingSystem.cpp

**Path:** `LightingSystem.cpp` | **Lines:** 233 | **Size:** 8659 bytes

```cpp
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
```

## LightingSystem.hpp

**Path:** `LightingSystem.hpp` | **Lines:** 40 | **Size:** 960 bytes

```cpp
#ifndef LIGHTING_SYSTEM_HPP
#define LIGHTING_SYSTEM_HPP

#include "Chunk.hpp"
#include <deque>
#include <memory>
#include <unordered_set>
#include <functional>

struct WorldLightNode {
    std::shared_ptr<Chunk> chunk;
    int8_t x;
    int8_t y;
    int8_t z;
};

class LightingSystem {
public:
    static void propagateLocalLight3D(Chunk& chunk);
    
    static void enqueueLightFace(
        const std::shared_ptr<Chunk>& chunk,
        int direction,
        std::deque<WorldLightNode>& lightQueue
    );

    static void enqueueBoundaryLight(
        const std::shared_ptr<Chunk>& chunk,
        std::deque<WorldLightNode>& lightQueue
    );

    static void propagateWorldLighting(
        std::deque<WorldLightNode>& lightQueue,
        std::unordered_set<Chunk*>& changedChunks,
        size_t maxNodesPerFrame,
        const std::function<std::shared_ptr<Chunk>(const Chunk&, int direction)>& getNeighborFunc
    );
};

#endif // LIGHTING_SYSTEM_HPP
```

## Main.cpp

**Path:** `Main.cpp` | **Lines:** 758 | **Size:** 32579 bytes

```cpp
#include <epoxy/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <chrono>
#include <cmath>

#include "MathUtils.hpp"
#include "Block.hpp"
#include "TextureAtlas.hpp"
#include "WorldGen.hpp"
#include "ChunkManager.hpp"
#include "Shader.hpp"
#include "Skybox.hpp"
#include "Camera.hpp"
#include "Physics.hpp"
#include "HUD.hpp"

// Global state for GLFW callbacks
Camera camera(Vec3(0.0f, 60.0f, 0.0f));
PhysicsController physics;
bool keys[1024] = { false };
bool firstMouse = true;
static std::atomic<bool> reloadShadersRequested{false};

float lastX = 640.0f, lastY = 360.0f;
bool cursorLocked = true;
bool showHUD = true;
bool showChunkBorders = false;
int diagMode = 0;

static GLenum drainOpenGLErrors() {
    GLenum firstError = GL_NO_ERROR;
    for (;;) {
        GLenum error = glGetError();
        if (error == GL_NO_ERROR) break;
        if (firstError == GL_NO_ERROR) firstError = error;
    }
    return firstError;
}
// Key callback
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode; (void)mods;
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            keys[key] = true;

            // Toggle Fly mode
            if (key == GLFW_KEY_F) {
                physics.isFlying = !physics.isFlying;
            }
            // Toggle HUD
            if (key == GLFW_KEY_H) {
                showHUD = !showHUD;
            }
            // Toggle Chunk Borders
            if (key == GLFW_KEY_B) {
                showChunkBorders = !showChunkBorders;
                std::cout << "Chunk Borders set to: " << (showChunkBorders ? "ON" : "OFF") << "\n";
            }
            if (key == GLFW_KEY_R) {
                reloadShadersRequested = true;
            }
            if (key >= GLFW_KEY_0 && key <= GLFW_KEY_6) {
                diagMode = key - GLFW_KEY_0;
                std::cout << "Diagnostic Shader Mode set to: " << diagMode << "\n";
            }
            if (key == GLFW_KEY_ESCAPE) {
                cursorLocked = !cursorLocked;
                glfwSetInputMode(window, GLFW_CURSOR, cursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            }
        } else if (action == GLFW_RELEASE) {
            keys[key] = false;
        }
    }
}

// Mouse movement callback
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    if (!cursorLocked) return;

    if (firstMouse) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - lastX;
    float yoffset = lastY - static_cast<float>(ypos); // Reversed since Y coordinates go from bottom to top

    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    camera.processMouseMovement(xoffset, yoffset);
}

int main(int argc, char** argv) {
    bool testMode = false;
    bool diagnosticsMode = false;
    bool staticTestMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--test") {
            testMode = true;
        }
        if (std::string(argv[i]) == "--diagnostics") {
            diagnosticsMode = true;
        }
        if (std::string(argv[i]) == "--static-test") {
            staticTestMode = true;
        }
        if (std::string(argv[i]) == "--borders") {
            showChunkBorders = true;
        }
    }
    diagnosticsMode = diagnosticsMode || staticTestMode;
    testMode = testMode || diagnosticsMode;

    std::cout << "Starting Infinite Voxel Renderer"
              << (staticTestMode ? " [STATIC TEST]" : (diagnosticsMode ? " [DIAGNOSTICS]" : (testMode ? " [TEST MODE]" : "")))
              << "...\n";

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // Keep the default framebuffer depth format compatible with the Hi-Z
    // depth copy used by glBlitFramebuffer.
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    int windowWidth = 1280;
    int windowHeight = 720;
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Infinite Voxel Engine (8192+ Render Distance)", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window!\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    GLint glMajor = 0;
    GLint glMinor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
    glGetIntegerv(GL_MINOR_VERSION, &glMinor);
    bool supportsOpenGL43 = glMajor > 4 || (glMajor == 4 && glMinor >= 3);
    bool supportsShaderDrawParameters =
        glfwExtensionSupported("GL_ARB_shader_draw_parameters") == GLFW_TRUE;
    bool supportsMultiDrawIndirect =
        supportsOpenGL43 || glfwExtensionSupported("GL_ARB_multi_draw_indirect") == GLFW_TRUE;
    if (!supportsOpenGL43 || !supportsMultiDrawIndirect || !supportsShaderDrawParameters) {
        std::cerr << "This renderer requires OpenGL 4.3 and GL_ARB_shader_draw_parameters. "
                  << "Detected OpenGL " << glMajor << "." << glMinor << ".\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwSwapInterval(0); // Disable VSync for unconstrained performance benchmarking
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    bool traversalFailure = false;
    { // Inner scope to ensure all OpenGL objects destruct while context is alive
        // Create procedural texture atlas
        GLuint atlasTexture = TextureAtlas::createProceduralAtlas();
        std::cout << "Texture atlas created (ID: " << atlasTexture << ")\n";

        // Main Voxel Shader
        const char* vShaderSrc = R"(
        #version 430 core
        #extension GL_ARB_shader_draw_parameters : require
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aTexCoord;
        layout (location = 3) in float aTexIndex;
        layout (location = 4) in float aAO;
        layout (location = 5) in vec3 aBlockRGB;
        layout (location = 6) in float aSkyLight;
        layout (location = 7) in float aWindWeight;

        out vec3 vNormal;
        out vec2 vTexCoord;
        flat out int vTexIndex;
        out float vAO;
        out vec3 vBlockRGB;
        out float vSkyLight;
        out float vDistance;
        out vec3 vWorldPosRelative;

        uniform mat4 uProjection;
        uniform mat4 uView;
        uniform vec3 uCameraWorldPos;
        uniform float uTime;

        struct SectionMetadata {
            vec4 chunkMinLod;
            vec4 sectionBounds;
        };
        layout(std430, binding = 0) readonly buffer SectionMetadataBuffer {
            SectionMetadata sections[];
        };

        void main() {
            SectionMetadata section = sections[gl_DrawIDARB];
            vec3 relPos = section.chunkMinLod.xyz + aPos;
            vec3 worldPos = uCameraWorldPos + relPos;

            // Subtle height-weighted motion for grass blades. The world-space
            // phase keeps adjacent chunks moving continuously across seams.
            float phase = uTime * 1.7 + dot(worldPos.xz, vec2(0.075, 0.055));
            vec2 wind = vec2(
                sin(phase) * 0.055 + sin(phase * 0.47 + worldPos.z * 0.08) * 0.018,
                cos(phase * 0.83) * 0.035 + cos(phase * 0.31 + worldPos.x * 0.06) * 0.012
            );
            relPos.xz += wind * aWindWeight;
            gl_Position = uProjection * uView * vec4(relPos, 1.0);

            vNormal = aNormal;
            vTexCoord = aTexCoord;
            vTexIndex = int(aTexIndex + 0.5);
            vAO = aAO;
            vBlockRGB = aBlockRGB;
            vSkyLight = aSkyLight;
            vDistance = length(relPos);
            
            vWorldPosRelative = relPos;
        }
    )";

    const char* fShaderSrc = R"(
        #version 430 core
        in vec3 vNormal;
        in vec2 vTexCoord;
        flat in int vTexIndex;
        in float vAO;
        in vec3 vBlockRGB;
        in float vSkyLight;
        in float vDistance;
            
        in vec3 vWorldPosRelative;

        out vec4 FragColor;

        uniform sampler2DArray uTextureAtlas;
        uniform vec3 uSunDir;
        uniform vec3 uSunColor;
        uniform vec3 uSkyAmbientColor;
        uniform vec3 uAbyssAmbientColor;
        uniform vec3 uSkyTint;
        uniform vec3 uFogColor;
        uniform vec3 uCameraWorldPos;
        uniform float uTime;
        uniform int uDiagMode;

        void main() {
            vec4 texColor = texture(uTextureAtlas, vec3(vTexCoord, float(vTexIndex)));
            if (texColor.a < 0.1) discard;
            // 1. Hemispheric Ambient Shading (Top vs Underside Glow)
            float hemiFactor = clamp(vNormal.y * 0.5 + 0.5, 0.0, 1.0);
            vec3 hemiAmbient = mix(uAbyssAmbientColor, uSkyAmbientColor, hemiFactor);

            // 2. Directional Sun Shading
            float diff = max(dot(vNormal, normalize(uSunDir)), 0.0);
            vec3 directSun = uSunColor * (diff * 0.65 + 0.35);

            // 3. Emissive RGB Block Light (Glow Crystals, Lava)
            vec3 emissiveRGB = vBlockRGB * 1.4;

            // 4. Subsurface Scattering Translucency & Rim Lighting for Foliage
            vec3 viewDir = normalize(-vWorldPosRelative);
            vec3 sunDirNorm = normalize(uSunDir);
            vec3 extraFoliageLight = vec3(0.0);
            bool isFoliage = vTexIndex == 7 || vTexIndex == 13 ||
                vTexIndex == 16 || vTexIndex == 19 ||
                vTexIndex == 22 || vTexIndex == 23 ||
                vTexIndex == 26 || vTexIndex == 27 ||
                (vTexIndex >= 29 && vTexIndex <= 57);
            if (isFoliage) {
                float backLighting = max(0.0, dot(-viewDir, sunDirNorm));
                float sss = pow(backLighting, 3.0) * 0.65;
                float rim = pow(1.0 - max(0.0, dot(viewDir, vNormal)), 3.5) * 0.25;
                extraFoliageLight = uSunColor * (sss + rim);
            }

            // 5. Shader-only Ice Biome Ambient (Replaces propagated cyan light for Sky Quartz)
            vec3 worldPos = uCameraWorldPos + vWorldPosRelative;
            float highSkyBiome = smoothstep(282.0, 318.0, worldPos.y);
            float daylight = smoothstep(-0.12, 0.25, sunDirNorm.y);
            float smoothAO = mix(0.35, 1.0, vAO);
            float sky = clamp(vSkyLight, 0.0, 1.0);
            float exposedAmount = mix(0.18, 1.0, sky) * mix(0.45, 1.0, smoothAO);

            vec3 iceBounceColor = vec3(0.07, 0.28, 0.48);
            vec3 iceBiomeAmbient = iceBounceColor * highSkyBiome * exposedAmount * mix(0.12, 1.0, daylight);

            bool isSnow = (vTexIndex == 59);
            if (isSnow) {
                iceBiomeAmbient = vec3(0.0);
            }

            // 6. Snow High-Albedo & Sheen
            vec3 snowFill = vec3(0.0);
            if (isSnow) {
                float sunFacing = max(dot(vNormal, sunDirNorm), 0.0);
                float snowSun = sunFacing * sky * daylight;
                snowFill = vec3(0.14) * sky + uSunColor * snowSun * 0.35;
            }

            // Total Combined Surface Illumination
            vec3 totalLight =
                hemiAmbient * mix(0.12, 1.0, sky) +
                directSun * 0.8 * sky +
                emissiveRGB +
                extraFoliageLight +
                iceBiomeAmbient;

            if (isSnow) {
                totalLight += snowFill;
            }

            // Linear Contact Ambient Occlusion
            vec3 baseColor = texColor.rgb * totalLight * smoothAO;

            // View-dependent Snow Sheen
            if (isSnow) {
                vec3 reflectedSun = reflect(-sunDirNorm, vNormal);
                float snowSheen = pow(max(dot(reflectedSun, viewDir), 0.0), 24.0) * sky * daylight;
                baseColor += uSunColor * snowSheen * 0.18;
            }
            // Atmospheric Fog blending distant islands into sky color
            float fogStart = 2000.0;
            float fogEnd = 4608.0;
            float fogFactor = smoothstep(fogStart, fogEnd, vDistance);
            if (uDiagMode == 1) {
                FragColor = vec4(texColor.rgb, 1.0);
            } else if (uDiagMode == 2) {
                FragColor = vec4(vec3(smoothAO), 1.0);
            } else if (uDiagMode == 3) {
                FragColor = vec4(totalLight, 1.0);
            } else if (uDiagMode == 4) {
                FragColor = vec4(vNormal * 0.5 + 0.5, 1.0);
            } else if (uDiagMode == 5) {
                FragColor = vec4(vec3(vAO), 1.0);
            } else if (uDiagMode == 6) {
                FragColor = vec4(vec3(0.6) * totalLight, 1.0);
            } else {
                FragColor = vec4(mix(baseColor, uFogColor, fogFactor), texColor.a);
            }
        }
    )";

    Shader voxelShader;
    if (!voxelShader.compileFromFile("assets/shaders/voxel.vert", "assets/shaders/voxel.frag")) {
        std::cerr << "Falling back to inline voxel shaders...\n";
        voxelShader.compile(vShaderSrc, fShaderSrc);
    }
    // Initialize Skybox & HUD
    Skybox skybox;
    skybox.init();

    HUD hud;
    hud.init();
    // Initialize Chunk Border Renderer
    ChunkBorderRenderer borderRenderer;
    borderRenderer.init();


    // Chunk Manager
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
    ChunkManager chunkMgr(windowWidth, windowHeight);
    chunkMgr.setDiagnosticsEnabled(diagnosticsMode);
    // Run boundary source test at local X=31
    chunkMgr.runBoundarySourceTest();

    // Timing
    float lastFrameTime = static_cast<float>(glfwGetTime());
    int frameCount = 0;
    float fpsTimer = 0.0f;
    float currentFPS = 60.0f;

    // Day/Night Cycle time
    float dayTime = 0.5f; // 0.0 to 1.0

    std::cout << "Engine ready! Entering main render loop...\n";
    int testFrameCount = 0;
    double totalFrameTimeMs = 0.0;
    int benchmarkFrameCount = 0;
    const std::string diagnosticsPath = staticTestMode
        ? "/tmp/infinite_static_diagnostics.csv"
        : "/tmp/infinite_gpu_diagnostics.csv";
    const double diagnosticsDurationSeconds = staticTestMode ? 40.0 : 30.0;
    std::ofstream diagnosticsFile;
    double diagnosticsStartTime = 0.0;
    int diagnosticsCompletedFrames = 0;
    if (diagnosticsMode) {
        diagnosticsFile.open(diagnosticsPath, std::ios::out | std::ios::trunc);
        if (!diagnosticsFile) {
            std::cerr << "Failed to open diagnostics report: " << diagnosticsPath << "\n";
            traversalFailure = true;
        } else {
            diagnosticsFile << "frame,elapsed_s,update_ms,traversal_ms,hud_ms,swap_ms,readback_ms,total_to_swap_ms,total_loop_ms,loaded_chunks,uploaded_meshes,pending_tasks,nodes,roots,candidate_indices,largest_candidate,emitted_commands,emitted_indices,largest_emitted,cpu_traversal,command_valid,gl_error,gl_start,gl_update,gl_traversal,gl_hud,gl_swap,gl_readback\n";
            diagnosticsStartTime = glfwGetTime();
        }
    }

    while (!glfwWindowShouldClose(window)) {
        if (diagnosticsMode && glfwGetTime() - diagnosticsStartTime >= diagnosticsDurationSeconds) break;
        auto frameStart = std::chrono::high_resolution_clock::now();
        GLenum frameStartGlError = diagnosticsMode ? drainOpenGLErrors() : GL_NO_ERROR;

        float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - lastFrameTime;
        if (deltaTime <= 0.0f) deltaTime = 0.016f;
        lastFrameTime = currentFrame;

        frameCount++;
        fpsTimer += deltaTime;
        if (fpsTimer >= 0.5f) {
            currentFPS = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        if (testMode && !staticTestMode) {
            testFrameCount++;
            // Keep the benchmark's world-space speed independent of FPS.
            // A fixed distance per rendered frame creates a feedback loop:
            // faster rendering moves farther, queues more chunks, then slows
            // down as the queue grows.
            camera.position.x += 150.0f * deltaTime;
            camera.position.y += 10.0f * deltaTime;
            camera.position.z += 150.0f * deltaTime;
        }
        glfwPollEvents();
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
        if (windowWidth < 1) windowWidth = 1;
        if (windowHeight < 1) windowHeight = 1;
        float aspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);

        // Update Physics & Camera
        bool superSpeed = keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_TAB];
        auto updateStart = std::chrono::high_resolution_clock::now();
        physics.update(camera, chunkMgr, keys, superSpeed, deltaTime);

        // Update Chunk Manager (loads/unloads & queues multithreaded LOD chunks)
        chunkMgr.update(camera.position);
        auto updateEnd = std::chrono::high_resolution_clock::now();
        GLenum updateGlError = diagnosticsMode ? drainOpenGLErrors() : GL_NO_ERROR;

        // Day/Night Sun direction calculations
        dayTime += deltaTime * 0.005f; // Slow day cycle
        float sunAngle = dayTime * 2.0f * PI;
        Vec3 sunDir(std::cos(sunAngle), std::sin(sunAngle), 0.3f);
        sunDir = sunDir.normalized();

        // each five-hundred-block band gets its own atmosphere.
        float camY = camera.position.y;
        int negativeLayer = WorldGen::getNegativeLayerIndex(
            static_cast<int64_t>(std::floor(camY))
        );

        Vec3 sunColor(1.0f, 0.95f, 0.85f);
        Vec3 skyTop;
        Vec3 skyHorizon;
        Vec3 skyAmbientColor;
        Vec3 abyssAmbientColor;

        float surfaceLayerBlend = WorldGen::getSurfaceLayerBlend(camY);

        Vec3 surfaceSkyTop(0.015f, 0.22f, 0.18f);
        Vec3 surfaceSkyHorizon(0.008f, 0.065f, 0.075f);
        Vec3 surfaceSkyAmbient(0.12f, 0.42f, 0.34f);
        Vec3 surfaceAbyssAmbient(0.008f, 0.08f, 0.09f);

        float altitudeFactor = std::clamp(
            (camY + 150.0f) / 600.0f,
            0.0f,
            1.0f
        );
        Vec3 floatingSkyTop(0.2f, 0.5f, 0.9f);
        Vec3 floatingSkyHorizon = Vec3::lerp(
            Vec3(0.35f, 0.32f, 0.55f),
            Vec3(0.70f, 0.85f, 1.00f),
            altitudeFactor
        );
        Vec3 floatingSkyAmbient = Vec3::lerp(
            Vec3(0.35f, 0.45f, 0.65f),
            Vec3(0.55f, 0.72f, 0.95f),
            altitudeFactor
        );
        Vec3 floatingAbyssAmbient = Vec3::lerp(
            Vec3(0.10f, 0.08f, 0.18f),
            Vec3(0.22f, 0.25f, 0.35f),
            altitudeFactor
        );

        skyTop = Vec3::lerp(surfaceSkyTop, floatingSkyTop, surfaceLayerBlend);
        skyHorizon = Vec3::lerp(surfaceSkyHorizon, floatingSkyHorizon, surfaceLayerBlend);
        skyAmbientColor = Vec3::lerp(surfaceSkyAmbient, floatingSkyAmbient, surfaceLayerBlend);
        abyssAmbientColor = Vec3::lerp(surfaceAbyssAmbient, floatingAbyssAmbient, surfaceLayerBlend);

        Vec3 fogColor = skyHorizon;
        // night only dims the chosen layer palette.
        if (sunDir.y < 0.0f) {
            float nightFactor = std::clamp(-sunDir.y * 2.0f, 0.0f, 1.0f);
            skyTop = Vec3::lerp(
                skyTop,
                Vec3(0.02f, 0.03f, 0.08f),
                nightFactor
            );
            skyHorizon = Vec3::lerp(
                skyHorizon,
                Vec3(0.05f, 0.08f, 0.15f),
                nightFactor
            );
            surfaceSkyHorizon = Vec3::lerp(
                surfaceSkyHorizon,
                Vec3(0.035f, 0.055f, 0.10f),
                nightFactor
            );
            floatingSkyHorizon = Vec3::lerp(
                floatingSkyHorizon,
                Vec3(0.065f, 0.105f, 0.19f),
                nightFactor
            );

            skyAmbientColor = Vec3::lerp(
                skyAmbientColor,
                Vec3(0.1f, 0.12f, 0.2f),
                nightFactor
            );
            abyssAmbientColor = Vec3::lerp(
                abyssAmbientColor,
                Vec3(0.05f, 0.06f, 0.1f),
                nightFactor
            );
            sunColor = Vec3::lerp(
                sunColor,
                Vec3(0.2f, 0.2f, 0.3f),
                nightFactor
            );
        }
        fogColor = skyHorizon;

        if (reloadShadersRequested.exchange(false)) {
            std::cout << "[HOT-RELOAD] Reloading shaders from disk...\n";
            if (voxelShader.compileFromFile("assets/shaders/voxel.vert", "assets/shaders/voxel.frag")) {
                std::cout << "[HOT-RELOAD] Voxel shader recompiled successfully!\n";
            }
            skybox.init();
        }
        // Render pass setup
        glViewport(0, 0, windowWidth, windowHeight);
        glClearColor(fogColor.x, fogColor.y, fogColor.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // View and Projection matrices
        Mat4 projection = camera.getProjectionMatrix(aspect);
        Mat4 view = camera.getViewMatrix();
        Mat4 viewProjection = projection * view;
        Frustum frustum = Frustum::extract(viewProjection);

        // 1. Render Skybox
        skybox.draw(
            projection,
            view,
            sunDir,
            sunColor,
            skyTop,
            skyHorizon,
            surfaceSkyHorizon,
            floatingSkyHorizon
        );

        // 2. Render Voxel Chunks
        voxelShader.use();
        voxelShader.setMat4("uProjection", projection);
        voxelShader.setMat4("uView", view);
        voxelShader.setVec3("uSunDir", sunDir);
        voxelShader.setVec3("uSunColor", sunColor);
        voxelShader.setVec3("uSkyAmbientColor", skyAmbientColor);
        voxelShader.setVec3("uAbyssAmbientColor", abyssAmbientColor);
        voxelShader.setVec3("uSkyTint", skyHorizon);
        voxelShader.setVec3("uFogColor", fogColor);
        voxelShader.setVec3("uCameraWorldPos", camera.position);
        voxelShader.setFloat("uTime", currentFrame);
        voxelShader.setInt("uDiagMode", diagMode);
        glBindTexture(GL_TEXTURE_2D_ARRAY, atlasTexture);
        voxelShader.setInt("uTextureAtlas", 0);

        auto traversalStart = std::chrono::high_resolution_clock::now();
        if (!chunkMgr.render(
            frustum,
            camera.position,
            camera.fov * DEG2RAD,
            windowWidth,
            windowHeight,
            voxelShader.programID,
            projection.m[5],
            view,
            viewProjection
        )) {
            traversalFailure = true;
            std::cerr << "Renderer failed because its geometry arena is unavailable.\n";
            break;
        }
        auto traversalEnd = std::chrono::high_resolution_clock::now();
        GLenum traversalGlError = diagnosticsMode ? drainOpenGLErrors() : GL_NO_ERROR;
        // 2.5 Render 32x32 Chunk Borders
        if (showChunkBorders) {
            chunkMgr.renderChunkBorders(borderRenderer, camera.position, projection, view);
        }


        // 3. Render HUD UI Overlay
        auto hudStart = std::chrono::high_resolution_clock::now();
        if (showHUD) {
            int totalChunks = 0, uploadedMeshes = 0, pendingTasks = 0;
            chunkMgr.getStats(totalChunks, uploadedMeshes, pendingTasks);

            std::stringstream ss1, ss2, ss3, ss4, ss5, ss6, ss7;
            ss1 << "INFINITE VOXEL ENGINE (LOD 0..4 RENDER DISTANCE)";
            ss2 << "FPS: " << static_cast<int>(currentFPS) << " | Frame Time: " << std::fixed << std::setprecision(1) << (1000.0f / currentFPS) << " ms";
            ss3 << "XYZ: " << std::fixed << std::setprecision(1) << camera.position.x << " / " << camera.position.y << " / " << camera.position.z;
            ss4 << "Effective Render Distance: ~4,608 blocks (LODs 0..4)";
            ss5 << "Chunks: " << totalChunks << " loaded | Meshes: " << uploadedMeshes << " | Queued Tasks: " << pendingTasks;
            ss6 << "Mode: " << (physics.isFlying ? "FLYING (WASD + Space/Shift)" : "WALKING (Physics Collision)")
                << (superSpeed ? " [SUPER SPEED 160m/s]" : "") << " | [F] Fly | [B] Borders (" << (showChunkBorders ? "ON" : "OFF") << ") | [H] HUD | [0-6] Diag (" << diagMode << ")";
            ss7 << "Biome: " << WorldGen::getNegativeLayerName(negativeLayer)
                << " | " << WorldGen::NEGATIVE_LAYER_COUNT
                << " layers x " << WorldGen::NEGATIVE_LAYER_HEIGHT << " blocks";
            hud.renderText(ss2.str(), 16.0f, 44.0f, 1.5f, Vec3(0.3f, 1.0f, 0.4f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss3.str(), 16.0f, 68.0f, 1.5f, Vec3(0.9f, 0.9f, 0.9f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss4.str(), 16.0f, 92.0f, 1.5f, Vec3(0.4f, 0.8f, 1.0f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss5.str(), 16.0f, 116.0f, 1.4f, Vec3(0.8f, 0.8f, 0.8f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss6.str(), 16.0f, 140.0f, 1.4f, Vec3(1.0f, 0.5f, 0.2f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss7.str(), 16.0f, 164.0f, 1.4f, Vec3(0.6f, 0.9f, 1.0f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
        }
        auto hudEnd = std::chrono::high_resolution_clock::now();
        GLenum hudGlError = diagnosticsMode ? drainOpenGLErrors() : GL_NO_ERROR;

        auto swapStart = std::chrono::high_resolution_clock::now();
        glfwSwapBuffers(window);
        auto swapEnd = std::chrono::high_resolution_clock::now();
        chunkMgr.markGpuWorkSubmitted();
        GLenum swapGlError = diagnosticsMode ? drainOpenGLErrors() : GL_NO_ERROR;

        auto frameEnd = std::chrono::high_resolution_clock::now();
        double frameTimeMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
        auto readbackStart = std::chrono::high_resolution_clock::now();
        // The static performance test samples indirect commands once per
        // second. Per-frame glGetBufferSubData is a synchronous diagnostic
        // readback and can eventually crash the NVIDIA driver without
        // improving the frame-time measurement.
        if (!staticTestMode || (testFrameCount % 120 == 0)) {
            chunkMgr.sampleGpuCommandDiagnostics();
        }
        auto readbackEnd = std::chrono::high_resolution_clock::now();
        GLenum readbackGlError = diagnosticsMode ? drainOpenGLErrors() : GL_NO_ERROR;

        if (diagnosticsMode && diagnosticsFile) {
            int totalChunks = 0, uploadedMeshes = 0, pendingTasks = 0;
            chunkMgr.getStats(totalChunks, uploadedMeshes, pendingTasks);
            const ChunkManager::FrameDiagnostics& frameDiagnostics = chunkMgr.getFrameDiagnostics();
            GLenum glError = frameStartGlError != GL_NO_ERROR ? frameStartGlError :
                (updateGlError != GL_NO_ERROR ? updateGlError :
                (traversalGlError != GL_NO_ERROR ? traversalGlError :
                (hudGlError != GL_NO_ERROR ? hudGlError :
                (swapGlError != GL_NO_ERROR ? swapGlError : readbackGlError))));
            double elapsedSeconds = glfwGetTime() - diagnosticsStartTime;
            diagnosticsFile << frameDiagnostics.frameIndex << ','
                << elapsedSeconds << ','
                << std::chrono::duration<double, std::milli>(updateEnd - updateStart).count() << ','
                << std::chrono::duration<double, std::milli>(traversalEnd - traversalStart).count() << ','
                << std::chrono::duration<double, std::milli>(hudEnd - hudStart).count() << ','
                << std::chrono::duration<double, std::milli>(swapEnd - swapStart).count() << ','
                << std::chrono::duration<double, std::milli>(readbackEnd - readbackStart).count() << ','
                << std::chrono::duration<double, std::milli>(swapEnd - frameStart).count() << ','
                << frameTimeMs << ','
                << totalChunks << ',' << uploadedMeshes << ',' << pendingTasks << ','
                << frameDiagnostics.nodeCount << ',' << frameDiagnostics.rootCount << ','
                << frameDiagnostics.candidateIndices << ',' << frameDiagnostics.largestCandidate << ','
                << frameDiagnostics.emittedCommands.nonZeroCommands << ','
                << frameDiagnostics.emittedCommands.totalIndices << ','
                << frameDiagnostics.emittedCommands.maxIndices << ','
                << (frameDiagnostics.cpuTraversalUsed ? 1 : 0) << ','
                << (frameDiagnostics.commandPayloadValid ? 1 : 0) << ','
                << static_cast<unsigned int>(glError) << ','
                << static_cast<unsigned int>(frameStartGlError) << ','
                << static_cast<unsigned int>(updateGlError) << ','
                << static_cast<unsigned int>(traversalGlError) << ','
                << static_cast<unsigned int>(hudGlError) << ','
                << static_cast<unsigned int>(swapGlError) << ','
                << static_cast<unsigned int>(readbackGlError) << '\n';
            diagnosticsFile.flush();
            ++diagnosticsCompletedFrames;
        }
        if (testFrameCount > 30) { // Skip first 30 warmup frames
            totalFrameTimeMs += frameTimeMs;
            benchmarkFrameCount++;
        }

        if (testMode && !diagnosticsMode && testFrameCount >= 600) {
            int totalChunks = 0, uploadedMeshes = 0, pendingTasks = 0;
            chunkMgr.getStats(totalChunks, uploadedMeshes, pendingTasks);
            double avgFrameTimeMs = benchmarkFrameCount > 0 ? (totalFrameTimeMs / benchmarkFrameCount) : 0.0;
            double avgFPS = avgFrameTimeMs > 0.0 ? (1000.0 / avgFrameTimeMs) : 0.0;

            std::cout << "\n=== BENCHMARK & VERIFICATION SUMMARY ===\n";
            std::cout << "Average Frame Render Time: " << std::fixed << std::setprecision(3) << avgFrameTimeMs << " ms (" << std::setprecision(1) << avgFPS << " FPS)\n";
            std::cout << "Target (< 5.0 ms): " << (avgFrameTimeMs < 5.0 ? "PASS" : "FAIL - Optimization Needed") << "\n";
            std::cout << "Stats: Loaded Chunks=" << totalChunks << ", Active Meshes=" << uploadedMeshes << ", Camera XYZ=("
                      << camera.position.x << ", " << camera.position.y << ", " << camera.position.z << ")\n";
            uint64_t count = chunkMgr.chunksProcessed.load();
            double genMs = count > 0 ? (chunkMgr.totalGenTimeUs.load() / 1000.0 / count) : 0;
            double meshMs = count > 0 ? (chunkMgr.totalMeshTimeUs.load() / 1000.0 / count) : 0;
            std::cout << "Chunk Worker Stats: Chunks Processed=" << count << " | Avg Gen Time=" << genMs << " ms | Avg Mesh Time=" << meshMs << " ms\n";

            // Save Framebuffer Screenshot to PPM
            std::vector<unsigned char> pixels(windowWidth * windowHeight * 3);
            glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            std::ofstream ppm("screenshot.ppm", std::ios::binary);
            ppm << "P6\n" << windowWidth << " " << windowHeight << "\n255\n";
            for (int row = windowHeight - 1; row >= 0; --row) {
                ppm.write(reinterpret_cast<char*>(&pixels[row * windowWidth * 3]), windowWidth * 3);
            }
            ppm.close();
            std::cout << "Saved screenshot.ppm\n";
            break;
        }
    }
    if (diagnosticsMode) {
        if (diagnosticsFile) {
            diagnosticsFile << "# completed_frames," << diagnosticsCompletedFrames << '\n';
            diagnosticsFile.close();
        }
        std::cout << "Diagnostics report: " << diagnosticsPath
                  << " (completed frames=" << diagnosticsCompletedFrames << ")\n";
    }
        glDeleteTextures(1, &atlasTexture);
    } // End inner scope (all GL shaders, meshes, textures, chunk manager destructed here while context is active)
    std::cout << "Shutting down renderer...\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return traversalFailure ? -1 : 0;
}
```

## MathUtils.hpp

**Path:** `MathUtils.hpp` | **Lines:** 217 | **Size:** 6954 bytes

```cpp
#ifndef MATH_UTILS_HPP
#define MATH_UTILS_HPP

#include <cmath>
#include <algorithm>
#include <array>
#include <iostream>

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;

struct Vec3 {
    float x, y, z;

    constexpr Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    constexpr Vec3(float s) : x(s), y(s), z(s) {}
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& r) const { return Vec3(x + r.x, y + r.y, z + r.z); }
    Vec3 operator-(const Vec3& r) const { return Vec3(x - r.x, y - r.y, z - r.z); }
    Vec3 operator*(const Vec3& r) const { return Vec3(x * r.x, y * r.y, z * r.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    Vec3& operator+=(const Vec3& r) { x += r.x; y += r.y; z += r.z; return *this; }
    Vec3& operator-=(const Vec3& r) { x -= r.x; y -= r.y; z -= r.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }

    Vec3 normalized() const {
        float len = length();
        if (len > 1e-6f) return *this / len;
        return Vec3(0, 0, 0);
    }

    static float dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return Vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    static Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
        return a * (1.0f - t) + b * t;
    }
};

struct Vec4 {
    float x, y, z, w;

    constexpr Vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    constexpr Vec4(float s) : x(s), y(s), z(s), w(s) {}
    constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

struct IVec3 {
    int64_t x, y, z;

    constexpr IVec3() : x(0), y(0), z(0) {}
    constexpr IVec3(int64_t x_, int64_t y_, int64_t z_) : x(x_), y(y_), z(z_) {}

    bool operator==(const IVec3& r) const { return x == r.x && y == r.y && z == r.z; }
    bool operator!=(const IVec3& r) const { return !(*this == r); }
    bool operator<(const IVec3& r) const {
        if (x != r.x) return x < r.x;
        if (y != r.y) return y < r.y;
        return z < r.z;
    }

    IVec3 operator+(const IVec3& r) const { return IVec3(x + r.x, y + r.y, z + r.z); }
    IVec3 operator-(const IVec3& r) const { return IVec3(x - r.x, y - r.y, z - r.z); }
    IVec3 operator*(int64_t s) const { return IVec3(x * s, y * s, z * s); }
};

struct IVec3Hash {
    std::size_t operator()(const IVec3& v) const noexcept {
        std::size_t h1 = std::hash<int64_t>{}(v.x);
        std::size_t h2 = std::hash<int64_t>{}(v.y);
        std::size_t h3 = std::hash<int64_t>{}(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct Mat4 {
    float m[16];

    Mat4() {
        std::fill(m, m + 16, 0.0f);
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static Mat4 identity() {
        return Mat4();
    }

    static Mat4 perspective(float fovRad, float aspect, float nearVal, float farVal) {
        Mat4 r;
        std::fill(r.m, r.m + 16, 0.0f);
        float tanHalfFov = std::tan(fovRad / 2.0f);
        r.m[0] = 1.0f / (aspect * tanHalfFov);
        r.m[5] = 1.0f / tanHalfFov;
        r.m[10] = -(farVal + nearVal) / (farVal - nearVal);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * farVal * nearVal) / (farVal - nearVal);
        r.m[15] = 0.0f;
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalized();
        Vec3 s = Vec3::cross(f, up).normalized();
        Vec3 u = Vec3::cross(s, f);

        Mat4 r;
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;  r.m[12] = -Vec3::dot(s, eye);
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;  r.m[13] = -Vec3::dot(u, eye);
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] = Vec3::dot(f, eye);
        r.m[3] = 0.0f; r.m[7] = 0.0f; r.m[11] = 0.0f; r.m[15] = 1.0f;
        return r;
    }

    Mat4 operator*(const Mat4& r) const {
        Mat4 out;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                out.m[row + col * 4] =
                    m[row + 0 * 4] * r.m[0 + col * 4] +
                    m[row + 1 * 4] * r.m[1 + col * 4] +
                    m[row + 2 * 4] * r.m[2 + col * 4] +
                    m[row + 3 * 4] * r.m[3 + col * 4];
            }
        }
        return out;
    }
};

struct Plane {
    Vec3 normal;
    float d;

    Plane() : normal(0, 1, 0), d(0) {}
    Plane(float a, float b, float c, float d_) : normal(a, b, c), d(d_) {
        float len = normal.length();
        if (len > 1e-6f) {
            normal = normal / len;
            d = d_ / len;
        }
    }

    float distance(const Vec3& p) const {
        return Vec3::dot(normal, p) + d;
    }
};

struct Frustum {
    Plane planes[6];

    static Frustum extract(const Mat4& vp) {
        Frustum f;
        const float* m = vp.m;

        // Left
        f.planes[0] = Plane(m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]);
        // Right
        f.planes[1] = Plane(m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]);
        // Bottom
        f.planes[2] = Plane(m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]);
        // Top
        f.planes[3] = Plane(m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]);
        // Near
        f.planes[4] = Plane(m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]);
        // Far
        f.planes[5] = Plane(m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]);

        return f;
    }

    bool intersectsAABB(const Vec3& minP, const Vec3& maxP) const {
        for (int i = 0; i < 6; ++i) {
            const Plane& pl = planes[i];
            float px = (pl.normal.x >= 0.0f) ? maxP.x : minP.x;
            float py = (pl.normal.y >= 0.0f) ? maxP.y : minP.y;
            float pz = (pl.normal.z >= 0.0f) ? maxP.z : minP.z;

            if (pl.normal.x * px + pl.normal.y * py + pl.normal.z * pz + pl.d < 0.0f) {
                return false;
            }
        }
        return true;
    }
};
inline constexpr int64_t floorDiv(int64_t a, int64_t b) {
    int64_t res = a / b;
    int64_t rem = a % b;
    if (rem != 0 && ((a < 0) ^ (b < 0))) {
        --res;
    }
    return res;
}

inline constexpr int worldToLocalVoxel(int64_t worldCoord, int chunkSize) {
    int64_t chunkIdx = floorDiv(worldCoord, chunkSize);
    return static_cast<int>(worldCoord - chunkIdx * chunkSize);
}

#endif // MATH_UTILS_HPP
```

## Mesh.hpp

**Path:** `Mesh.hpp` | **Lines:** 528 | **Size:** 19546 bytes

```cpp
#ifndef MESH_HPP
#define MESH_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>
#include <epoxy/gl.h>
#include <GLFW/glfw3.h>
#include "MathUtils.hpp"

struct VoxelVertex {
    float x, y, z;        // Position relative to the section origin
    float nx, ny, nz;     // Normal
    float u, v;           // UV Texture Coords
    float texIndex;       // Texture Atlas ID
    float ao;             // Ambient Occlusion (0.2 - 1.0)
    float lightR, lightG, lightB; // Emissive RGB Block Light
    float skyLight;       // 3D Sky Light (0.0 - 1.0)
    float windWeight;     // 0 at the ground, 1 at the tip of wind-animated plants
};

struct GeometryHandle {
    size_t vertexOffset = 0;
    size_t indexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    int32_t baseVertex = 0;
    size_t gpuBytes = 0;
    bool valid = false;
};

struct DrawElementsIndirectCommand {
    uint32_t count = 0;
    uint32_t instanceCount = 1;
    uint32_t firstIndex = 0;
    int32_t baseVertex = 0;
    uint32_t baseInstance = 0;
};

struct IndirectCommandDiagnostics {
    uint64_t nonZeroCommands = 0;
    uint64_t totalIndices = 0;
    uint32_t maxIndices = 0;
};

struct alignas(16) SectionGpuMetadata {
    float chunkMinLod[4] = {}; // camera-relative min xyz, lod
    float sectionBounds[4] = {}; // world size, reserved
};

static_assert(sizeof(DrawElementsIndirectCommand) == 20, "Indirect command layout must match OpenGL");

class GeometryArena {
private:
    struct FreeRange {
        size_t start = 0;
        size_t count = 0;
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    static constexpr size_t NUM_FRAME_CONTEXTS = 3;
    struct FrameContext {
        GLsync fence = 0;
        GLuint metadataBuffer = 0;
        GLuint indirectBuffer = 0;
        size_t metadataCapacityBytes = 0;
        size_t indirectCapacityBytes = 0;
        std::vector<GeometryHandle> retiredGeometry;
    };
    FrameContext frameContexts[NUM_FRAME_CONTEXTS];
    size_t frameIndex = 0;
    bool initialized = false;
    bool frameContextReady = true;
    size_t lastDrawCommandCount = 0;

    size_t vertexCapacity = 0;
    size_t indexCapacity = 0;
    std::vector<FreeRange> freeVertexRanges;
    std::vector<FreeRange> freeIndexRanges;
    size_t metadataCapacityBytes = 0;
    size_t indirectCapacityBytes = 0;

    static size_t allocateRange(std::vector<FreeRange>& ranges, size_t count) {
        size_t best = std::numeric_limits<size_t>::max();
        size_t bestCount = std::numeric_limits<size_t>::max();
        for (size_t i = 0; i < ranges.size(); ++i) {
            if (ranges[i].count >= count && ranges[i].count < bestCount) {
                best = i;
                bestCount = ranges[i].count;
            }
        }
        if (best == std::numeric_limits<size_t>::max()) return best;

        size_t start = ranges[best].start;
        ranges[best].start += count;
        ranges[best].count -= count;
        if (ranges[best].count == 0) ranges.erase(ranges.begin() + static_cast<std::ptrdiff_t>(best));
        return start;
    }

    static void releaseRange(std::vector<FreeRange>& ranges, size_t start, size_t count) {
        if (count == 0) return;
        ranges.push_back({start, count});
        std::sort(ranges.begin(), ranges.end(), [](const FreeRange& a, const FreeRange& b) {
            return a.start < b.start;
        });

        std::vector<FreeRange> merged;
        merged.reserve(ranges.size());
        for (const FreeRange& range : ranges) {
            if (range.count == 0) continue;
            if (!merged.empty() &&
                merged.back().start + merged.back().count >= range.start) {
                size_t end = std::max(
                    merged.back().start + merged.back().count,
                    range.start + range.count
                );
                merged.back().count = end - merged.back().start;
            } else {
                merged.push_back(range);
            }
        }
        ranges.swap(merged);
    }

    void configureVertexArray() {
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, x));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, nx));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, u));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, texIndex));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, ao));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, lightR));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, skyLight));
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, windWeight));
        glBindVertexArray(0);
    }

    bool growVertexStorage(size_t required) {
        size_t oldCapacity = vertexCapacity;
        if (oldCapacity > std::numeric_limits<size_t>::max() / 2) return false;
        size_t newCapacity = std::max(required, oldCapacity * 2);

        GLuint oldVbo = vbo;
        GLuint newVbo = 0;
        glGenBuffers(1, &newVbo);
        glBindBuffer(GL_COPY_WRITE_BUFFER, newVbo);
        while (glGetError() != GL_NO_ERROR);

        glBufferData(
            GL_COPY_WRITE_BUFFER,
            static_cast<GLsizeiptr>(newCapacity * sizeof(VoxelVertex)),
            nullptr,
            GL_DYNAMIC_DRAW
        );
        if (glGetError() != GL_NO_ERROR) {
            glDeleteBuffers(1, &newVbo);
            return false;
        }

        glBindBuffer(GL_COPY_READ_BUFFER, oldVbo);
        glCopyBufferSubData(
            GL_COPY_READ_BUFFER,
            GL_COPY_WRITE_BUFFER,
            0,
            0,
            static_cast<GLsizeiptr>(oldCapacity * sizeof(VoxelVertex))
        );
        if (glGetError() != GL_NO_ERROR) {
            glDeleteBuffers(1, &newVbo);
            return false;
        }

        vbo = newVbo;
        vertexCapacity = newCapacity;
        releaseRange(freeVertexRanges, oldCapacity, newCapacity - oldCapacity);
        configureVertexArray();
        glDeleteBuffers(1, &oldVbo);
        return true;
    }

    bool growIndexStorage(size_t required) {
        size_t oldCapacity = indexCapacity;
        if (oldCapacity > std::numeric_limits<size_t>::max() / 2) return false;
        size_t newCapacity = std::max(required, oldCapacity * 2);

        GLuint oldEbo = ebo;
        GLuint newEbo = 0;
        glGenBuffers(1, &newEbo);
        glBindBuffer(GL_COPY_WRITE_BUFFER, newEbo);
        while (glGetError() != GL_NO_ERROR);

        glBufferData(
            GL_COPY_WRITE_BUFFER,
            static_cast<GLsizeiptr>(newCapacity * sizeof(uint32_t)),
            nullptr,
            GL_DYNAMIC_DRAW
        );
        if (glGetError() != GL_NO_ERROR) {
            glDeleteBuffers(1, &newEbo);
            return false;
        }

        glBindBuffer(GL_COPY_READ_BUFFER, oldEbo);
        glCopyBufferSubData(
            GL_COPY_READ_BUFFER,
            GL_COPY_WRITE_BUFFER,
            0,
            0,
            static_cast<GLsizeiptr>(oldCapacity * sizeof(uint32_t))
        );
        if (glGetError() != GL_NO_ERROR) {
            glDeleteBuffers(1, &newEbo);
            return false;
        }

        ebo = newEbo;
        indexCapacity = newCapacity;
        releaseRange(freeIndexRanges, oldCapacity, newCapacity - oldCapacity);
        configureVertexArray();
        glDeleteBuffers(1, &oldEbo);
        return true;
    }
    void uploadBufferRange(
        GLenum target,
        GLuint buffer,
        size_t offset,
        size_t bytes,
        const void* data
    ) {
        glBindBuffer(target, buffer);
        glBufferSubData(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(bytes), data);
    }

public:
    GeometryArena() = default;
    ~GeometryArena() { cleanUp(); }

    GeometryArena(const GeometryArena&) = delete;
    GeometryArena& operator=(const GeometryArena&) = delete;

    // Leave enough headroom for the initial streamed view so the render
    // thread does not hit an arena growth boundary during normal play.
    bool initialize(size_t initialVertices = 1u << 20, size_t initialIndices = 3u << 20) {
        if (initialized) return true;

        vertexCapacity = initialVertices;
        indexCapacity = initialIndices;
        freeVertexRanges.push_back({0, initialVertices});
        freeIndexRanges.push_back({0, initialIndices});

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        for (size_t i = 0; i < NUM_FRAME_CONTEXTS; ++i) {
            glGenBuffers(1, &frameContexts[i].metadataBuffer);
            glGenBuffers(1, &frameContexts[i].indirectBuffer);
        }

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertexCapacity * sizeof(VoxelVertex)),
            nullptr,
            GL_DYNAMIC_DRAW
        );
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(indexCapacity * sizeof(uint32_t)),
            nullptr,
            GL_DYNAMIC_DRAW
        );
        glBindVertexArray(0);
        configureVertexArray();
        initialized = true;
        frameContextReady = true;
        lastDrawCommandCount = 0;
        return true;
    }

    void cleanUp() {
        for (size_t i = 0; i < NUM_FRAME_CONTEXTS; ++i) {
            if (frameContexts[i].fence) glDeleteSync(frameContexts[i].fence);
            frameContexts[i].fence = 0;
            if (frameContexts[i].metadataBuffer) glDeleteBuffers(1, &frameContexts[i].metadataBuffer);
            if (frameContexts[i].indirectBuffer) glDeleteBuffers(1, &frameContexts[i].indirectBuffer);
            frameContexts[i].metadataBuffer = 0;
            frameContexts[i].indirectBuffer = 0;
            frameContexts[i].metadataCapacityBytes = 0;
            frameContexts[i].indirectCapacityBytes = 0;
            frameContexts[i].retiredGeometry.clear();
        }
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = vbo = ebo = 0;
        initialized = false;
        frameContextReady = true;
        lastDrawCommandCount = 0;
        vertexCapacity = 0;
        indexCapacity = 0;
        freeVertexRanges.clear();
        freeIndexRanges.clear();
    }

    void advanceFrame() {
        for (size_t offset = 1; offset <= NUM_FRAME_CONTEXTS; ++offset) {
            size_t candidateIndex = (frameIndex + offset) % NUM_FRAME_CONTEXTS;
            FrameContext& candidate = frameContexts[candidateIndex];
            if (candidate.fence) {
                GLenum result = glClientWaitSync(candidate.fence, 0, 0);
                if (result != GL_ALREADY_SIGNALED &&
                    result != GL_CONDITION_SATISFIED) {
                    continue;
                }
                glDeleteSync(candidate.fence);
                candidate.fence = 0;
            }

            frameIndex = candidateIndex;
            frameContextReady = true;
            for (const GeometryHandle& handle : candidate.retiredGeometry) {
                if (handle.valid) {
                    releaseRange(freeVertexRanges, handle.vertexOffset, handle.vertexCount);
                    releaseRange(freeIndexRanges, handle.indexOffset, handle.indexCount);
                }
            }
            candidate.retiredGeometry.clear();
            return;
        }

        // Keep drawing from the last safe buffer instead of waiting on the gpu.
        frameContextReady = false;
    }

    void markSubmitted() {
        FrameContext& ctx = frameContexts[frameIndex];
        if (ctx.fence) glDeleteSync(ctx.fence);
        ctx.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    GeometryHandle upload(const std::vector<VoxelVertex>& vertices, const std::vector<uint32_t>& indices) {
        GeometryHandle handle;
        if (!initialized || vertices.empty() || indices.empty()) return handle;
        if (vertices.size() > std::numeric_limits<uint32_t>::max() ||
            indices.size() > std::numeric_limits<uint32_t>::max()) return handle;

        size_t vertexStart = allocateRange(freeVertexRanges, vertices.size());
        if (vertexStart == std::numeric_limits<size_t>::max()) {
            if (!growVertexStorage(vertexCapacity + vertices.size())) return handle;
            vertexStart = allocateRange(freeVertexRanges, vertices.size());
        }

        size_t indexStart = allocateRange(freeIndexRanges, indices.size());
        if (indexStart == std::numeric_limits<size_t>::max()) {
            if (!growIndexStorage(indexCapacity + indices.size())) return handle;
            indexStart = allocateRange(freeIndexRanges, indices.size());
        }

        if (vertexStart == std::numeric_limits<size_t>::max() ||
            indexStart == std::numeric_limits<size_t>::max() ||
            vertexStart > static_cast<size_t>(std::numeric_limits<int32_t>::max()) ||
            indexStart > std::numeric_limits<uint32_t>::max()) {
            if (vertexStart != std::numeric_limits<size_t>::max()) {
                releaseRange(freeVertexRanges, vertexStart, vertices.size());
            }
            if (indexStart != std::numeric_limits<size_t>::max()) {
                releaseRange(freeIndexRanges, indexStart, indices.size());
            }
            return handle;
        }

        glBindVertexArray(vao);
        uploadBufferRange(
            GL_ARRAY_BUFFER,
            vbo,
            vertexStart * sizeof(VoxelVertex),
            vertices.size() * sizeof(VoxelVertex),
            vertices.data()
        );
        uploadBufferRange(
            GL_ELEMENT_ARRAY_BUFFER,
            ebo,
            indexStart * sizeof(uint32_t),
            indices.size() * sizeof(uint32_t),
            indices.data()
        );
        glBindVertexArray(0);

        handle.vertexOffset = vertexStart;
        handle.indexOffset = indexStart;
        handle.vertexCount = static_cast<uint32_t>(vertices.size());
        handle.indexCount = static_cast<uint32_t>(indices.size());
        handle.baseVertex = static_cast<int32_t>(vertexStart);
        handle.gpuBytes = vertices.size() * sizeof(VoxelVertex) + indices.size() * sizeof(uint32_t);
        handle.valid = true;
        return handle;
    }

    void release(const GeometryHandle& handle) {
        if (!handle.valid) return;
        frameContexts[frameIndex].retiredGeometry.push_back(handle);
    }

    void releaseImmediate(const GeometryHandle& handle) {
        if (!handle.valid) return;
        releaseRange(freeVertexRanges, handle.vertexOffset, handle.vertexCount);
        releaseRange(freeIndexRanges, handle.indexOffset, handle.indexCount);
    }

    bool uploadDrawData(
        const std::vector<SectionGpuMetadata>& metadata,
        const std::vector<DrawElementsIndirectCommand>& commands
    ) {
        if (!initialized ||
            !frameContextReady ||
            metadata.size() != commands.size()) {
            return false;
        }
        FrameContext& ctx = frameContexts[frameIndex];

        size_t metadataBytes = metadata.size() * sizeof(SectionGpuMetadata);
        size_t indirectBytes = commands.size() * sizeof(DrawElementsIndirectCommand);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx.metadataBuffer);
        if (metadataBytes > ctx.metadataCapacityBytes) {
            ctx.metadataCapacityBytes = std::max(metadataBytes, std::max<size_t>(sizeof(SectionGpuMetadata), ctx.metadataCapacityBytes * 2));
            glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(ctx.metadataCapacityBytes), nullptr, GL_STREAM_DRAW);
        }
        if (metadataBytes > 0) {
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(metadataBytes), metadata.data());
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, ctx.indirectBuffer);
        if (indirectBytes > ctx.indirectCapacityBytes) {
            ctx.indirectCapacityBytes = std::max(indirectBytes, std::max<size_t>(sizeof(DrawElementsIndirectCommand), ctx.indirectCapacityBytes * 2));
            glBufferData(GL_DRAW_INDIRECT_BUFFER, static_cast<GLsizeiptr>(ctx.indirectCapacityBytes), nullptr, GL_STREAM_DRAW);
        }
        if (indirectBytes > 0) {
            glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, static_cast<GLsizeiptr>(indirectBytes), commands.data());
        }
        lastDrawCommandCount = commands.size();
        return true;
    }
    size_t getLastDrawCommandCount() const {
        return lastDrawCommandCount;
    }




    void drawIndirect(size_t commandCount) const {
        if (!initialized || commandCount == 0) return;
        const FrameContext& ctx = frameContexts[frameIndex];
        glBindVertexArray(vao);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx.metadataBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, ctx.indirectBuffer);
        glMultiDrawElementsIndirect(
            GL_TRIANGLES,
            GL_UNSIGNED_INT,
            nullptr,
            static_cast<GLsizei>(commandCount),
            static_cast<GLsizei>(sizeof(DrawElementsIndirectCommand))
        );
        glBindVertexArray(0);
    }

    IndirectCommandDiagnostics inspectIndirectCommands(size_t commandCount, size_t activeFrameIndex = std::numeric_limits<size_t>::max()) const {
        IndirectCommandDiagnostics diagnostics;
        if (!initialized || commandCount == 0) return diagnostics;
        size_t idx = (activeFrameIndex != std::numeric_limits<size_t>::max()) ? activeFrameIndex : frameIndex;
        std::vector<DrawElementsIndirectCommand> commands(commandCount);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, frameContexts[idx].indirectBuffer);
        glGetBufferSubData(
            GL_DRAW_INDIRECT_BUFFER,
            0,
            static_cast<GLsizeiptr>(commands.size() * sizeof(DrawElementsIndirectCommand)),
            commands.data()
        );
        for (const DrawElementsIndirectCommand& command : commands) {
            if (command.count == 0 || command.instanceCount == 0) continue;
            ++diagnostics.nonZeroCommands;
            diagnostics.totalIndices += command.count;
            diagnostics.maxIndices = std::max(diagnostics.maxIndices, command.count);
        }
        return diagnostics;
    }



    bool isInitialized() const { return initialized; }
};

class Mesh {
public:
    GeometryHandle geometry;
    bool uploaded = false;

    void attach(const GeometryHandle& handle) {
        geometry = handle;
        uploaded = handle.valid;
    }

    void cleanUp() {
        geometry = GeometryHandle{};
        uploaded = false;
    }
};

#endif // MESH_HPP
```

## MeshBuilder.hpp

**Path:** `MeshBuilder.hpp` | **Lines:** 112 | **Size:** 4345 bytes

```cpp
#ifndef MESH_BUILDER_HPP
#define MESH_BUILDER_HPP

#include "Chunk.hpp"
#include "MeshingNeighborhood.hpp"
#include "WorldGenerator.hpp"
#include "LightingSystem.hpp"
#include "Mesher.hpp"
#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>

class MeshBuilder {
public:
    static float calculateAO(bool side1, bool side2, bool corner) {
        return Mesher::calculateAO(side1, side2, corner);
    }

    static void generateVoxelData(Chunk& chunk, MeshingNeighborhood* neighborhood = nullptr) {
        WorldGenerator::generateVoxelData(chunk, neighborhood);
    }

    static void updateNeighborhood(
        const Chunk& chunk,
        MeshingNeighborhood& neighborhood,
        const std::array<std::shared_ptr<Chunk>, 6>& neighbors,
        bool rebuildBlocks = false
    ) {
        if (rebuildBlocks) {
            neighborhood.blocks.fill(BLOCK_AIR);
        }
        neighborhood.light.fill(0);

        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    neighborhood.blocks[getPaddedVoxelIndex(x, y, z)] =
                        chunk.getBlock(x, y, z);
                    neighborhood.light[getPaddedVoxelIndex(x, y, z)] =
                        chunk.getLight(x, y, z);
                }
            }
        }

        for (int y = -1; y <= CHUNK_SIZE; ++y) {
            for (int z = -1; z <= CHUNK_SIZE; ++z) {
                for (int x = -1; x <= CHUNK_SIZE; ++x) {
                    int outsideCount = (x < 0 || x >= CHUNK_SIZE ? 1 : 0) +
                        (y < 0 || y >= CHUNK_SIZE ? 1 : 0) +
                        (z < 0 || z >= CHUNK_SIZE ? 1 : 0);
                    if (outsideCount == 0) continue;

                    int paddedIndex = getPaddedVoxelIndex(x, y, z);
                    if (outsideCount == 1) {
                        int direction = -1;
                        int nx = x;
                        int ny = y;
                        int nz = z;
                        if (x < 0) {
                            direction = DIR_NEG_X;
                            nx = CHUNK_SIZE - 1;
                        } else if (x >= CHUNK_SIZE) {
                            direction = DIR_POS_X;
                            nx = 0;
                        } else if (y < 0) {
                            direction = DIR_NEG_Y;
                            ny = CHUNK_SIZE - 1;
                        } else if (y >= CHUNK_SIZE) {
                            direction = DIR_POS_Y;
                            ny = 0;
                        } else if (z < 0) {
                            direction = DIR_NEG_Z;
                            nz = CHUNK_SIZE - 1;
                        } else {
                            direction = DIR_POS_Z;
                            nz = 0;
                        }

                        const std::shared_ptr<Chunk>& neighbor = neighbors[direction];
                        bool neighborReady = neighbor &&
                            neighbor->resident.load(std::memory_order_acquire) &&
                            neighbor->isGenerated.load(std::memory_order_acquire);
                        if (neighborReady) {
                            neighborhood.blocks[paddedIndex] =
                                neighbor->getBlock(nx, ny, nz);
                            neighborhood.light[paddedIndex] =
                                neighbor->getLight(nx, ny, nz);
                            continue;
                        }
                    }

                    int bx = std::clamp(x, 0, CHUNK_SIZE - 1);
                    int by = std::clamp(y, 0, CHUNK_SIZE - 1);
                    int bz = std::clamp(z, 0, CHUNK_SIZE - 1);
                    uint16_t bLight = chunk.getLight(bx, by, bz);
                    if (bLight == 0 && y >= 0 && getBlockInfo(neighborhood.blocks[paddedIndex]).isTransparent) {
                        bLight = packLight(0, 0, 0, 15);
                    }
                    neighborhood.light[paddedIndex] = bLight;
                }
            }
        }
    }

    static void buildMesh(Chunk& chunk, const MeshingNeighborhood* neighborhood = nullptr) {
        Mesher::buildMesh(chunk, neighborhood);
    }
};

#endif // MESH_BUILDER_HPP
```

## Mesher.cpp

**Path:** `Mesher.cpp` | **Lines:** 532 | **Size:** 24057 bytes

```cpp
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
```

## Mesher.hpp

**Path:** `Mesher.hpp` | **Lines:** 29 | **Size:** 639 bytes

```cpp
#ifndef MESHER_HPP
#define MESHER_HPP

#include "Chunk.hpp"
#include "MeshingNeighborhood.hpp"
#include "Mesh.hpp"
#include <vector>
#include <cstdint>
#include <memory>

struct ChunkBuildInput {
    IVec3 position;
    int lod = 0;
    std::shared_ptr<Chunk> chunk;
    std::unique_ptr<MeshingNeighborhood> neighborhood;
};

struct ChunkBuildOutput {
    std::vector<VoxelVertex> vertices;
    std::vector<uint32_t> indices;
};

class Mesher {
public:
    static float calculateAO(bool side1, bool side2, bool corner);
    static ChunkBuildOutput buildMesh(Chunk& chunk, const MeshingNeighborhood* neighborhood);
};

#endif // MESHER_HPP
```

## MeshingNeighborhood.hpp

**Path:** `MeshingNeighborhood.hpp` | **Lines:** 34 | **Size:** 1031 bytes

```cpp
#ifndef MESHING_NEIGHBORHOOD_HPP
#define MESHING_NEIGHBORHOOD_HPP

#include "Chunk.hpp"
#include <array>
#include <algorithm>

struct MeshingNeighborhood {
    std::array<uint8_t, PADDED_VOL> blocks{};
    std::array<uint16_t, PADDED_VOL> light{};

    inline uint8_t block(int x, int y, int z) const {
        if (x < -1 || x > CHUNK_SIZE ||
            y < -1 || y > CHUNK_SIZE ||
            z < -1 || z > CHUNK_SIZE) {
            return BLOCK_AIR;
        }
        return blocks[getPaddedVoxelIndex(x, y, z)];
    }

    inline uint16_t lightAt(int x, int y, int z) const {
        if (x < -1 || x > CHUNK_SIZE ||
            y < -1 || y > CHUNK_SIZE ||
            z < -1 || z > CHUNK_SIZE) {
            int bx = std::clamp(x, 0, CHUNK_SIZE - 1);
            int by = std::clamp(y, 0, CHUNK_SIZE - 1);
            int bz = std::clamp(z, 0, CHUNK_SIZE - 1);
            return light[getPaddedVoxelIndex(bx, by, bz)];
        }
        return light[getPaddedVoxelIndex(x, y, z)];
    }
};

#endif // MESHING_NEIGHBORHOOD_HPP
```

## Physics.hpp

**Path:** `Physics.hpp` | **Lines:** 142 | **Size:** 5093 bytes

```cpp
#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include "MathUtils.hpp"
#include "Camera.hpp"
#include "IWorldQuery.hpp"
#include <cmath>
#include <algorithm>

struct PlayerAABB {
    Vec3 minP;
    Vec3 maxP;

    static PlayerAABB getAt(Vec3 pos) {
        PlayerAABB b;
        b.minP = Vec3(pos.x - 0.3f, pos.y - 1.62f, pos.z - 0.3f);
        b.maxP = Vec3(pos.x + 0.3f, pos.y + 0.18f, pos.z + 0.3f);
        return b;
    }

    bool intersects(const Vec3& bMin, const Vec3& bMax) const {
        return (minP.x < bMax.x && maxP.x > bMin.x) &&
               (minP.y < bMax.y && maxP.y > bMin.y) &&
               (minP.z < bMax.z && maxP.z > bMin.z);
    }
};

class PhysicsController {
public:
    Vec3 velocity;
    bool isFlying = true; // Default to flight mode so player can explore infinite islands immediately!
    bool isGrounded = false;
    float walkSpeed = 7.0f;
    float flySpeed = 30.0f;
    float superFlySpeed = 160.0f;

    PhysicsController() : velocity(0, 0, 0) {}

    void update(Camera& camera, const IWorldQuery& worldQuery, bool keys[1024], bool superSpeed, float dt) {
        if (dt > 0.1f) dt = 0.1f; // Cap max delta time

        Vec3 inputDir(0, 0, 0);
        Vec3 forward = Vec3(camera.front.x, 0.0f, camera.front.z).normalized();
        Vec3 right = camera.right;

        if (keys['W']) inputDir += forward;
        if (keys['S']) inputDir -= forward;
        if (keys['A']) inputDir -= right;
        if (keys['D']) inputDir += right;

        if (inputDir.lengthSq() > 0.001f) {
            inputDir = inputDir.normalized();
        }

        if (isFlying) {
            Vec3 flyDir = inputDir;
            if (keys[' ']) flyDir.y += 1.0f;
            if (keys['C'] || keys[GLFW_KEY_LEFT_SHIFT]) flyDir.y -= 1.0f;

            float curSpeed = superSpeed ? superFlySpeed : flySpeed;
            camera.position += flyDir * curSpeed * dt;
            velocity = Vec3(0, 0, 0);
            isGrounded = false;
            return;
        }

        // Walking / Gravity physics mode
        float curSpeed = superSpeed ? (walkSpeed * 1.8f) : walkSpeed;
        velocity.x = inputDir.x * curSpeed;
        velocity.z = inputDir.z * curSpeed;

        // Apply gravity
        velocity.y -= 26.0f * dt;

        // Jump
        if (isGrounded && keys[' ']) {
            velocity.y = 9.5f;
            isGrounded = false;
        }

        // Axis-by-axis collision resolution
        // 1. Move X
        camera.position.x += velocity.x * dt;
        resolveCollisions(camera.position, worldQuery, 0);

        // 2. Move Y
        camera.position.y += velocity.y * dt;
        isGrounded = false;
        resolveCollisions(camera.position, worldQuery, 1);

        // 3. Move Z
        camera.position.z += velocity.z * dt;
        resolveCollisions(camera.position, worldQuery, 2);
    }

private:
    void resolveCollisions(Vec3& pos, const IWorldQuery& worldQuery, int axis) {
        PlayerAABB playerBox = PlayerAABB::getAt(pos);

        int minX = static_cast<int>(std::floor(playerBox.minP.x));
        int maxX = static_cast<int>(std::floor(playerBox.maxP.x));
        int minY = static_cast<int>(std::floor(playerBox.minP.y));
        int maxY = static_cast<int>(std::floor(playerBox.maxP.y));
        int minZ = static_cast<int>(std::floor(playerBox.minP.z));
        int maxZ = static_cast<int>(std::floor(playerBox.maxP.z));

        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    if (worldQuery.isBlockSolidAt(x, y, z)) {
                        Vec3 blockMin(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                        Vec3 blockMax = blockMin + Vec3(1.0f);

                        if (playerBox.intersects(blockMin, blockMax)) {
                            if (axis == 0) { // X axis
                                if (velocity.x > 0) pos.x = blockMin.x - 0.301f;
                                else if (velocity.x < 0) pos.x = blockMax.x + 0.301f;
                                velocity.x = 0;
                            } else if (axis == 1) { // Y axis
                                if (velocity.y > 0) {
                                    pos.y = blockMin.y - 0.181f;
                                    velocity.y = 0;
                                } else if (velocity.y < 0) {
                                    pos.y = blockMax.y + 1.621f;
                                    velocity.y = 0;
                                    isGrounded = true;
                                }
                            } else if (axis == 2) { // Z axis
                                if (velocity.z > 0) pos.z = blockMin.z - 0.301f;
                                else if (velocity.z < 0) pos.z = blockMax.z + 0.301f;
                                velocity.z = 0;
                            }
                            playerBox = PlayerAABB::getAt(pos);
                        }
                    }
                }
            }
        }
    }
};

#endif // PHYSICS_HPP
```

## Renderer.cpp

**Path:** `Renderer.cpp` | **Lines:** 52 | **Size:** 1870 bytes

```cpp
#include "Renderer.hpp"
#include <algorithm>

bool Renderer::render(
    Vec3 cameraPos,
    const std::vector<Chunk*>& selectedChunks,
    IndirectCommandDiagnostics* outDiagnostics
) {
    if (!geometryArena.isInitialized()) return false;

    std::vector<SectionGpuMetadata> drawMetadata;
    std::vector<DrawElementsIndirectCommand> drawCommands;
    drawMetadata.reserve(selectedChunks.size());
    drawCommands.reserve(selectedChunks.size());

    for (size_t i = 0; i < selectedChunks.size(); ++i) {
        Chunk* chunk = selectedChunks[i];
        if (!chunk) continue;

        SectionGpuMetadata meta;
        Vec3 minP(
            static_cast<float>(chunk->worldMin.x) - cameraPos.x,
            static_cast<float>(chunk->worldMin.y) - cameraPos.y,
            static_cast<float>(chunk->worldMin.z) - cameraPos.z
        );
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

    geometryArena.uploadDrawData(drawMetadata, drawCommands);
    geometryArena.drawIndirect(geometryArena.getLastDrawCommandCount());

    if (outDiagnostics) {
        *outDiagnostics = geometryArena.inspectIndirectCommands(geometryArena.getLastDrawCommandCount());
    }

    return true;
}
```

## Renderer.hpp

**Path:** `Renderer.hpp` | **Lines:** 42 | **Size:** 912 bytes

```cpp
#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "Mesh.hpp"
#include "Chunk.hpp"
#include "MathUtils.hpp"
#include <vector>

class Renderer {
private:
    GeometryArena geometryArena;

public:
    Renderer() = default;

    bool initialize(size_t maxVertices = 16777216, size_t maxIndices = 33554432) {
        return geometryArena.initialize(maxVertices, maxIndices);
    }

    void cleanUp() {
        geometryArena.cleanUp();
    }

    GeometryArena& getGeometryArena() { return geometryArena; }
    const GeometryArena& getGeometryArena() const { return geometryArena; }

    void advanceFrame() {
        geometryArena.advanceFrame();
    }

    void markSubmitted() {
        geometryArena.markSubmitted();
    }

    bool render(
        Vec3 cameraPos,
        const std::vector<Chunk*>& selectedChunks,
        IndirectCommandDiagnostics* outDiagnostics = nullptr
    );
};

#endif // RENDERER_HPP
```

## Shader.hpp

**Path:** `Shader.hpp` | **Lines:** 170 | **Size:** 5584 bytes

```cpp
#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <epoxy/gl.h>
#include <GLFW/glfw3.h>
#include "MathUtils.hpp"

class Shader {
public:
    GLuint programID = 0;

    Shader() = default;
    Shader(const char* vertexSrc, const char* fragmentSrc) {
        compile(vertexSrc, fragmentSrc);
    }
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept : programID(other.programID), uniformCache(std::move(other.uniformCache)) {
        other.programID = 0;
    }

    Shader& operator=(Shader&& other) noexcept {
        if (this != &other) {
            if (programID != 0) glDeleteProgram(programID);
            programID = other.programID;
            uniformCache = std::move(other.uniformCache);
            other.programID = 0;
        }
        return *this;
    }

    ~Shader() {
        if (programID) glDeleteProgram(programID);
    }

    bool compile(const char* vShaderCode, const char* fShaderCode) {
        GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        GLuint newProgram = glCreateProgram();
        glAttachShader(newProgram, vertex);
        glAttachShader(newProgram, fragment);
        glLinkProgram(newProgram);
        checkCompileErrors(newProgram, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);

        GLint linked = GL_FALSE;
        glGetProgramiv(newProgram, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            glDeleteProgram(newProgram);
            return false;
        }

        if (programID != 0) glDeleteProgram(programID);
        programID = newProgram;
        uniformCache.clear();
        return true;
    }
    bool compileFromFile(const std::string& vertexPath, const std::string& fragmentPath) {
        std::ifstream vFile(vertexPath), fFile(fragmentPath);
        if (!vFile.is_open() || !fFile.is_open()) {
            std::cerr << "Failed to open shader files: " << vertexPath << ", " << fragmentPath << "\n";
            return false;
        }
        std::stringstream vStream, fStream;
        vStream << vFile.rdbuf();
        fStream << fFile.rdbuf();
        return compile(vStream.str().c_str(), fStream.str().c_str());
    }

    void compileCompute(const char* computeShaderCode) {
        GLuint compute = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(compute, 1, &computeShaderCode, NULL);
        glCompileShader(compute);
        checkCompileErrors(compute, "COMPUTE");

        GLint compiled = GL_FALSE;
        glGetShaderiv(compute, GL_COMPILE_STATUS, &compiled);
        if (compiled != GL_TRUE) {
            glDeleteShader(compute);
            programID = 0;
            return;
        }

        programID = glCreateProgram();
        glAttachShader(programID, compute);
        glLinkProgram(programID);
        checkCompileErrors(programID, "PROGRAM");

        GLint linked = GL_FALSE;
        glGetProgramiv(programID, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            glDeleteProgram(programID);
            programID = 0;
            glDeleteShader(compute);
            return;
        }

        glDeleteShader(compute);
    }

    void use() const {
        if (programID) glUseProgram(programID);
    }

    GLint getUniformLoc(const std::string& name) const {
        auto it = uniformCache.find(name);
        if (it != uniformCache.end()) return it->second;
        GLint loc = glGetUniformLocation(programID, name.c_str());
        uniformCache[name] = loc;
        return loc;
    }

    void setBool(const std::string& name, bool value) const {
        glUniform1i(getUniformLoc(name), (int)value);
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(getUniformLoc(name), value);
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(getUniformLoc(name), value);
    }
    void setVec3(const std::string& name, const Vec3& value) const {
        glUniform3f(getUniformLoc(name), value.x, value.y, value.z);
    }
    void setVec2(const std::string& name, float x, float y) const {
        glUniform2f(getUniformLoc(name), x, y);
    }
    void setMat4(const std::string& name, const Mat4& mat) const {
        glUniformMatrix4fv(getUniformLoc(name), 1, GL_FALSE, mat.m);
    }

private:
    mutable std::unordered_map<std::string, GLint> uniformCache;
private:
    void checkCompileErrors(GLuint shader, std::string type) {
        GLint success;
        GLchar infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n";
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n";
            }
        }
    }
};

#endif // SHADER_HPP
```

## SimplexNoise.cpp

**Path:** `SimplexNoise.cpp` | **Lines:** 44 | **Size:** 2416 bytes

```cpp
#include "SimplexNoise.hpp"

const float SimplexNoise::grad3[16][3] = {
    {1.0f,1.0f,0.0f},{-1.0f,1.0f,0.0f},{1.0f,-1.0f,0.0f},{-1.0f,-1.0f,0.0f},
    {1.0f,0.0f,1.0f},{-1.0f,0.0f,1.0f},{1.0f,0.0f,-1.0f},{-1.0f,0.0f,-1.0f},
    {0.0f,1.0f,1.0f},{0.0f,-1.0f,1.0f},{0.0f,1.0f,-1.0f},{0.0f,-1.0f,-1.0f},
    {1.0f,1.0f,0.0f},{0.0f,-1.0f,1.0f},{-1.0f,1.0f,0.0f},{0.0f,-1.0f,-1.0f}
};

const uint8_t SimplexNoise::perm[512] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
    140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
    247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
    57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
    74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
    65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,
    200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
    52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
    207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
    119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,
    129,22,39,253,19,98,108,110,79,113,224,232,178,185,115,164,
    143,149,34,81,176,157,25,24,251,126,107,45,14,67,61,84,
    121,50,22,112,63,222,97,141,127,104,180,181,214,249,191,128,
    114,246,241,106,78,195,193,215,236,179,29,150,243,199,239,156,
    184,218,93,228,49,210,235,162,12,242,138,51,72,254,145,31,

    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
    140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
    247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
    57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
    74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
    65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,
    200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
    52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
    207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
    119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,
    129,22,39,253,19,98,108,110,79,113,224,232,178,185,115,164,
    143,149,34,81,176,157,25,24,251,126,107,45,14,67,61,84,
    121,50,22,112,63,222,97,141,127,104,180,181,214,249,191,128,
    114,246,241,106,78,195,193,215,236,179,29,150,243,199,239,156,
    184,218,93,228,49,210,235,162,12,242,138,51,72,254,145,31
};
```

## SimplexNoise.hpp

**Path:** `SimplexNoise.hpp` | **Lines:** 120 | **Size:** 3533 bytes

```cpp
#ifndef SIMPLEX_NOISE_HPP
#define SIMPLEX_NOISE_HPP

#include <cmath>
#include <cstdint>
#include <algorithm>

class SimplexNoise {
private:
    static const uint8_t perm[512];
    static const float grad3[16][3];

    static inline int fastfloor(float x) {
        int xi = (int)x;
        return x < xi ? xi - 1 : xi;
    }

    static inline float dot(const float g[3], float x, float y, float z) {
        return g[0] * x + g[1] * y + g[2] * z;
    }

public:
    static float eval3D(float xin, float yin, float zin) {
        float n0, n1, n2, n3;

        const float F3 = 1.0f / 3.0f;
        float s = (xin + yin + zin) * F3;
        int i = fastfloor(xin + s);
        int j = fastfloor(yin + s);
        int k = fastfloor(zin + s);

        const float G3 = 1.0f / 6.0f;
        float t = (i + j + k) * G3;
        float X0 = i - t;
        float Y0 = j - t;
        float Z0 = k - t;
        float x0 = xin - X0;
        float y0 = yin - Y0;
        float z0 = zin - Z0;

        int i1, j1, k1;
        int i2, j2, k2;

        if (x0 >= y0) {
            if (y0 >= z0)      { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }
            else if (x0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }
            else               { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }
        } else {
            if (y0 < z0)       { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }
            else if (x0 < z0)  { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }
            else               { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }
        }

        float x1 = x0 - i1 + G3;
        float y1 = y0 - j1 + G3;
        float z1 = z0 - k1 + G3;
        float x2 = x0 - i2 + 2.0f * G3;
        float y2 = y0 - j2 + 2.0f * G3;
        float z2 = z0 - k2 + 2.0f * G3;
        float x3 = x0 - 1.0f + 3.0f * G3;
        float y3 = y0 - 1.0f + 3.0f * G3;
        float z3 = z0 - 1.0f + 3.0f * G3;

        int ii = i & 255;
        int jj = j & 255;
        int kk = k & 255;

        float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;
        if (t0 < 0) n0 = 0.0f;
        else {
            t0 *= t0;
            int gi0 = perm[ii + perm[jj + perm[kk]]] & 15;
            n0 = t0 * t0 * dot(grad3[gi0], x0, y0, z0);
        }

        float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
        if (t1 < 0) n1 = 0.0f;
        else {
            t1 *= t1;
            int gi1 = perm[ii+i1 + perm[jj+j1 + perm[kk+k1]]] & 15;
            n1 = t1 * t1 * dot(grad3[gi1], x1, y1, z1);
        }

        float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
        if (t2 < 0) n2 = 0.0f;
        else {
            t2 *= t2;
            int gi2 = perm[ii+i2 + perm[jj+j2 + perm[kk+k2]]] & 15;
            n2 = t2 * t2 * dot(grad3[gi2], x2, y2, z2);
        }

        float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
        if (t3 < 0) n3 = 0.0f;
        else {
            t3 *= t3;
            int gi3 = perm[ii+1 + perm[jj+1 + perm[kk+1]]] & 15;
            n3 = t3 * t3 * dot(grad3[gi3], x3, y3, z3);
        }

        return 32.0f * (n0 + n1 + n2 + n3);
    }

    static float octave3D(float x, float y, float z, int octaves, float persistence, float lacunarity) {
        float total = 0.0f;
        float frequency = 1.0f;
        float amplitude = 1.0f;
        float maxValue = 0.0f;

        for (int i = 0; i < octaves; i++) {
            total += eval3D(x * frequency, y * frequency, z * frequency) * amplitude;
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return total / maxValue;
    }
};

#endif // SIMPLEX_NOISE_HPP
```

## Skybox.hpp

**Path:** `Skybox.hpp` | **Lines:** 147 | **Size:** 5490 bytes

```cpp
#ifndef SKYBOX_HPP
#define SKYBOX_HPP

#include "Shader.hpp"
#include "MathUtils.hpp"
#include <vector>
#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

class Skybox {
private:
    GLuint vao = 0, vbo = 0;
    Shader shader;

public:
    Skybox() = default;
    ~Skybox() {
        if (vao != 0) glDeleteVertexArrays(1, &vao);
        if (vbo != 0) glDeleteBuffers(1, &vbo);
    }

    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;

    Skybox(Skybox&& other) noexcept
        : vao(other.vao), vbo(other.vbo), shader(std::move(other.shader)) {
        other.vao = 0;
        other.vbo = 0;
    }

    Skybox& operator=(Skybox&& other) noexcept {
        if (this != &other) {
            if (vao != 0) glDeleteVertexArrays(1, &vao);
            if (vbo != 0) glDeleteBuffers(1, &vbo);
            vao = other.vao;
            vbo = other.vbo;
            shader = std::move(other.shader);
            other.vao = 0;
            other.vbo = 0;
        }
        return *this;
    }
    void init() {
        if (!shader.compileFromFile("assets/shaders/skybox.vert", "assets/shaders/skybox.frag")) {
            std::cerr << "Falling back to inline skybox shaders...\n";
            const char* vShader = R"(
                #version 330 core
                layout (location = 0) in vec3 aPos;
                out vec3 vWorldPos;
                uniform mat4 uProjection;
                uniform mat4 uView;
                void main() {
                    vWorldPos = aPos;
                    mat4 rotView = mat4(mat3(uView));
                    vec4 clipPos = uProjection * rotView * vec4(aPos, 1.0);
                    gl_Position = clipPos.xyww;
                }
            )";
            const char* fShader = R"(
                #version 330 core
                in vec3 vWorldPos;
                out vec4 FragColor;
                uniform vec3 uSunDir;
                uniform vec3 uSunColor;
                uniform vec3 uSkyTopColor;
                uniform vec3 uSkyHorizonColor;
                uniform vec3 uSkySurfaceHorizonColor;
                uniform vec3 uSkyFloatingHorizonColor;
                void main() {
                    vec3 dir = normalize(vWorldPos);
                    float layerBlend = smoothstep(-0.85, 0.85, dir.y);
                    vec3 directionalHorizon = mix(uSkySurfaceHorizonColor, uSkyFloatingHorizonColor, layerBlend);
                    vec3 horizonColor = mix(directionalHorizon, uSkyHorizonColor, 0.35);
                    float h = max(dir.y, 0.0);
                    vec3 skyColor = mix(horizonColor, uSkyTopColor, pow(h, 0.6));
                    float sunDot = max(dot(dir, normalize(uSunDir)), 0.0);
                    float sunDisk = pow(sunDot, 800.0) * 3.0;
                    float sunGlow = pow(sunDot, 12.0) * 0.4;
                    float moonDot = max(dot(dir, -normalize(uSunDir)), 0.0);
                    float moonDisk = pow(moonDot, 1200.0) * 1.5;
                    vec3 finalSky = skyColor + uSunColor * (sunDisk + sunGlow) + vec3(0.8, 0.9, 1.0) * moonDisk;
                    FragColor = vec4(finalSky, 1.0);
                }
            )";
            shader.compile(vShader, fShader);
        }

        float skyboxVertices[] = {
            // Positions          
            -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }

    void draw(
        const Mat4& projection,
        const Mat4& view,
        Vec3 sunDir,
        Vec3 sunColor,
        Vec3 skyTop,
        Vec3 skyHorizon,
        Vec3 skySurfaceHorizon,
        Vec3 skyFloatingHorizon
    ) {
        glDepthFunc(GL_LEQUAL);
        shader.use();
        shader.setMat4("uProjection", projection);
        shader.setMat4("uView", view);
        shader.setVec3("uSunDir", sunDir);
        shader.setVec3("uSunColor", sunColor);
        shader.setVec3("uSkyTopColor", skyTop);
        shader.setVec3("uSkyHorizonColor", skyHorizon);
        shader.setVec3("uSkySurfaceHorizonColor", skySurfaceHorizon);
        shader.setVec3("uSkyFloatingHorizonColor", skyFloatingHorizon);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);
    }
};

#endif // SKYBOX_HPP
```

## StreamPlanner.cpp

**Path:** `StreamPlanner.cpp` | **Lines:** 131 | **Size:** 5206 bytes

```cpp
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
```

## StreamPlanner.hpp

**Path:** `StreamPlanner.hpp` | **Lines:** 36 | **Size:** 923 bytes

```cpp
#ifndef STREAM_PLANNER_HPP
#define STREAM_PLANNER_HPP

#include "ChunkStore.hpp"
#include "MathUtils.hpp"
#include <vector>
#include <array>

constexpr float SCREEN_SPACE_DIAMETER_THRESHOLD = 40.0f;
constexpr int LOD_RADII[NUM_LODS] = { 6, 8, 10, 12, 14, 16, 18 };

struct StreamTarget {
    IVec3 position;
    int lod;
    float priority;
};

class StreamPlanner {
public:
    static bool isChunkOutOfRange(const Chunk* chunk, const Vec3& camPos);
    
    static void getChunkBounds(const Chunk* chunk, Vec3 cameraPos, Vec3& minP, Vec3& maxP);
    
    static void selectHierarchicalNode(
        Chunk* chunk,
        const Frustum& frustum,
        Vec3 cameraPos,
        float projectionScale,
        const ChunkStore& store,
        std::vector<Chunk*>& selectedChunks
    );

    static std::vector<StreamTarget> computeStreamingPlan(const Vec3& camPos, const ChunkStore& store);
};

#endif // STREAM_PLANNER_HPP
```

## TextureAtlas.hpp

**Path:** `TextureAtlas.hpp` | **Lines:** 564 | **Size:** 34259 bytes

```cpp
#ifndef TEXTURE_ATLAS_HPP
#define TEXTURE_ATLAS_HPP

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

class TextureAtlas {
public:
    static constexpr int TILE_SIZE = 16;
    static constexpr int MAX_TILES = 256;

    static GLuint createProceduralAtlas() {
        std::vector<uint8_t> pixels(MAX_TILES * TILE_SIZE * TILE_SIZE * 4, 255);

        int currentTileID = 0;
        // Helper to set pixel color at tile coordinate (x, y)
        auto setPixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            if (currentTileID < 0 || currentTileID >= MAX_TILES || x < 0 || x >= TILE_SIZE || y < 0 || y >= TILE_SIZE) return;
            int idx = (currentTileID * TILE_SIZE * TILE_SIZE + y * TILE_SIZE + x) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        };

        // Pseudo-random helper
        auto randNoise = [](int x, int y, int seed) -> float {
            uint32_t n = static_cast<uint32_t>(x) +
                static_cast<uint32_t>(y) * 57u +
                static_cast<uint32_t>(seed) * 131u;
            n = (n << 13) ^ n;
            uint32_t value = n * (n * n * 15731u + 789221u) + 1376312589u;
            return 1.0f - static_cast<float>(value & 0x7fffffffu) / 1073741824.0f;
        };

        // Dummy pixel masks for now. They are deliberately hand-authored as
        // clusters instead of radial shapes so the canopy reads as foliage at
        // close range. Real leaf art can replace these atlas tiles later.
        const char* leafMasks[6][16] = {
            {
                "......##........", "....######......", "...########.....", "..##########....",
                ".############...", "##############..", "##############..", "###############.",
                "##############..", ".############...", "..###########...", "...#########....",
                "....#######.....", ".....#####......", "......###.......", "................"
            },
            {
                ".....###........", "...#######......", "..#########.....", ".#####.#####....",
                ".############...", "##############..", "##############..", "###############.",
                ".#############..", "..#####.#####...", "...##########...", "....########....",
                ".....######.....", "......####......", "................", "................"
            },
            {
                ".......##.......", ".....#####......", "...########.....", "..#####.###.....",
                ".############...", "###########.##..", "##############..", "###############.",
                "##############..", "..###########...", "...###.######...", "....########....",
                ".....######.....", "......####......", ".......##.......", "................"
            },
            {
                "....####........", "..########......", ".##########.....", ".####.######....",
                "#############...", "##############..", "######.#######..", "###############.",
                "##############..", ".######.######..", "..###########...", "...##########...",
                "....#######.....", ".....#####......", "......###.......", "................"
            },
            {
                "......###.......", "....#######.....", "..##########....", ".######.#####...",
                "#############...", "###########.##..", "##############..", "###############.",
                "##############..", "..###########...", "...##########...", "....#####.##....",
                ".....######.....", "......####......", "................", "................"
            },
            {
                ".....####.......", "...########.....", "..##########....", ".#######.###....",
                "#############...", "##############..", "########.######.", "###############.",
                "##############..", ".###########....", "..######.####...", "...##########...",
                "....#######.....", ".....#####......", "......###.......", "................"
            }
        };

        // Leaves are rendered as alpha-tested cutouts rather than blended
        // quads. This keeps depth ordering deterministic in dense trees.
        auto leafMask = [&](int px, int py, int tileID) -> bool {
            int variant = 0;
            if (tileID >= 38 && tileID <= 57) {
                variant = (tileID - 38) % 5 + 1;
            }
            return leafMasks[variant][py][px] == '#';
        };
        (void)leafMask;

        // Generate each 16x16 tile
        for (int tileID = 0; tileID < 60; ++tileID) {
            currentTileID = tileID;

            for (int py = 0; py < TILE_SIZE; ++py) {
                for (int px = 0; px < TILE_SIZE; ++px) {
                    float n = randNoise(px, py, tileID * 17) * 0.15f;

                    switch (tileID) {
                        case 0: { // Grass Top
                            uint8_t r = (uint8_t)std::clamp(55.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(170.0f + n * 60.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 1: { // Grass Side
                            if (py < 4 + (int)(n * 10.0f)) { // Green top lip
                                uint8_t r = (uint8_t)std::clamp(55.0f + n * 40.0f, 0.0f, 255.0f);
                                uint8_t g = (uint8_t)std::clamp(170.0f + n * 60.0f, 0.0f, 255.0f);
                                uint8_t b = (uint8_t)std::clamp(45.0f + n * 30.0f, 0.0f, 255.0f);
                                setPixel(px, py, r, g, b);
                            } else { // Dirt body
                                uint8_t r = (uint8_t)std::clamp(115.0f + n * 40.0f, 0.0f, 255.0f);
                                uint8_t g = (uint8_t)std::clamp(75.0f + n * 30.0f, 0.0f, 255.0f);
                                uint8_t b = (uint8_t)std::clamp(45.0f + n * 20.0f, 0.0f, 255.0f);
                                setPixel(px, py, r, g, b);
                            }
                            break;
                        }
                        case 2: { // Dirt
                            uint8_t r = (uint8_t)std::clamp(115.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(75.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 20.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 3: { // Stone
                            uint8_t val = (uint8_t)std::clamp(120.0f + n * 50.0f, 0.0f, 255.0f);
                            setPixel(px, py, val, val, val);
                            break;
                        }
                        case 58: { // Deep Stone
                            // A cool, darker slate gives the underside a
                            // readable material gradient without relying on
                            // shader-only color tricks.
                            float vertical = static_cast<float>(py) / 15.0f;
                            float vein = ((px * 5 + py * 3) % 11 == 0) ? 1.0f : 0.0f;
                            uint8_t r = (uint8_t)std::clamp(55.0f + vertical * 10.0f + n * 30.0f + vein * 16.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(64.0f + vertical * 12.0f + n * 32.0f + vein * 18.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(70.0f + vertical * 14.0f + n * 36.0f + vein * 22.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 4: { // Glow Crystal
                            float dist = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f)) / 8.0f;
                            uint8_t r = (uint8_t)std::clamp(255.0f - dist * 100.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(230.0f - dist * 120.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(100.0f + dist * 120.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 5: { // Oak Log Side
                            float barkNoise = std::sin(px * 1.5f) * 0.2f + n * 0.3f;
                            uint8_t r = (uint8_t)std::clamp(100.0f + barkNoise * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(65.0f + barkNoise * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(35.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 6: { // Oak Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.3f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(160.0f + ring * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(120.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(70.0f + ring * 20.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 7: // Standard Oak Leaves
                        case 38:
                        case 39:
                        case 40:
                        case 41:
                        case 42: {
                            uint8_t r = (uint8_t)std::clamp(45.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(155.0f + n * 50.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(35.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 8: { // Sand
                            uint8_t r = (uint8_t)std::clamp(220.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(200.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(130.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 9: { // Sky Quartz
                            uint8_t r = (uint8_t)std::clamp(210.0f + n * 45.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(235.0f + n * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(255.0f + n * 10.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 10: { // Water
                            uint8_t r = (uint8_t)std::clamp(35.0f + n * 20.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(115.0f + n * 35.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(205.0f + n * 35.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 11: { // Birch Log Side
                            float stripe = (py % 4 == 0 && (px + (py / 4) * 5) % 7 < 3) ? 0.25f : 0.95f;
                            uint8_t val = (uint8_t)std::clamp(230.0f * stripe + n * 25.0f, 0.0f, 255.0f);
                            setPixel(px, py, val, val, (uint8_t)(val * 0.95f));
                            break;
                        }
                        case 12: { // Birch Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.2f) * 0.2f + n * 0.15f;
                            uint8_t r = (uint8_t)std::clamp(220.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(195.0f + ring * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(140.0f + ring * 20.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 13: // Light Spring Oak Leaves
                        case 43:
                        case 44:
                        case 45:
                        case 46:
                        case 47: {
                            uint8_t r = (uint8_t)std::clamp(65.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(180.0f + n * 50.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 14: { // Pine Log Side
                            float barkNoise = std::sin(px * 2.0f) * 0.25f + n * 0.35f;
                            uint8_t r = (uint8_t)std::clamp(75.0f + barkNoise * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(45.0f + barkNoise * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(25.0f + barkNoise * 15.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 15: { // Pine Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.5f) * 0.25f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(130.0f + ring * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(85.0f + ring * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + ring * 15.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 16: { // Pine Leaves
                            uint8_t r = (uint8_t)std::clamp(20.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(100.0f + n * 45.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + n * 35.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 17: { // Dark Log Side
                            float barkNoise = std::sin(px * 1.8f) * 0.2f + n * 0.3f;
                            uint8_t r = (uint8_t)std::clamp(55.0f + barkNoise * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(35.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(20.0f + barkNoise * 15.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 18: { // Dark Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.2f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(90.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(60.0f + ring * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(35.0f + ring * 15.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 19: // Dark Forest Oak Leaves
                        case 48:
                        case 49:
                        case 50:
                        case 51:
                        case 52: {
                            uint8_t r = (uint8_t)std::clamp(25.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(125.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(25.0f + n * 25.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 20: { // Cherry Log Side
                            float barkNoise = std::sin(px * 1.5f) * 0.15f + n * 0.25f;
                            uint8_t r = (uint8_t)std::clamp(80.0f + barkNoise * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(45.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(45.0f + barkNoise * 20.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 21: { // Cherry Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.2f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(150.0f + ring * 35.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(95.0f + ring * 25.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(90.0f + ring * 25.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 22: { // Cherry Leaves
                            uint8_t r = (uint8_t)std::clamp(245.0f + n * 15.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(140.0f + n * 50.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(185.0f + n * 40.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 23: // Warm Golden-Tipped Oak Leaves
                        case 53:
                        case 54:
                        case 55:
                        case 56:
                        case 57: {
                            uint8_t r = (uint8_t)std::clamp(85.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(160.0f + n * 45.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(30.0f + n * 25.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 24: { // Magic Log Side
                            bool rune = (px + py * 3 + (int)(n * 10)) % 7 == 0;
                            uint8_t r = rune ? 40 : (uint8_t)std::clamp(45.0f + n * 25.0f, 0.0f, 255.0f);
                            uint8_t g = rune ? 220 : (uint8_t)std::clamp(25.0f + n * 20.0f, 0.0f, 255.0f);
                            uint8_t b = rune ? 255 : (uint8_t)std::clamp(85.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 25: { // Magic Log Top
                            float radius = std::sqrt((px - 7.5f) * (px - 7.5f) + (py - 7.5f) * (py - 7.5f));
                            float ring = std::sin(radius * 2.0f) * 0.3f + n * 0.2f;
                            uint8_t r = (uint8_t)std::clamp(110.0f + ring * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(70.0f + ring * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(220.0f + ring * 35.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 26: { // Magic Leaves
                            uint8_t r = (uint8_t)std::clamp(30.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(225.0f + n * 30.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(245.0f + n * 10.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 27: { // Golden Leaves
                            uint8_t r = (uint8_t)std::clamp(245.0f + n * 10.0f, 0.0f, 255.0f);
                            uint8_t g = (uint8_t)std::clamp(205.0f + n * 40.0f, 0.0f, 255.0f);
                            uint8_t b = (uint8_t)std::clamp(30.0f + n * 30.0f, 0.0f, 255.0f);
                            setPixel(px, py, r, g, b);
                            break;
                        }
                        case 29: // Meadow grass lower
                        case 30: // Fine grass lower
                        case 31: // Seeded grass lower
                        case 32: // Meadow grass upper
                        case 33: // Fine grass upper
                        case 34: // Seeded grass upper
                        case 35: // Meadow grass two-tall lower
                        case 36: // Fine grass two-tall lower
                        case 37: { // Seeded grass two-tall lower
                            // The rare two-block plant is authored as one
                            // logical 16x32 sprite. The upper and lower atlas
                            // tiles use the same global Y coordinate, so a
                            // blade that crosses the split is mathematically
                            // continuous at the block boundary.
                            if (tileID >= 32) {
                                bool upperHalf = tileID <= 34;
                                int variant = upperHalf ? tileID - 32 : tileID - 35;
                                float spriteHeight = upperHalf
                                    ? static_cast<float>(31 - py)
                                    : static_cast<float>(15 - py);
                                float spriteNoise = randNoise(
                                    px,
                                    static_cast<int>(spriteHeight),
                                    500 + variant * 17
                                ) * 0.15f;
                                auto onTallBlade = [&](float baseX, float lean,
                                                       float bladeHeight, float width) {
                                    if (spriteHeight < 1.0f || spriteHeight > bladeHeight) return false;
                                    float t = spriteHeight / bladeHeight;
                                    float centerX = baseX + lean * t;
                                    float bladeWidth = width * (1.0f - 0.45f * t) *
                                        (upperHalf ? 0.85f : 1.0f);
                                    return std::abs((static_cast<float>(px) + 0.5f) - centerX) <= bladeWidth;
                                };

                                bool blade = false;
                                if (variant == 0) {
                                    blade = onTallBlade(2.4f, -1.2f, 22.0f, 0.9f) ||
                                        onTallBlade(5.2f, 1.3f, 31.0f, 1.05f) ||
                                        onTallBlade(8.0f, -0.7f, 31.0f, 1.2f) ||
                                        onTallBlade(10.7f, 1.1f, 27.0f, 1.0f) ||
                                        onTallBlade(13.2f, -1.0f, 24.0f, 0.9f);
                                } else if (variant == 1) {
                                    blade = onTallBlade(2.1f, 1.0f, 25.0f, 0.9f) ||
                                        onTallBlade(4.9f, -1.1f, 31.0f, 1.0f) ||
                                        onTallBlade(7.4f, 0.8f, 24.0f, 0.85f) ||
                                        onTallBlade(9.8f, -1.3f, 31.0f, 1.1f) ||
                                        onTallBlade(12.8f, 0.9f, 27.0f, 0.95f);
                                } else {
                                    blade = onTallBlade(2.8f, -0.9f, 23.0f, 0.9f) ||
                                        onTallBlade(5.8f, 1.2f, 29.0f, 1.0f) ||
                                        onTallBlade(8.1f, -0.8f, 31.0f, 1.1f) ||
                                        onTallBlade(10.9f, 1.0f, 26.0f, 0.95f) ||
                                        onTallBlade(13.3f, -0.7f, 21.0f, 0.85f);
                                }

                                float seedX = variant == 1 ? 9.8f : 8.0f;
                                bool seedHead = variant == 2 &&
                                    spriteHeight >= 27.0f && spriteHeight <= 31.0f &&
                                    std::abs((static_cast<float>(px) + 0.5f) - seedX) <= 1.0f;
                                bool rootedTuft = !upperHalf && spriteHeight <= 2.0f &&
                                    px >= 1 && px <= 14 && ((px + py + tileID) % 3 != 0);

                                if (blade || rootedTuft || seedHead) {
                                    float shade = spriteNoise * 0.8f +
                                        (((px * 3 + static_cast<int>(spriteHeight) + variant) % 7 == 0)
                                            ? 0.12f : 0.0f);
                                    // Use the exact same palette as the normal
                                    // grass sprites below. Only the sprite
                                    // layout differs between the two variants.
                                    uint8_t r = seedHead
                                        ? (uint8_t)std::clamp(150.0f + shade * 35.0f, 0.0f, 255.0f)
                                        : (uint8_t)std::clamp(30.0f + shade * 35.0f, 0.0f, 255.0f);
                                    uint8_t g = seedHead
                                        ? (uint8_t)std::clamp(170.0f + shade * 35.0f, 0.0f, 255.0f)
                                        : (uint8_t)std::clamp(118.0f + shade * 70.0f, 0.0f, 255.0f);
                                    uint8_t b = seedHead
                                        ? (uint8_t)std::clamp(55.0f + shade * 20.0f, 0.0f, 255.0f)
                                        : (uint8_t)std::clamp(30.0f + shade * 35.0f, 0.0f, 255.0f);
                                    setPixel(px, py, r, g, b, 255);
                                } else {
                                    setPixel(px, py, 0, 0, 0, 0);
                                }
                                break;
                            }

                            float height = static_cast<float>(15 - py);
                            bool upperHalf = tileID >= 32 && tileID <= 34;
                            bool specializedLower = tileID >= 35;
                            int grassVariant = specializedLower
                                ? (tileID - 35) % 3
                                : (tileID - 29) % 3;
                            auto onBlade = [&](float baseX, float lean, float bladeHeight, float width) {
                                if (height < 1.0f || height > bladeHeight) return false;
                                float t = height / bladeHeight;
                                float centerX = baseX + lean * t;
                                float upperWidthScale = upperHalf ? 0.85f : 1.0f;
                                float bladeWidth = width * (1.0f - 0.45f * t) * upperWidthScale;
                                return std::abs((static_cast<float>(px) + 0.5f) - centerX) <= bladeWidth;
                            };

                            bool blade = false;
                            bool seedHead = false;
                            if (grassVariant == 2) {
                                blade = onBlade(3.0f, 0.65f, 9.0f, 0.85f) ||
                                    onBlade(6.0f, -0.8f, 13.0f, 0.95f) ||
                                    onBlade(8.3f, 0.45f, 15.0f, 1.0f) ||
                                    onBlade(11.0f, -0.65f, 11.0f, 0.9f) ||
                                    onBlade(13.4f, 0.5f, 8.0f, 0.8f);
                                float seedX = (px < 8) ? 6.0f : 8.3f;
                                seedHead = height >= 12.0f && height <= 15.0f &&
                                    std::abs((static_cast<float>(px) + 0.5f) - seedX) <= 1.0f;
                            } else if (grassVariant == 0) {
                                blade = onBlade(2.5f, -0.8f, 8.0f, 0.85f) ||
                                    onBlade(5.3f, 0.9f, 12.0f, 1.0f) ||
                                    onBlade(8.0f, -0.45f, 15.0f, 1.1f) ||
                                    onBlade(10.7f, 0.75f, 10.0f, 0.9f) ||
                                    onBlade(13.2f, -0.7f, 7.0f, 0.8f);
                            } else {
                                blade = onBlade(2.2f, 0.7f, 10.0f, 0.9f) ||
                                    onBlade(4.8f, -0.55f, 14.0f, 0.95f) ||
                                    onBlade(7.2f, 0.35f, 9.0f, 0.8f) ||
                                    onBlade(9.6f, -0.75f, 15.0f, 1.05f) ||
                                    onBlade(12.8f, 0.55f, 11.0f, 0.9f);
                            }

                            // Upper sections stay narrower at their base so
                            // the two-block plant reads as one continuous stem.
                            // Opaque pixels at the bottom make the blades meet
                            // the soil instead of appearing to hover above it.
                            bool twoTallStem = specializedLower && height >= 6.0f && height <= 12.0f &&
                                px >= 7 && px <= 8;
                            bool rootedTuft = !upperHalf && height <= 2.0f && px >= 1 && px <= 14 &&
                                ((px + py + tileID) % 3 != 0);
                            if (blade || rootedTuft || seedHead || twoTallStem) {
                                float highlight = ((px * 3 + py + tileID) % 7 == 0) ? 1.0f : 0.0f;
                                float shade = n * 0.8f + highlight * 0.12f;
                                uint8_t r = 0;
                                uint8_t g = 0;
                                uint8_t b = 0;
                                if (seedHead) {
                                    r = (uint8_t)std::clamp(150.0f + shade * 35.0f, 0.0f, 255.0f);
                                    g = (uint8_t)std::clamp(170.0f + shade * 35.0f, 0.0f, 255.0f);
                                    b = (uint8_t)std::clamp(55.0f + shade * 20.0f, 0.0f, 255.0f);
                                } else {
                                    r = (uint8_t)std::clamp(30.0f + shade * 35.0f, 0.0f, 255.0f);
                                    g = (uint8_t)std::clamp(118.0f + shade * 70.0f, 0.0f, 255.0f);
                                    b = (uint8_t)std::clamp(30.0f + shade * 35.0f, 0.0f, 255.0f);
                                }
                                setPixel(px, py, r, g, b, 255);
                            } else {
                                setPixel(px, py, 0, 0, 0, 0);
                            }
                            break;
                        }
                        case 59: { // Snow
                            uint8_t value =
                                static_cast<uint8_t>(std::clamp(242.0f + n * 18.0f, 0.0f, 255.0f));

                            setPixel(
                                px,
                                py,
                                value,
                                value,
                                static_cast<uint8_t>(std::min(255, value + 3))
                            );
                            break;
                        }
                        default: { // Fallback magenta pattern
                            bool alt = (px + py) % 2 == 0;
                            setPixel(px, py, alt ? 255 : 0, 0, alt ? 255 : 0);
                            break;
                        }
                    }
                }
            }
        }
        for (int tileID = 60; tileID < MAX_TILES; ++tileID) {
            currentTileID = tileID;
            for (int py = 0; py < TILE_SIZE; ++py) {
                for (int px = 0; px < TILE_SIZE; ++px) {
                    bool alt = (px + py) % 2 == 0;
                    setPixel(px, py, alt ? 255 : 0, 0, alt ? 255 : 0, 255);
                }
            }
        }

        GLuint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texId);

        constexpr int mipLevels = 5; // 16, 8, 4, 2, 1
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipLevels, GL_RGBA8, TILE_SIZE, TILE_SIZE, MAX_TILES);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, TILE_SIZE, TILE_SIZE, MAX_TILES, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (epoxy_has_gl_extension("GL_EXT_texture_filter_anisotropic")) {
            GLfloat maxAniso = 0.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
            glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(maxAniso, 8.0f));
        }

        return texId;
    }
};

#endif // TEXTURE_ATLAS_HPP
```

## WorldGen.hpp

**Path:** `WorldGen.hpp` | **Lines:** 1166 | **Size:** 47536 bytes

```cpp
#ifndef WORLD_GEN_HPP
#define WORLD_GEN_HPP

#include "MathUtils.hpp"
#include "SimplexNoise.hpp"
#include "Block.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

class WorldGen {
private:
    static inline int64_t floorDiv(int64_t a, int64_t b) {
        return (a >= 0) ? (a / b) : ((a - b + 1) / b);
    }


    static inline uint64_t treeHash(int64_t tx, int64_t tz) {
        uint64_t h = static_cast<uint64_t>(tx) * 0x9E3779B185EBCA87ULL ^ static_cast<uint64_t>(tz) * 0xC2B2AE3D27D4EB4FULL;
        h ^= h >> 30;
        h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 27;
        h *= 0x94D049BB133111EBULL;
        h ^= h >> 31;
        return h;
    }

    struct SurfaceCacheEntry {
        int64_t x = -9999999;
        int64_t z = -9999999;
        int64_t surfaceY = -1000000;
    };

    static inline int64_t getSurfaceYAtCached(int64_t wx, int64_t wz) {
        thread_local SurfaceCacheEntry cache[512];
        uint32_t slot = static_cast<uint32_t>((wx * 73856093LL ^ wz * 19349663LL) & 511);
        if (cache[slot].x == wx && cache[slot].z == wz) {
            return cache[slot].surfaceY;
        }
        int64_t sy = getSurfaceYAt(wx, wz, -50, 250);
        cache[slot] = { wx, wz, sy };
        return sy;
    }
public:

    static inline int64_t getSurfaceYAt(
        int64_t wx,
        int64_t wz,
        int64_t minY = -1000,
        int64_t maxY = 300
    ) {
        constexpr int64_t SURFACE_SCAN_STEP = 8;
        int64_t startY = std::min<int64_t>(
            300,
            maxY + SURFACE_SCAN_STEP
        );
        int64_t stopY = std::max<int64_t>(-1000, minY);
        for (int64_t y = startY; y >= stopY; y -= SURFACE_SCAN_STEP) {
            if (getDensity(wx, y, wz, 1) > 0.0f) {
                for (int64_t ry = y + SURFACE_SCAN_STEP; ry >= y; --ry) {
                    if (getDensity(wx, ry, wz, 1) > 0.0f && getDensity(wx, ry + 1, wz, 1) <= 0.0f) {
                        return ry;
                    }
                }
                return y;
            }
        }
        return -1000000;
    }
    static inline float smoothBand(float value) {
        float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    static inline float getIslandDensityBlend(float worldY) {
        return smoothBand((worldY + 500.0f) / 500.0f);
    }

    static inline float layerBoundaryFade(float progress) {
        // Keep the surface layer's upper edge soft across roughly 175 blocks.
        return smoothBand(progress * 8.0f) * smoothBand((1.0f - progress) * 4.0f);
    }

    static float getNegativeLayerDensity(
        int64_t wx,
        int64_t wy,
        int64_t wz,
        int scale
    ) {
        constexpr int64_t MIN_NEGATIVE_WORLD_Y = -1000;
        constexpr int64_t MAX_NEGATIVE_DENSITY_Y = -300;
        constexpr float NEGATIVE_DENSITY_HEIGHT = 700.0f;
        if (wy < MIN_NEGATIVE_WORLD_Y || wy > MAX_NEGATIVE_DENSITY_Y) {
            return -1.0f;
        }

        float progress = static_cast<float>(wy - MIN_NEGATIVE_WORLD_Y) /
            NEGATIVE_DENSITY_HEIGHT;
        float macro = SimplexNoise::octave3D(
            static_cast<float>(wx + 2371) * 0.0022f,
            0.0f,
            static_cast<float>(wz - 1789) * 0.0022f,
            3,
            0.5f,
            2.0f
        );
        float local = SimplexNoise::eval3D(
            static_cast<float>(wx - 1013) * 0.012f,
            static_cast<float>(wy + 431) * 0.012f,
            static_cast<float>(wz + 701) * 0.012f
        );
        float ridged = 1.0f - std::abs(SimplexNoise::eval3D(
            static_cast<float>(wx + 811) * 0.008f,
            static_cast<float>(wy - 613) * 0.010f,
            static_cast<float>(wz - 947) * 0.008f
        ));
        float heightField = 0.44f + macro * 0.20f + local * 0.06f;
        float density = (heightField - progress) * 3.2f + ridged * 0.18f;
        float hollowNoise = SimplexNoise::eval3D(
            static_cast<float>(wx) * 0.018f,
            static_cast<float>(wy) * 0.018f,
            static_cast<float>(wz) * 0.018f
        );
        if (hollowNoise > 0.62f) {
            density -= (hollowNoise - 0.62f) * 2.8f;
        }
        density += scale > 1 ? local * 0.06f : local * 0.12f;
        return density * layerBoundaryFade(progress);
    }
    static inline float distToSegmentSq(float px, float py, float pz, float x0, float y0, float z0, float x1, float y1, float z1) {
        float vx = x1 - x0, vy = y1 - y0, vz = z1 - z0;
        float wx = px - x0, wy = py - y0, wz = pz - z0;
        float c1 = wx * vx + wy * vy + wz * vz;
        if (c1 <= 0.0f) return wx * wx + wy * wy + wz * wz;
        float c2 = vx * vx + vy * vy + vz * vz;
        if (c2 <= c1) {
            float dx = px - x1, dy = py - y1, dz = pz - z1;
            return dx * dx + dy * dy + dz * dz;
        }
        float b = c1 / c2;
        float bx = x0 + b * vx, by = y0 + b * vy, bz = z0 + b * vz;
        float dx = px - bx, dy = py - by, dz = pz - bz;
        return dx * dx + dy * dy + dz * dz;
    }

    static inline bool inEllipsoid(float px, float py, float pz, float cx, float cy, float cz, float rx, float ry, float rz, uint64_t seed) {
        float dx = (px - cx) / rx;
        float dy = (py - cy) / ry;
        float dz = (pz - cz) / rz;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > 1.35f) return false;
        uint64_t pSeed = seed ^ treeHash(static_cast<int64_t>(px), static_cast<int64_t>(pz)) ^ (static_cast<int64_t>(py) * 1337);
        float noise = ((pSeed % 100) / 100.0f - 0.5f) * 0.35f;
        return (distSq + noise) <= 1.0f;
    }
    struct TreeCandidateCacheEntry {
        int64_t tx = 0;
        int64_t tz = 0;
        int64_t groundY = -999;
        int layer = -2;
        uint64_t seed = 0;
        int roll = 0;
        uint8_t leafBlock = BLOCK_AIR;
        uint8_t logBlock = BLOCK_OAK_LOG;
        bool valid = false;
    };
public:
    static constexpr int NEGATIVE_LAYER_COUNT = 1;
    static constexpr int64_t NEGATIVE_LAYER_HEIGHT = 500;

    static int getNegativeLayerIndex(int64_t wy) {
        if (wy < -1000 || wy > -500) return -1;
        return 0;
    }

    static const char* getNegativeLayerName(int layer) {
        static constexpr const char* names[NEGATIVE_LAYER_COUNT] = {
            "surface layer"
        };
        if (layer < 0 || layer >= NEGATIVE_LAYER_COUNT) return "overworld";
        return names[layer];
    }

    static bool isNegativeWorldY(int64_t wy) {
        return wy >= -1000 && wy <= -500;
    }
    // 0 keeps the atmosphere in the surface-layer palette; 1 reaches the
    // floating-island palette at the top of the transition void.
    static float getSurfaceLayerBlend(float worldY) {
        // Atmosphere transitions more slowly than terrain so the layer
        // colors do not snap while crossing the empty space.
        return smoothBand((worldY + 750.0f) / 1000.0f);
    }

    static constexpr int64_t TREE_MAX_RADIUS = 8;
    // evaluates the tree block at one world position.
    struct TreeSite {
        int64_t tx;
        int64_t tz;
        int64_t groundY;
        uint64_t seed;
        int roll;
        uint8_t leafBlock;
        uint8_t logBlock;
    };

    static uint8_t evaluateTreeSite(
        const TreeSite& tree,
        int64_t wx,
        int64_t wy,
        int64_t wz,
        int scale
    ) {
        int64_t tx = tree.tx;
        int64_t tz = tree.tz;
        int64_t groundY = tree.groundY;
        uint64_t seed = tree.seed;
        int roll = tree.roll;
        uint8_t leafBlock = tree.leafBlock;
        uint8_t logBlock = tree.logBlock;

        float fwx = static_cast<float>(wx);
        float fwy = static_cast<float>(wy);
        float fwz = static_cast<float>(wz);

        if (scale >= 2) {
            int height = 8 + static_cast<int>((seed >> 8) % 7);
            if (roll < 6) height = 4;
            else if (roll >= 94) height = 20;

            if (wx == tx && wz == tz && wy > groundY && wy <= groundY + height) {
                return logBlock;
            }
            float cdx = static_cast<float>(wx - tx);
            float cdy = static_cast<float>(wy - (groundY + height - 2));
            float cdz = static_cast<float>(wz - tz);
            if (cdx * cdx + cdy * cdy * 1.2f + cdz * cdz <= 16.0f) {
                return leafBlock;
            }
            return BLOCK_AIR;
        }

        float ftx = static_cast<float>(tx);
        float fty = static_cast<float>(groundY);
        float ftz = static_cast<float>(tz);

        if (roll < 6) {
            int height = 3 + static_cast<int>((seed >> 8) % 3);
            float topY = fty + height;
            int64_t leanX = (seed & 1) ? 1 : 0;
            int64_t leanZ = ((seed >> 1) & 1) ? 1 : 0;

            if (wy > groundY && wy <= groundY + height) {
                int64_t curX = (wy > groundY + 2) ? (tx + leanX) : tx;
                int64_t curZ = (wy > groundY + 2) ? (tz + leanZ) : tz;
                if (wx == curX && wz == curZ) return logBlock;
            }
            if (inEllipsoid(fwx, fwy, fwz, ftx + leanX, topY, ftz + leanZ,
                    1.8f, 1.4f, 1.8f, seed + 1)) {
                return leafBlock;
            }
        } else if (roll >= 94) {
            int height = 18 + static_cast<int>((seed >> 8) % 9);
            float topY = fty + height;

            if (wx == tx && wz == tz && fwy > fty && fwy <= topY) return logBlock;

            int branchY1 = height - 12;
            int branchY2 = height - 7;
            float bLen1 = 5.5f;
            float bLen2 = 4.5f;

            if (distToSegmentSq(fwx, fwy, fwz, ftx + 1, fty + branchY1, ftz,
                    ftx + bLen1, fty + branchY1 + 2, ftz + 1) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz,
                    ftx - bLen1, fty + branchY1 + 2, ftz - 1) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz + 1,
                    ftx - 1, fty + branchY1 + 2, ftz + bLen1) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz,
                    ftx + 1, fty + branchY1 + 2, ftz - bLen1) <= 0.8f) return logBlock;

            if (distToSegmentSq(fwx, fwy, fwz, ftx + 1, fty + branchY2, ftz,
                    ftx + bLen2, fty + branchY2 + 2, ftz - 2) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY2, ftz,
                    ftx - bLen2, fty + branchY2 + 2, ftz + 2) <= 0.8f) return logBlock;

            if (inEllipsoid(fwx, fwy, fwz, ftx + bLen1, fty + branchY1 + 2,
                    ftz + 1, 3.2f, 2.2f, 3.2f, seed + 1)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx - bLen1, fty + branchY1 + 2,
                    ftz - 1, 3.2f, 2.2f, 3.2f, seed + 2)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx - 1, fty + branchY1 + 2,
                    ftz + bLen1, 3.2f, 2.2f, 3.2f, seed + 3)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx + 1, fty + branchY1 + 2,
                    ftz - bLen1, 3.2f, 2.2f, 3.2f, seed + 4)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx + bLen2, fty + branchY2 + 2,
                    ftz - 2, 3.2f, 2.2f, 3.2f, seed + 5)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx - bLen2, fty + branchY2 + 2,
                    ftz + 2, 3.2f, 2.2f, 3.2f, seed + 6)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, ftx, topY + 2, ftz,
                    4.0f, 2.8f, 4.0f, seed + 7)) return leafBlock;
        } else {
            int height = 7 + static_cast<int>((seed >> 8) % 9);
            float topY = fty + height;
            float leanAngle = static_cast<float>((seed >> 12) % 360) * 0.0174533f;
            float leanAmount = 0.8f + static_cast<float>((seed >> 16) % 15) * 0.1f;

            for (int dy = 1; dy <= height; ++dy) {
                float progress = static_cast<float>(dy) / static_cast<float>(height);
                float curveX = std::sin(progress * 2.5f + leanAngle) * (progress * leanAmount);
                float curveZ = std::cos(progress * 2.5f + leanAngle) * (progress * leanAmount);
                int64_t curX = tx + static_cast<int64_t>(std::round(curveX));
                int64_t curZ = tz + static_cast<int64_t>(std::round(curveZ));
                if (wx == curX && wz == curZ && wy == groundY + dy) return logBlock;
            }

            float finalCurveX = std::sin(2.5f + leanAngle) * leanAmount;
            float finalCurveZ = std::cos(2.5f + leanAngle) * leanAmount;
            float tipX = ftx + finalCurveX;
            float tipZ = ftz + finalCurveZ;
            float branchY = topY - 3.0f;
            float br1X = tipX + 3.0f;
            float br1Z = tipZ + 1.5f;
            float br2X = tipX - 2.8f;
            float br2Z = tipZ - 2.0f;
            float br3X = tipX + 1.0f;
            float br3Z = tipZ - 3.2f;

            if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ,
                    br1X, topY - 1.0f, br1Z) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ,
                    br2X, topY - 1.0f, br2Z) <= 0.8f) return logBlock;
            if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ,
                    br3X, topY - 1.0f, br3Z) <= 0.8f) return logBlock;

            if (inEllipsoid(fwx, fwy, fwz, br1X, topY - 1.0f, br1Z,
                    2.8f, 2.0f, 2.8f, seed + 1)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, br2X, topY - 1.0f, br2Z,
                    2.8f, 2.0f, 2.8f, seed + 2)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, br3X, topY - 1.0f, br3Z,
                    2.8f, 2.0f, 2.8f, seed + 3)) return leafBlock;
            if (inEllipsoid(fwx, fwy, fwz, tipX, topY + 1.2f, tipZ,
                    3.6f, 2.4f, 3.6f, seed + 4)) return leafBlock;
        }

        return BLOCK_AIR;
    }

    static void collectTreeSites(
        int64_t minWX,
        int64_t maxWX,
        int64_t minWY,
        int64_t maxWY,
        int64_t minWZ,
        int64_t maxWZ,
        std::vector<TreeSite>& outTrees
    ) {
        outTrees.clear();
        constexpr int64_t CELL_SIZE = 5;
        int64_t minCX = floorDiv(minWX - TREE_MAX_RADIUS, CELL_SIZE);
        int64_t maxCX = floorDiv(maxWX + TREE_MAX_RADIUS, CELL_SIZE);
        int64_t minCZ = floorDiv(minWZ - TREE_MAX_RADIUS, CELL_SIZE);
        int64_t maxCZ = floorDiv(maxWZ + TREE_MAX_RADIUS, CELL_SIZE);

        for (int64_t cz = minCZ; cz <= maxCZ; ++cz) {
            for (int64_t cx = minCX; cx <= maxCX; ++cx) {
                uint64_t hash =
                    static_cast<uint64_t>(cx) * 0x9E3779B185EBCA87ULL ^
                    static_cast<uint64_t>(cz) * 0xC2B2AE3D27D4EB4FULL;
                hash ^= hash >> 30;
                hash *= 0xBF58476D1CE4E5B9ULL;
                hash ^= hash >> 27;

                int64_t tx = cx * CELL_SIZE + static_cast<int64_t>(hash % CELL_SIZE);
                int64_t tz = cz * CELL_SIZE + static_cast<int64_t>((hash >> 8) % CELL_SIZE);
                if (tx < minWX - TREE_MAX_RADIUS || tx > maxWX + TREE_MAX_RADIUS ||
                    tz < minWZ - TREE_MAX_RADIUS || tz > maxWZ + TREE_MAX_RADIUS) {
                    continue;
                }

                float lakeNoise = getLakeNoise(tx, tz);
                float floraPatchNoise = getFloraPatchNoise(tx, tz);
                if (lakeNoise >= 0.72f || floraPatchNoise <= 0.40f) continue;
                int64_t groundY = getSurfaceYAt(tx, tz, minWY - 32, maxWY);
                if (groundY < -100000 || maxWY < groundY || minWY > groundY + 32) continue;
                int layer = getNegativeLayerIndex(groundY);
                if (groundY < 0 && layer != 0) continue;

                // groundY from getSurfaceYAt already guarantees groundDensity > 0 and aboveGroundDensity <= 0

                uint64_t seed = treeHash(tx, tz);
                int roll = static_cast<int>(seed % 100);
                uint8_t leafShade[4] = {
                    BLOCK_LEAVES,
                    BLOCK_LEAVES_LIGHT,
                    BLOCK_LEAVES_DARK,
                    BLOCK_LEAVES_WARM
                };
                outTrees.push_back({
                    tx,
                    tz,
                    groundY,
                    seed,
                    roll,
                    leafShade[(seed >> 4) % 4],
                    BLOCK_OAK_LOG
                });
            }
        }
    }

    static uint8_t getTreeBlockFromSites(
        const std::vector<TreeSite>& trees,
        int64_t wx,
        int64_t wy,
        int64_t wz,
        int scale
    ) {
        if (wy < -1000 || wy > 300) return BLOCK_AIR;
        for (const TreeSite& tree : trees) {
            if (std::abs(wx - tree.tx) > TREE_MAX_RADIUS ||
                std::abs(wz - tree.tz) > TREE_MAX_RADIUS ||
                wy < tree.groundY ||
                wy > tree.groundY + 32) {
                continue;
            }
            uint8_t block = evaluateTreeSite(tree, wx, wy, wz, scale);
            if (block != BLOCK_AIR) return block;
        }
        return BLOCK_AIR;
    }

    static uint8_t getTreeBlockAt(int64_t wx, int64_t wy, int64_t wz, int scale = 1) {
        if (wy < -1000 || wy > 300) return BLOCK_AIR;
        if (wy < 0) {
            int layer = getNegativeLayerIndex(wy);
            if (layer != 0) return BLOCK_AIR;
        }

        constexpr int64_t cellSize = 5;
        int64_t cellX = floorDiv(wx, cellSize);
        int64_t cellZ = floorDiv(wz, cellSize);

        float fwx = static_cast<float>(wx);
        float fwy = static_cast<float>(wy);
        float fwz = static_cast<float>(wz);

        for (int dz = -2; dz <= 2; ++dz) {
            for (int dx = -2; dx <= 2; ++dx) {
                int64_t cx = cellX + dx;
                int64_t cz = cellZ + dz;

                uint64_t hash = static_cast<uint64_t>(cx) * 0x9E3779B185EBCA87ULL ^ static_cast<uint64_t>(cz) * 0xC2B2AE3D27D4EB4FULL;
                hash ^= hash >> 30;
                hash *= 0xBF58476D1CE4E5B9ULL;
                hash ^= hash >> 27;

                int targetX = static_cast<int>(hash % cellSize);
                int targetZ = static_cast<int>((hash >> 8) % cellSize);

                int64_t tx = cx * cellSize + targetX;
                int64_t tz = cz * cellSize + targetZ;

                if (std::abs(wx - tx) > TREE_MAX_RADIUS ||
                    std::abs(wz - tz) > TREE_MAX_RADIUS) continue;
                int requestedLayer = getNegativeLayerIndex(wy);

                static thread_local TreeCandidateCacheEntry cache[2048];
                uint32_t slot = static_cast<uint32_t>(
                    (tx * 73856093LL ^ tz * 19349663LL) & 2047
                );
                TreeCandidateCacheEntry& candidate = cache[slot];

                if (candidate.tx != tx || candidate.tz != tz ||
                    candidate.layer != requestedLayer) {
                    candidate.tx = tx;
                    candidate.tz = tz;
                    candidate.layer = requestedLayer;
                    candidate.valid = false;
                    candidate.groundY = -999;

                    float lakeNoise = getLakeNoise(tx, tz);
                    float floraPatchNoise = getFloraPatchNoise(tx, tz);
                    if (lakeNoise < 0.72f && floraPatchNoise > 0.40f) {
                        int64_t cachedGroundY = -999;
                        if (requestedLayer >= 0) {
                            int64_t layerMin = -NEGATIVE_LAYER_HEIGHT *
                                static_cast<int64_t>(requestedLayer + 1);
                            int64_t layerMax = layerMin + NEGATIVE_LAYER_HEIGHT - 1;
                            cachedGroundY = getSurfaceYAt(
                                tx, tz, layerMin, layerMax
                            );
                        } else {
                            cachedGroundY = getSurfaceYAtCached(tx, tz);
                        }
                        if (cachedGroundY > -100000) {
                            int groundLayer = getNegativeLayerIndex(cachedGroundY);
                            bool allowedLayer = requestedLayer < 0
                                ? cachedGroundY >= 0
                                : groundLayer == requestedLayer;
                            float aboveGroundDensity = getDensity(
                                tx, cachedGroundY + 1, tz, 1
                            );
                            float groundDensity = getDensity(tx, cachedGroundY, tz, 1);
                            if (allowedLayer && groundDensity > 0.0f &&
                                aboveGroundDensity <= 0.0f) {
                                candidate.groundY = cachedGroundY;
                                candidate.seed = treeHash(tx, tz);
                                candidate.roll = static_cast<int>(candidate.seed % 100);
                                uint8_t leafShade[4] = {
                                    BLOCK_LEAVES,
                                    BLOCK_LEAVES_LIGHT,
                                    BLOCK_LEAVES_DARK,
                                    BLOCK_LEAVES_WARM
                                };
                                candidate.leafBlock = leafShade[
                                    (candidate.seed >> 4) % 4
                                ];
                                candidate.logBlock = BLOCK_OAK_LOG;
                                candidate.valid = true;
                            }
                        }
                    }
                }

                if (!candidate.valid ||
                    wy < candidate.groundY ||
                    wy > candidate.groundY + 32) {
                    continue;
                }

                int64_t groundY = candidate.groundY;
                uint64_t seed = candidate.seed;
                int roll = candidate.roll;
                uint8_t leafBlock = candidate.leafBlock;
                uint8_t logBlock = candidate.logBlock;

                // Coarse LOD fast-path for scale >= 2 (LOD 1, 2, 3, 4)
                if (scale >= 2) {
                    int height = 8 + static_cast<int>((seed >> 8) % 7);
                    if (roll < 6) height = 4;
                    else if (roll >= 94) height = 20;

                    if (wx == tx && wz == tz && wy > groundY && wy <= groundY + height) {
                        return logBlock;
                    }
                    float cdx = static_cast<float>(wx - tx);
                    float cdy = static_cast<float>(wy - (groundY + height - 2));
                    float cdz = static_cast<float>(wz - tz);
                    if (cdx * cdx + cdy * cdy * 1.2f + cdz * cdz <= 16.0f) {
                        return leafBlock;
                    }
                    continue;
                }

                // LOD 0 Detail Oak Tree Generation
                float ftx = static_cast<float>(tx);
                float fty = static_cast<float>(groundY);
                float ftz = static_cast<float>(tz);

                if (roll < 6) {
                    // RARE TYPE 1: Tiny Sapling Oak (3-5 blocks tall, ~6% chance)
                    int height = 3 + static_cast<int>((seed >> 8) % 3);
                    float topY = fty + height;

                    int64_t leanX = (seed & 1) ? 1 : 0;
                    int64_t leanZ = ((seed >> 1) & 1) ? 1 : 0;

                    if (wy > groundY && wy <= groundY + height) {
                        int64_t curX = (wy > groundY + 2) ? (tx + leanX) : tx;
                        int64_t curZ = (wy > groundY + 2) ? (tz + leanZ) : tz;
                        if (wx == curX && wz == curZ) return logBlock;
                    }

                    if (inEllipsoid(fwx, fwy, fwz, ftx + leanX, topY, ftz + leanZ, 1.8f, 1.4f, 1.8f, seed + 1)) return leafBlock;
                }
                else if (roll >= 94) {
                    // RARE TYPE 2: Huge Elder Greatwood Oak (18-26 blocks tall, ~6% chance)
                    int height = 18 + static_cast<int>((seed >> 8) % 9);
                    float topY = fty + height;

                    if (wx == tx && wz == tz && fwy > fty && fwy <= topY) return logBlock;

                    int branchY1 = height - 12, branchY2 = height - 7;
                    float bLen1 = 5.5f, bLen2 = 4.5f;

                    if (distToSegmentSq(fwx, fwy, fwz, ftx + 1, fty + branchY1, ftz, ftx + bLen1, fty + branchY1 + 2, ftz + 1) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz, ftx - bLen1, fty + branchY1 + 2, ftz - 1) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz + 1, ftx - 1, fty + branchY1 + 2, ftz + bLen1) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY1, ftz, ftx + 1, fty + branchY1 + 2, ftz - bLen1) <= 0.8f) return logBlock;

                    if (distToSegmentSq(fwx, fwy, fwz, ftx + 1, fty + branchY2, ftz, ftx + bLen2, fty + branchY2 + 2, ftz - 2) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, ftx, fty + branchY2, ftz, ftx - bLen2, fty + branchY2 + 2, ftz + 2) <= 0.8f) return logBlock;

                    if (inEllipsoid(fwx, fwy, fwz, ftx + bLen1, fty + branchY1 + 2, ftz + 1, 3.2f, 2.2f, 3.2f, seed + 1)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx - bLen1, fty + branchY1 + 2, ftz - 1, 3.2f, 2.2f, 3.2f, seed + 2)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx - 1, fty + branchY1 + 2, ftz + bLen1, 3.2f, 2.2f, 3.2f, seed + 3)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx + 1, fty + branchY1 + 2, ftz - bLen1, 3.2f, 2.2f, 3.2f, seed + 4)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx + bLen2, fty + branchY2 + 2, ftz - 2, 3.2f, 2.2f, 3.2f, seed + 5)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx - bLen2, fty + branchY2 + 2, ftz + 2, 3.2f, 2.2f, 3.2f, seed + 6)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, ftx, topY + 2, ftz, 4.0f, 2.8f, 4.0f, seed + 7)) return leafBlock;
                }
                else {
                    // STANDARD VARIED OAK TREE (~88% of trees)
                    int height = 7 + static_cast<int>((seed >> 8) % 9);
                    float topY = fty + height;

                    float leanAngle = static_cast<float>((seed >> 12) % 360) * 0.0174533f;
                    float leanAmount = 0.8f + static_cast<float>((seed >> 16) % 15) * 0.1f;


                    for (int dy = 1; dy <= height; ++dy) {
                        float progress = static_cast<float>(dy) / static_cast<float>(height);
                        float curveX = std::sin(progress * 2.5f + leanAngle) * (progress * leanAmount);
                        float curveZ = std::cos(progress * 2.5f + leanAngle) * (progress * leanAmount);

                        int64_t curX = tx + static_cast<int64_t>(std::round(curveX));
                        int64_t curZ = tz + static_cast<int64_t>(std::round(curveZ));

                        if (wx == curX && wz == curZ && wy == groundY + dy) return logBlock;
                    }

                    float finalCurveX = std::sin(2.5f + leanAngle) * leanAmount;
                    float finalCurveZ = std::cos(2.5f + leanAngle) * leanAmount;
                    float tipX = ftx + finalCurveX;
                    float tipZ = ftz + finalCurveZ;

                    float branchY = topY - 3.0f;
                    float br1X = tipX + 3.0f, br1Z = tipZ + 1.5f;
                    float br2X = tipX - 2.8f, br2Z = tipZ - 2.0f;
                    float br3X = tipX + 1.0f, br3Z = tipZ - 3.2f;

                    if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ, br1X, topY - 1.0f, br1Z) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ, br2X, topY - 1.0f, br2Z) <= 0.8f) return logBlock;
                    if (distToSegmentSq(fwx, fwy, fwz, tipX, branchY, tipZ, br3X, topY - 1.0f, br3Z) <= 0.8f) return logBlock;

                    if (inEllipsoid(fwx, fwy, fwz, br1X, topY - 1.0f, br1Z, 2.8f, 2.0f, 2.8f, seed + 1)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, br2X, topY - 1.0f, br2Z, 2.8f, 2.0f, 2.8f, seed + 2)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, br3X, topY - 1.0f, br3Z, 2.8f, 2.0f, 2.8f, seed + 3)) return leafBlock;
                    if (inEllipsoid(fwx, fwy, fwz, tipX, topY + 1.2f, tipZ, 3.6f, 2.4f, 3.6f, seed + 4)) return leafBlock;
                }
            }
        }
        return BLOCK_AIR;
    }
    // The base field is shared by terrain and the underside formations below.
    // Keeping it separate prevents support probes from recursively sampling
    // the final density field.
    static float getFloatingIslandDensity(
        int64_t wx,
        int64_t wy,
        int64_t wz,
        int scale = 1
    ) {
        float fx = static_cast<float>(wx);
        float fy = static_cast<float>(wy);
        float fz = static_cast<float>(wz);

        // Domain warping (consistent across all LOD scales to prevent terrain shifting between LOD levels)
        float warpX = SimplexNoise::eval3D(fx * 0.005f, fy * 0.005f, fz * 0.005f) * 40.0f;
        float warpY = SimplexNoise::eval3D(fx * 0.005f + 100.0f, fy * 0.005f, fz * 0.005f) * 20.0f;
        float warpZ = SimplexNoise::eval3D(fx * 0.005f, fy * 0.005f, fz * 0.005f + 100.0f) * 40.0f;

        float wx_w = fx + warpX;
        float wy_w = fy + warpY;
        float wz_w = fz + warpZ;

        // Large scale 3D continent noise
        float nLarge = SimplexNoise::octave3D(wx_w * 0.004f, wy_w * 0.006f, wz_w * 0.004f, 3, 0.5f, 2.0f);

        // Low-frequency regional noise groups islands into broad cloud fields
        // instead of distributing similarly sized islands uniformly everywhere.
        float nMacro = SimplexNoise::octave3D(
            wx_w * 0.0012f,
            wy_w * 0.0020f,
            wz_w * 0.0012f,
            2,
            0.5f,
            2.0f
        );

        // Ridged secondary noise gives some islands spines, shelves, and
        // irregular silhouettes without changing the large island footprint.
        float nRidge = 1.0f - std::abs(SimplexNoise::eval3D(
            wx_w * 0.010f,
            wy_w * 0.012f,
            wz_w * 0.010f
        ));

        // Continuous 3D island field: there are no periodic vertical layers.
        // The formation threshold keeps the field open while the 3D noises
        // create isolated volumes at every height.
        float density = nLarge * 1.65f +
            nMacro * 0.35f +
            (nRidge - 0.45f) * 0.25f -
            0.75f;

        // Sparse low-frequency cavities create occasional arches and hollow
        // undersides while remaining part of the shared LOD density field.
        if (density > 0.15f) {
            float cavernNoise = SimplexNoise::eval3D(
                wx_w * 0.022f,
                wy_w * 0.022f,
                wz_w * 0.022f
            );
            if (cavernNoise > 0.68f) {
                density -= (cavernNoise - 0.68f) * 3.0f;
            }
        }

        // Fine detail 3D noise (low-pass filtered for coarse LOD scales)
        if (scale <= 4) {
            float nDetail = SimplexNoise::eval3D(wx_w * 0.02f, wy_w * 0.02f, wz_w * 0.02f) * 0.25f;
            density += nDetail;
        }

        return density;
    }

    // The surface layer and floating islands meet through the existing void,
    // so density eases from the layer's empty upper edge into the island
    // field instead of changing fields at a single Y coordinate.
    static float getBaseDensity(int64_t wx, int64_t wy, int64_t wz, int scale = 1) {
        if (wy < 0) {
            float surfaceDensity = getNegativeLayerDensity(wx, wy, wz, scale);
            if (wy <= -500) return surfaceDensity;

            // Blend the extended surface edge into the lower island field
            // instead of exposing a hard horizontal layer boundary.
            float islandBlend = getIslandDensityBlend(static_cast<float>(wy));
            if (wy > -300) surfaceDensity = 0.0f;
            float islandDensity = getFloatingIslandDensity(wx, wy, wz, scale);
            return surfaceDensity * (1.0f - islandBlend) +
                islandDensity * islandBlend;
        }

        return getFloatingIslandDensity(wx, wy, wz, scale);
    }
    static float getDensity(int64_t wx, int64_t wy, int64_t wz, int scale = 1) {
        return getBaseDensity(wx, wy, wz, scale);
    }



    // Broad 2D habitat fields used by post-terrain features. Keeping these
    // separate from density lets one biome contain distinct local habitats.
    static float getLakeNoise(int64_t wx, int64_t wz) {
        return SimplexNoise::octave3D(
            static_cast<float>(wx) * 0.0025f,
            0.0f,
            static_cast<float>(wz) * 0.0025f,
            2,
            0.5f,
            2.0f
        ) * 0.5f + 0.5f;
    }

    static float getFloraPatchNoise(int64_t wx, int64_t wz) {
        return SimplexNoise::octave3D(
            static_cast<float>(wx + 7919) * 0.0008f,
            0.0f,
            static_cast<float>(wz - 104729) * 0.0008f,
            2,
            0.5f,
            2.0f
        ) * 0.5f + 0.5f;
    }

    static float getFloraNoise(int64_t wx, int64_t wz) {
        float broad = getFloraPatchNoise(wx, wz) * 2.0f - 1.0f;
        float local = SimplexNoise::eval3D(
            static_cast<float>(wx) * 0.025f,
            0.0f,
            static_cast<float>(wz) * 0.025f
        );
        return std::clamp(0.5f + broad * 0.35f + local * 0.15f, 0.0f, 1.0f);
    }

    // Continuous habitat quality for grass. There is no hard patch boundary:
    // the value is mapped directly to the chance of placing a blade.
    static float getTallGrassHabitatNoise(int64_t wx, int64_t wz) {
        return SimplexNoise::octave3D(
            static_cast<float>(wx + 17321) * 0.012f,
            0.0f,
            static_cast<float>(wz - 9511) * 0.012f,
            3,
            0.5f,
            2.0f
        ) * 0.5f + 0.5f;
    }

    // Stable per-cell variation turns the continuous chance into actual
    // deterministic placements without adding world-state or save data.
    static float getTallGrassCellNoise(int64_t wx, int64_t wz) {
        uint64_t hash = static_cast<uint64_t>(wx) * 0x9E3779B185EBCA87ULL ^
            static_cast<uint64_t>(wz) * 0xC2B2AE3D27D4EB4FULL;
        hash ^= hash >> 30;
        hash *= 0xBF58476D1CE4E5B9ULL;
        hash ^= hash >> 27;
        hash *= 0x94D049BB133111EBULL;
        hash ^= hash >> 31;
        return static_cast<float>((hash >> 11) & 0x1FFFFFFFFFFFFFULL) /
            9007199254740992.0f;
    }

    // Keep zone boundaries deterministic in world space, but break up the
    // exact cutoff with a low-frequency field. The broad signal controls
    // where a zone lives; the transition noise only affects its edge, so
    // interiors remain stable and recognizable.
    static float getDesertBiomeNoise(int64_t wx, int64_t wz) {
        return SimplexNoise::eval3D(
            static_cast<float>(wx) * 0.001f,
            0.0f,
            static_cast<float>(wz) * 0.001f
        );
    }

    static bool isDesertZone(int64_t wx, int64_t wz) {
        constexpr float threshold = 0.4f;
        constexpr float transitionHalfWidth = 0.12f;
        float biomeNoise = getDesertBiomeNoise(wx, wz);

        if (biomeNoise <= threshold - transitionHalfWidth) return false;
        if (biomeNoise >= threshold + transitionHalfWidth) return true;

        float transitionNoise = SimplexNoise::octave3D(
            static_cast<float>(wx + 3749) * 0.018f,
            0.0f,
            static_cast<float>(wz - 9277) * 0.018f,
            3,
            0.5f,
            2.0f
        ) * 0.5f + 0.5f;
        float desertBlend = (biomeNoise - (threshold - transitionHalfWidth)) /
            (2.0f * transitionHalfWidth);
        return transitionNoise < desertBlend;
    }

    static bool isHighSkyZone(int64_t wx, int64_t wy, int64_t wz) {
        constexpr float boundaryY = 300.0f;
        constexpr float transitionHalfWidth = 8.0f;

        // Wobble the nominal altitude boundary in X/Z first. This gives the
        // zone a large readable shape instead of a perfectly level slice.
        float boundaryNoise = SimplexNoise::eval3D(
            static_cast<float>(wx - 6103) * 0.012f,
            0.0f,
            static_cast<float>(wz + 1187) * 0.012f
        );
        float boundary = boundaryY + boundaryNoise * 18.0f;
        float distance = static_cast<float>(wy) - boundary;

        if (distance <= -transitionHalfWidth) return false;
        if (distance >= transitionHalfWidth) return true;

        // Within the narrow edge band, select patches from a second field so
        // the boundary is ragged and interlocking instead of one voxel-wide.
        float transitionNoise = SimplexNoise::eval3D(
            static_cast<float>(wx + 14783) * 0.038f,
            static_cast<float>(wy - 3251) * 0.032f,
            static_cast<float>(wz - 4819) * 0.038f
        ) * 0.5f + 0.5f;
        float skyBlend = distance / (2.0f * transitionHalfWidth) + 0.5f;
        return transitionNoise < skyBlend;
    }

    // Deep Stone follows local geometric sharpness rather than world height
    // or random material noise. A broad flat underside has little change in
    // the horizontal density field, while a cliff edge or narrow tip is dark.
    static float getDeepStoneSharpness(
        int64_t wx,
        int64_t wy,
        int64_t wz,
        int scale,
        float density
    ) {
        int64_t nearStep = std::max(1, scale);
        int64_t farStep = nearStep * 3;

        float nearXPos = getDensity(wx + nearStep, wy, wz, scale);
        float nearXNeg = getDensity(wx - nearStep, wy, wz, scale);
        float nearZPos = getDensity(wx, wy, wz + nearStep, scale);
        float nearZNeg = getDensity(wx, wy, wz - nearStep, scale);
        float farXPos = getDensity(wx + farStep, wy, wz, scale);
        float farXNeg = getDensity(wx - farStep, wy, wz, scale);
        float farZPos = getDensity(wx, wy, wz + farStep, scale);
        float farZNeg = getDensity(wx, wy, wz - farStep, scale);

        float nearCurvature =
            std::abs(nearXPos + nearXNeg - 2.0f * density) +
            std::abs(nearZPos + nearZNeg - 2.0f * density);
        float farCurvature =
            std::abs(farXPos + farXNeg - 2.0f * density) +
            std::abs(farZPos + farZNeg - 2.0f * density);
        float nearSlope =
            std::abs(nearXPos - nearXNeg) +
            std::abs(nearZPos - nearZNeg);
        float farSlope =
            std::abs(farXPos - farXNeg) +
            std::abs(farZPos - farZNeg);

        float sharpness = nearCurvature * 0.55f +
            farCurvature * 0.30f +
            nearSlope * 0.10f +
            farSlope * 0.05f;
        float transition = std::clamp((sharpness - 0.18f) / 0.82f, 0.0f, 1.0f);
        return transition * transition * (3.0f - 2.0f * transition);
    }

    static float getDeepStoneSharpnessFromValues(
        float density,
        float nearXPos, float nearXNeg,
        float nearZPos, float nearZNeg,
        float farXPos, float farXNeg,
        float farZPos, float farZNeg
    ) {
        float nearCurvature =
            std::abs(nearXPos + nearXNeg - 2.0f * density) +
            std::abs(nearZPos + nearZNeg - 2.0f * density);
        float farCurvature =
            std::abs(farXPos + farXNeg - 2.0f * density) +
            std::abs(farZPos + farZNeg - 2.0f * density);
        float nearSlope =
            std::abs(nearXPos - nearXNeg) +
            std::abs(nearZPos - nearZNeg);
        float farSlope =
            std::abs(farXPos - farXNeg) +
            std::abs(farZPos - farZNeg);

        float sharpness = nearCurvature * 0.55f +
            farCurvature * 0.30f +
            nearSlope * 0.10f +
            farSlope * 0.05f;
        float transition = std::clamp((sharpness - 0.18f) / 0.82f, 0.0f, 1.0f);
        return transition * transition * (3.0f - 2.0f * transition);
    }


    static uint8_t getNegativeLayerBlock(
        int layer,
        int64_t wx,
        int64_t wy,
        int64_t wz,
        bool surface,
        bool shallow,
        float sharpness
    ) {
        (void)layer; (void)wx; (void)wy; (void)wz;
        if (surface) return BLOCK_GRASS;
        if (shallow) return BLOCK_DIRT;
        if (sharpness > 0.50f) return BLOCK_DEEP_STONE;
        return BLOCK_STONE;
    }

    static uint8_t getBlockAtWithDensitiesAndSharpness(
        int64_t wx,
        int64_t wy,
        int64_t wz,
        int scale,
        float density,
        float aboveDensity,
        float above2Density,
        float sharpness
    ) {
        if (density <= 0.0f) {
            return BLOCK_AIR;
        }

        int layer = getNegativeLayerIndex(wy);
        if (layer >= 0) {
            return getNegativeLayerBlock(
                layer,
                wx,
                wy,
                wz,
                aboveDensity <= 0.0f,
                above2Density <= 0.0f,
                sharpness
            );
        }

        if (isHighSkyZone(wx, wy, wz)) {
            if (aboveDensity <= 0.0f) return BLOCK_SKY_QUARTZ;
            if (sharpness > 0.50f) return BLOCK_DEEP_STONE;
            return BLOCK_STONE;
        }

        bool isDesert = isDesertZone(wx, wz);

        if (aboveDensity <= 0.0f) {
            if (isDesert) return BLOCK_SAND;
            return BLOCK_GRASS;
        }

        if (above2Density <= 0.0f) {
            if (isDesert) return BLOCK_SAND;
            return BLOCK_DIRT;
        }

        if (scale <= 4) {
            float crystalNoise = SimplexNoise::eval3D(
                static_cast<float>(wx) * 0.015f,
                static_cast<float>(wy) * 0.015f,
                static_cast<float>(wz) * 0.015f
            );
            if (crystalNoise > 0.78f && density < 0.2f) return BLOCK_GLOW_CRYSTAL;
        }

        if (sharpness > 0.50f) return BLOCK_DEEP_STONE;

        return BLOCK_STONE;
    }
    // One jittered site per small world cell gives forests an even, natural
    // distribution without the clumping caused by local-noise maxima.
    static bool isTreeSite(int64_t wx, int64_t wz) {
        constexpr int64_t cellSize = 5;
        int64_t localX = wx % cellSize;
        int64_t localZ = wz % cellSize;
        if (localX < 0) localX += cellSize;
        if (localZ < 0) localZ += cellSize;

        int64_t cellX = (wx - localX) / cellSize;
        int64_t cellZ = (wz - localZ) / cellSize;
        uint64_t hash = static_cast<uint64_t>(cellX) * 0x9E3779B185EBCA87ULL;
        hash ^= static_cast<uint64_t>(cellZ) + 0xC2B2AE3D27D4EB4FULL +
            (hash << 6) + (hash >> 2);
        hash ^= hash >> 30;
        hash *= 0xBF58476D1CE4E5B9ULL;
        hash ^= hash >> 27;

        int targetX = static_cast<int>(hash % cellSize);
        int targetZ = static_cast<int>((hash >> 8) % cellSize);
        return localX == targetX && localZ == targetZ;
    }

    // Evaluates the block type at world position (wx, wy, wz)
    static uint8_t getBlockAt(int64_t wx, int64_t wy, int64_t wz, int scale = 1) {
        float density = getDensity(wx, wy, wz, scale);

        if (density <= 0.0f) {
            if (wy < 0) return BLOCK_AIR;
            return getTreeBlockAt(wx, wy, wz, scale);
        }

        // Check block directly above to determine surface
        float aboveDensity = getDensity(wx, wy + scale, wz, scale);

        int layer = getNegativeLayerIndex(wy);
        if (layer >= 0) {
            float above2Density = getDensity(wx, wy + 4 * scale, wz, scale);
            return getNegativeLayerBlock(
                layer,
                wx,
                wy,
                wz,
                aboveDensity <= 0.0f,
                above2Density <= 0.0f,
                0.0f
            );
        }

        // High sky island biome with a noisy spatial transition around its
        // nominal altitude boundary.
        if (isHighSkyZone(wx, wy, wz)) {
            if (aboveDensity <= 0.0f) return BLOCK_SKY_QUARTZ;
            if (getDeepStoneSharpness(wx, wy, wz, scale, density) > 0.50f) {
                return BLOCK_DEEP_STONE;
            }
            return BLOCK_STONE;
        }

        // Desert floating island biome with a noisy edge between regional
        // habitats instead of a single hard contour.
        bool isDesert = isDesertZone(wx, wz);

        if (aboveDensity <= 0.0f) {
            // Surface block
            if (isDesert) return BLOCK_SAND;
            return BLOCK_GRASS;
        }

        // Check depth below surface
        float above2Density = getDensity(wx, wy + 4 * scale, wz, scale);
        if (above2Density <= 0.0f) {
            if (isDesert) return BLOCK_SAND;
            return BLOCK_DIRT;
        }

        // Underside or deep interior crystal lights (rare, elegant illumination)
        if (scale <= 4) {
            float crystalNoise = SimplexNoise::eval3D(wx * 0.015f, wy * 0.015f, wz * 0.015f);
            if (crystalNoise > 0.78f && density < 0.2f) {
                return BLOCK_GLOW_CRYSTAL;
            }
        }

        if (getDeepStoneSharpness(wx, wy, wz, scale, density) > 0.50f) {
            return BLOCK_DEEP_STONE;
        }

        return BLOCK_STONE;
    }

    static uint8_t getBlockAtWithDensities(
        int64_t wx,
        int64_t wy,
        int64_t wz,
        int scale,
        float density,
        float aboveDensity,
        float above2Density
    ) {
        if (density <= 0.0f) {
            return BLOCK_AIR;
        }

        int layer = getNegativeLayerIndex(wy);
        if (layer >= 0) {
            float sharpness = getDeepStoneSharpness(wx, wy, wz, scale, density);
            return getNegativeLayerBlock(
                layer,
                wx,
                wy,
                wz,
                aboveDensity <= 0.0f,
                above2Density <= 0.0f,
                sharpness
            );
        }

        if (isHighSkyZone(wx, wy, wz)) {
            if (aboveDensity <= 0.0f) return BLOCK_SKY_QUARTZ;
            if (getDeepStoneSharpness(wx, wy, wz, scale, density) > 0.50f) {
                return BLOCK_DEEP_STONE;
            }
            return BLOCK_STONE;
        }

        bool isDesert = isDesertZone(wx, wz);

        if (aboveDensity <= 0.0f) {
            if (isDesert) return BLOCK_SAND;
            return BLOCK_GRASS;
        }

        if (above2Density <= 0.0f) {
            if (isDesert) return BLOCK_SAND;
            return BLOCK_DIRT;
        }

        if (scale <= 4) {
            float crystalNoise = SimplexNoise::eval3D(
                static_cast<float>(wx) * 0.015f,
                static_cast<float>(wy) * 0.015f,
                static_cast<float>(wz) * 0.015f
            );
            if (crystalNoise > 0.78f && density < 0.2f) return BLOCK_GLOW_CRYSTAL;
        }

        if (getDeepStoneSharpness(wx, wy, wz, scale, density) > 0.50f) {
            return BLOCK_DEEP_STONE;
        }

        return BLOCK_STONE;
    }
};

#endif // WORLD_GEN_HPP
```

## WorldGenerator.cpp

**Path:** `WorldGenerator.cpp` | **Lines:** 412 | **Size:** 16822 bytes

```cpp
#include "WorldGenerator.hpp"
#include "LightingSystem.hpp"

static thread_local std::vector<uint8_t> paddedBlocks;
static thread_local std::vector<uint16_t> paddedLight;

static inline uint8_t getPaddedBlock(const Chunk& chunk, int x, int y, int z) {
    if (paddedBlocks.empty()) return chunk.getBlock(x, y, z);
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) return BLOCK_AIR;
    return paddedBlocks[getPaddedVoxelIndex(x, y, z)];
}

static inline uint16_t getPaddedLight(const Chunk& chunk, int x, int y, int z) {
    if (paddedLight.empty()) return chunk.getPaddedLight(x, y, z);
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) {
        return chunk.getPaddedLight(x, y, z);
    }
    return paddedLight[getPaddedVoxelIndex(x, y, z)];
}

static inline void setPaddedLight(int x, int y, int z, uint16_t l) {
    if (paddedLight.empty()) return;
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) return;
    paddedLight[getPaddedVoxelIndex(x, y, z)] = l;
}

static void finalizeVoxelData(
    Chunk& chunk,
    const std::vector<float>* densityGrid = nullptr,
    MeshingNeighborhood* neighborhood = nullptr
) {
    bool hasSolid = false;
    for (int z = 0; z < CHUNK_SIZE && !hasSolid; ++z) {
        for (int y = 0; y < CHUNK_SIZE && !hasSolid; ++y) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                if (chunk.getBlock(x, y, z) != BLOCK_AIR) {
                    hasSolid = true;
                    break;
                }
            }
        }
    }

    chunk.isEmpty = !hasSolid;
    if (chunk.isEmpty) {
        paddedBlocks.clear();
        paddedLight.clear();
        std::fill(chunk.light, chunk.light + CHUNK_VOL, 0);
        if (neighborhood) {
            neighborhood->blocks.fill(BLOCK_AIR);
            neighborhood->light.fill(0);
        }
        chunk.isGenerated = true;
        return;
    }

    int scale = chunk.scale;
    int64_t wmx = chunk.worldMin.x;
    int64_t wmy = chunk.worldMin.y;
    int64_t wmz = chunk.worldMin.z;
    paddedBlocks.resize(PADDED_VOL);

    constexpr int GRID_DX = CHUNK_SIZE + 7; // 39
    constexpr int GRID_DZ = CHUNK_SIZE + 7; // 39
    auto gridIndex = [](int x, int y, int z) {
        return ((y + 1) * GRID_DZ + (z + 3)) * GRID_DX + (x + 3);
    };

    auto sampleDensity = [&](int x, int y, int z) {
        return (*densityGrid)[gridIndex(
            std::clamp(x, -3, 35),
            std::clamp(y, -1, 37),
            std::clamp(z, -3, 35)
        )];
    };

    for (int y = -1; y <= CHUNK_SIZE; ++y) {
        for (int z = -1; z <= CHUNK_SIZE; ++z) {
            for (int x = -1; x <= CHUNK_SIZE; ++x) {
                uint8_t block = BLOCK_AIR;
                bool inside = x >= 0 && x < CHUNK_SIZE &&
                    y >= 0 && y < CHUNK_SIZE &&
                    z >= 0 && z < CHUNK_SIZE;
                if (inside) {
                    block = chunk.getBlock(x, y, z);
                } else if (densityGrid) {
                    float density = sampleDensity(x, y, z);
                    if (density <= 0.0f) {
                        block = BLOCK_AIR;
                    } else {
                        float aboveDensity = sampleDensity(x, y + 1, z);
                        float above2Density = sampleDensity(x, y + 4, z);

                        float nearXPos = sampleDensity(x + 1, y, z);
                        float nearXNeg = sampleDensity(x - 1, y, z);
                        float nearZPos = sampleDensity(x, y, z + 1);
                        float nearZNeg = sampleDensity(x, y, z - 1);
                        float farXPos = sampleDensity(x + 3, y, z);
                        float farXNeg = sampleDensity(x - 3, y, z);
                        float farZPos = sampleDensity(x, y, z + 3);
                        float farZNeg = sampleDensity(x, y, z - 3);

                        float sharpness = WorldGen::getDeepStoneSharpnessFromValues(
                            density, nearXPos, nearXNeg, nearZPos, nearZNeg,
                            farXPos, farXNeg, farZPos, farZNeg
                        );

                        int64_t wx = wmx + x * scale + scale / 2;
                        int64_t wy = wmy + y * scale + scale / 2;
                        int64_t wz = wmz + z * scale + scale / 2;
                        block = WorldGen::getBlockAtWithDensitiesAndSharpness(
                            wx, wy, wz, scale, density, aboveDensity, above2Density, sharpness
                        );
                    }
                } else {
                    int64_t wx = wmx + x * scale + scale / 2;
                    int64_t wy = wmy + y * scale + scale / 2;
                    int64_t wz = wmz + z * scale + scale / 2;
                    block = WorldGen::getBlockAt(wx, wy, wz, scale);
                }
                paddedBlocks[getPaddedVoxelIndex(x, y, z)] = block;
            }
        }
    }
    LightingSystem::propagateLocalLight3D(chunk);
    if (neighborhood) {
        std::copy(
            paddedBlocks.begin(),
            paddedBlocks.end(),
            neighborhood->blocks.begin()
        );
        std::copy(
            chunk.light,
            chunk.light + CHUNK_VOL,
            neighborhood->light.begin()
        );
    }
    chunk.isGenerated = true;
}

void WorldGenerator::generateVoxelData(Chunk& chunk, MeshingNeighborhood* neighborhood) {
    bool hasSolid = false;
    (void)hasSolid;
    int scale = chunk.scale;
    int64_t wmx = chunk.worldMin.x;
    int64_t wmy = chunk.worldMin.y;
    int64_t wmz = chunk.worldMin.z;

    constexpr int GRID_SIZE = 39; // ix, iy, iz: 0 to 38
    constexpr int GRID_VOL = GRID_SIZE * GRID_SIZE * GRID_SIZE; // 59,319

    thread_local std::vector<float> densityGrid;
    densityGrid.resize(GRID_VOL);

    auto gridIndex = [](int x, int y, int z) {
        return ((y + 1) * GRID_SIZE + (z + 3)) * GRID_SIZE + (x + 3);
    };

    for (int iy = 0; iy < GRID_SIZE; ++iy) {
        int y = iy - 1;
        int64_t wy = wmy + y * scale + scale / 2;
        for (int iz = 0; iz < GRID_SIZE; ++iz) {
            int z = iz - 3;
            int64_t wz = wmz + z * scale + scale / 2;
            for (int ix = 0; ix < GRID_SIZE; ++ix) {
                int x = ix - 3;
                int64_t wx = wmx + x * scale + scale / 2;
                densityGrid[gridIndex(x, y, z)] =
                    WorldGen::getDensity(wx, wy, wz, scale);
            }
        }
    }
    int64_t minSampleX = wmx + scale / 2;
    int64_t maxSampleX = wmx + (CHUNK_SIZE - 1) * scale + scale / 2;
    int64_t minSampleY = wmy + scale / 2;
    int64_t maxSampleY = wmy + (CHUNK_SIZE - 1) * scale + scale / 2;
    int64_t minSampleZ = wmz + scale / 2;
    int64_t maxSampleZ = wmz + (CHUNK_SIZE - 1) * scale + scale / 2;

    thread_local std::vector<WorldGen::TreeSite> treeSites;
    WorldGen::collectTreeSites(
        minSampleX,
        maxSampleX,
        minSampleY,
        maxSampleY,
        minSampleZ,
        maxSampleZ,
        treeSites
    );

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                int64_t wx = wmx + x * scale + scale / 2;
                int64_t wy = wmy + y * scale + scale / 2;
                int64_t wz = wmz + z * scale + scale / 2;
                float density = densityGrid[gridIndex(x, y, z)];
                if (density <= 0.0f) {
                    chunk.setBlock(x, y, z, BLOCK_AIR);
                    continue;
                }
                float aboveDensity = densityGrid[gridIndex(x, y + 1, z)];
                float above2Density = densityGrid[gridIndex(x, y + 4, z)];

                float nearXPos = densityGrid[gridIndex(x + 1, y, z)];
                float nearXNeg = densityGrid[gridIndex(x - 1, y, z)];
                float nearZPos = densityGrid[gridIndex(x, y, z + 1)];
                float nearZNeg = densityGrid[gridIndex(x, y, z - 1)];
                float farXPos = densityGrid[gridIndex(x + 3, y, z)];
                float farXNeg = densityGrid[gridIndex(x - 3, y, z)];
                float farZPos = densityGrid[gridIndex(x, y, z + 3)];
                float farZNeg = densityGrid[gridIndex(x, y, z - 3)];

                float sharpness = WorldGen::getDeepStoneSharpnessFromValues(
                    density, nearXPos, nearXNeg, nearZPos, nearZNeg,
                    farXPos, farXNeg, farZPos, farZNeg
                );

                uint8_t block = WorldGen::getBlockAtWithDensitiesAndSharpness(
                    wx, wy, wz, scale, density, aboveDensity, above2Density, sharpness
                );
                chunk.setBlock(x, y, z, block);

                if (block != BLOCK_AIR) {
                    hasSolid = true;
                }
            }
        }
    }

    auto floorDiv = [](int64_t a, int64_t b) {
        int64_t quotient = a / b;
        int64_t remainder = a % b;
        if (remainder != 0 && ((a < 0) ^ (b < 0))) --quotient;
        return quotient;
    };
    auto ceilDiv = [&](int64_t a, int64_t b) {
        return -floorDiv(-a, b);
    };
    auto cellRange = [&](int64_t minWorld, int64_t maxWorld, int64_t worldOrigin) {
        int64_t minCell = ceilDiv(minWorld - worldOrigin - scale / 2, scale);
        int64_t maxCell = floorDiv(maxWorld - worldOrigin - scale / 2, scale);
        return std::pair<int, int>(
            static_cast<int>(std::max<int64_t>(0, minCell)),
            static_cast<int>(std::min<int64_t>(CHUNK_SIZE - 1, maxCell))
        );
    };

    for (const WorldGen::TreeSite& tree : treeSites) {
        auto xRange = cellRange(
            tree.tx - WorldGen::TREE_MAX_RADIUS,
            tree.tx + WorldGen::TREE_MAX_RADIUS,
            wmx
        );
        auto yRange = cellRange(tree.groundY, tree.groundY + 32, wmy);
        auto zRange = cellRange(
            tree.tz - WorldGen::TREE_MAX_RADIUS,
            tree.tz + WorldGen::TREE_MAX_RADIUS,
            wmz
        );
        if (xRange.first > xRange.second ||
            yRange.first > yRange.second ||
            zRange.first > zRange.second) {
            continue;
        }

        for (int z = zRange.first; z <= zRange.second; ++z) {
            for (int y = yRange.first; y <= yRange.second; ++y) {
                for (int x = xRange.first; x <= xRange.second; ++x) {
                    if (densityGrid[gridIndex(x, y, z)] > 0.0f ||
                        chunk.getBlock(x, y, z) != BLOCK_AIR) {
                        continue;
                    }
                    int64_t wx = wmx + x * scale + scale / 2;
                    int64_t wy = wmy + y * scale + scale / 2;
                    int64_t wz = wmz + z * scale + scale / 2;
                    uint8_t block = WorldGen::evaluateTreeSite(
                        tree, wx, wy, wz, scale
                    );
                    if (block != BLOCK_AIR) {
                        chunk.setBlock(x, y, z, block);
                        hasSolid = true;
                    }
                }
            }
        }
    }

    if (scale == 1) {
        auto findSurfaceY = [&](int x, int z) -> int {
            for (int y = CHUNK_SIZE - 1; y >= 0; --y) {
                uint8_t block = chunk.getBlock(x, y, z);
                uint8_t above = y + 1 < CHUNK_SIZE
                    ? chunk.getBlock(x, y + 1, z)
                    : WorldGen::getBlockAt(wmx + x, wmy + y + 1, wmz + z, 1);
                if (block != BLOCK_AIR && block != BLOCK_WATER && above == BLOCK_AIR) {
                    return y;
                }
            }
            return -1;
        };

        int surfaceHeights[CHUNK_SIZE * CHUNK_SIZE];
        std::fill(surfaceHeights, surfaceHeights + CHUNK_SIZE * CHUNK_SIZE, -1);
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                surfaceHeights[z * CHUNK_SIZE + x] = findSurfaceY(x, z);
            }
        }

        auto surfaceAt = [&](int x, int z) -> int {
            if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) return -1;
            return surfaceHeights[z * CHUNK_SIZE + x];
        };

        for (int z = 3; z < CHUNK_SIZE - 3; ++z) {
            for (int x = 3; x < CHUNK_SIZE - 3; ++x) {
                int surfaceY = surfaceAt(x, z);
                if (surfaceY < 1) continue;

                int64_t wx = wmx + x;
                int64_t wz = wmz + z;
                float lakeNoise = WorldGen::getLakeNoise(wx, wz);
                bool lakePeak = lakeNoise > WorldGen::getLakeNoise(wx + 12, wz) &&
                    lakeNoise > WorldGen::getLakeNoise(wx - 12, wz) &&
                    lakeNoise > WorldGen::getLakeNoise(wx, wz + 12) &&
                    lakeNoise > WorldGen::getLakeNoise(wx, wz - 12);

                if (chunk.getBlock(x, surfaceY, z) == BLOCK_GRASS &&
                    lakeNoise > 0.64f && lakePeak) {
                    int rimMinY = CHUNK_SIZE;
                    bool completeRim = true;
                    for (int dz = -3; dz <= 3; ++dz) {
                        for (int dx = -3; dx <= 3; ++dx) {
                            if (std::max(std::abs(dx), std::abs(dz)) != 3) continue;
                            int rimY = surfaceAt(x + dx, z + dz);
                            if (rimY < 0) completeRim = false;
                            else rimMinY = std::min(rimMinY, rimY);
                        }
                    }

                    bool completeBasin = true;
                    bool flatBasin = true;
                    for (int dz = -2; dz <= 2 && completeBasin; ++dz) {
                        for (int dx = -2; dx <= 2; ++dx) {
                            int basinY = surfaceAt(x + dx, z + dz);
                            if (basinY < 0 || basinY > surfaceY) {
                                completeBasin = false;
                                break;
                            }
                            if (basinY != surfaceY) flatBasin = false;
                        }
                    }

                    bool naturalBasin = completeBasin && !flatBasin && rimMinY >= surfaceY + 1;
                    bool plateauPond = flatBasin && rimMinY >= surfaceY;
                    if (completeRim && (naturalBasin || plateauPond)) {
                        int waterLevel = std::min(surfaceY + 1, rimMinY);
                        for (int dz = -2; dz <= 2; ++dz) {
                            for (int dx = -2; dx <= 2; ++dx) {
                                int basinY = surfaceAt(x + dx, z + dz);
                                if (basinY < 0 || basinY >= waterLevel) continue;

                                if (flatBasin && chunk.getBlock(x + dx, basinY, z + dz) == BLOCK_GRASS) {
                                    chunk.setBlock(x + dx, basinY, z + dz, BLOCK_WATER);
                                } else {
                                    for (int y = basinY + 1; y <= waterLevel; ++y) {
                                        if (chunk.getBlock(x + dx, y, z + dz) == BLOCK_AIR) {
                                            chunk.setBlock(x + dx, y, z + dz, BLOCK_WATER);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                int surfaceY = surfaceAt(x, z);
                if (surfaceY < 0 || surfaceY + 1 >= CHUNK_SIZE) continue;
                if (chunk.getBlock(x, surfaceY, z) != BLOCK_GRASS ||
                    chunk.getBlock(x, surfaceY + 1, z) != BLOCK_AIR) {
                    continue;
                }

                int64_t wx = wmx + x;
                int64_t wz = wmz + z;
                if (WorldGen::getLakeNoise(wx, wz) >= 0.72f) continue;

                float grassChance = 0.08f +
                    WorldGen::getTallGrassHabitatNoise(wx, wz) * 0.72f;
                if (WorldGen::getTallGrassCellNoise(wx, wz) > grassChance) {
                    continue;
                }
                chunk.setBlock(x, surfaceY + 1, z, BLOCK_TALL_GRASS);

                bool rareTwoTall = WorldGen::getTallGrassCellNoise(
                    wx + 104729,
                    wz - 7919
                ) < 0.10f;
                if (rareTwoTall && surfaceY + 2 < CHUNK_SIZE &&
                    chunk.getBlock(x, surfaceY + 2, z) == BLOCK_AIR) {
                    chunk.setBlock(x, surfaceY + 2, z, BLOCK_TALL_GRASS_TOP);
                }
            }
        }
    }
    finalizeVoxelData(chunk, &densityGrid, neighborhood);
}
```

## WorldGenerator.hpp

**Path:** `WorldGenerator.hpp` | **Lines:** 24 | **Size:** 798 bytes

```cpp
#ifndef WORLD_GENERATOR_HPP
#define WORLD_GENERATOR_HPP

#include "Chunk.hpp"
#include "WorldGen.hpp"
#include "Block.hpp"
#include "MathUtils.hpp"
#include "MeshingNeighborhood.hpp"

class WorldGenerator {
public:
    static uint8_t getBlockAt(int64_t wx, int64_t wy, int64_t wz, int scale = 1) {
        return WorldGen::getBlockAt(wx, wy, wz, scale);
    }
    static float getDensity(int64_t wx, int64_t wy, int64_t wz, int scale = 1) {
        return WorldGen::getDensity(wx, wy, wz, scale);
    }
    static int64_t getSurfaceYAt(int64_t wx, int64_t wz, int64_t minY = -1000, int64_t maxY = 250) {
        return WorldGen::getSurfaceYAt(wx, wz, minY, maxY);
    }
    static void generateVoxelData(Chunk& chunk, MeshingNeighborhood* neighborhood = nullptr);
};

#endif // WORLD_GENERATOR_HPP
```

