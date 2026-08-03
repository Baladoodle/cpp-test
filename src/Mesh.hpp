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
    float lodLevel;       // LOD level for shader fog/fade
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

struct alignas(16) TraversalNodeGpu {
    float chunkMinLod[4] = {};
    float sectionBounds[4] = {};
    uint32_t topology[4] = {}; // first child, child count, complete, geometry
    uint32_t draw0[4] = {}; // count, instances, first index, base vertex bits
    uint32_t draw1[4] = {}; // base instance, reserved
};

static_assert(sizeof(SectionGpuMetadata) == 32, "SSBO metadata must remain two vec4 values");
static_assert(sizeof(TraversalNodeGpu) == 80, "Traversal node layout must match std430");
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
    GLuint metadataBuffer = 0;
    GLuint indirectBuffer = 0;
    GLuint nodeBuffer = 0;
    GLuint rootBuffer = 0;
    GLuint childLinkBuffer = 0;
    GLuint traversalQueueA = 0;
    GLuint traversalQueueB = 0;
    GLuint traversalCounterBuffer = 0;
    GLsync lastSubmittedFence = 0;
    bool initialized = false;

    size_t vertexCapacity = 0;
    size_t indexCapacity = 0;
    std::vector<FreeRange> freeVertexRanges;
    std::vector<FreeRange> freeIndexRanges;
    size_t metadataCapacityBytes = 0;
    size_t indirectCapacityBytes = 0;
    size_t nodeCapacityBytes = 0;
    size_t rootCapacityBytes = 0;
    size_t childLinkCapacityBytes = 0;
    size_t traversalQueueCapacityBytes = 0;

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
        glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, lodLevel));
        glEnableVertexAttribArray(8);
        glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, windWeight));
        glBindVertexArray(0);
    }

    void growVertexStorage(size_t required) {
        size_t oldCapacity = vertexCapacity;
        size_t newCapacity = std::max(required, std::max<size_t>(1, oldCapacity * 2));
        releaseRange(freeVertexRanges, oldCapacity, newCapacity - oldCapacity);

        // Move the existing arena on the GPU. Re-uploading the entire CPU
        // mirror here caused multi-hundred-millisecond render-thread stalls
        // whenever the arena crossed a capacity boundary.
        GLuint oldVbo = vbo;
        GLuint newVbo = 0;
        glGenBuffers(1, &newVbo);
        glBindBuffer(GL_COPY_WRITE_BUFFER, newVbo);
        glBufferData(
            GL_COPY_WRITE_BUFFER,
            static_cast<GLsizeiptr>(newCapacity * sizeof(VoxelVertex)),
            nullptr,
            GL_DYNAMIC_DRAW
        );
        glBindBuffer(GL_COPY_READ_BUFFER, oldVbo);
        glCopyBufferSubData(
            GL_COPY_READ_BUFFER,
            GL_COPY_WRITE_BUFFER,
            0,
            0,
            static_cast<GLsizeiptr>(oldCapacity * sizeof(VoxelVertex))
        );
        glDeleteBuffers(1, &oldVbo);
        vbo = newVbo;
        vertexCapacity = newCapacity;
        configureVertexArray();
    }

    void growIndexStorage(size_t required) {
        size_t oldCapacity = indexCapacity;
        size_t newCapacity = std::max(required, std::max<size_t>(1, oldCapacity * 2));
        releaseRange(freeIndexRanges, oldCapacity, newCapacity - oldCapacity);

        GLuint oldEbo = ebo;
        GLuint newEbo = 0;
        glGenBuffers(1, &newEbo);
        glBindBuffer(GL_COPY_WRITE_BUFFER, newEbo);
        glBufferData(
            GL_COPY_WRITE_BUFFER,
            static_cast<GLsizeiptr>(newCapacity * sizeof(uint32_t)),
            nullptr,
            GL_DYNAMIC_DRAW
        );
        glBindBuffer(GL_COPY_READ_BUFFER, oldEbo);
        glCopyBufferSubData(
            GL_COPY_READ_BUFFER,
            GL_COPY_WRITE_BUFFER,
            0,
            0,
            static_cast<GLsizeiptr>(oldCapacity * sizeof(uint32_t))
        );
        glDeleteBuffers(1, &oldEbo);
        ebo = newEbo;
        indexCapacity = newCapacity;
        configureVertexArray();
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
    bool initialize(size_t initialVertices = 8u << 20, size_t initialIndices = 24u << 20) {
        if (initialized) return true;

        vertexCapacity = initialVertices;
        indexCapacity = initialIndices;
        freeVertexRanges.push_back({0, initialVertices});
        freeIndexRanges.push_back({0, initialIndices});

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glGenBuffers(1, &metadataBuffer);
        glGenBuffers(1, &indirectBuffer);
        glGenBuffers(1, &nodeBuffer);
        glGenBuffers(1, &rootBuffer);
        glGenBuffers(1, &childLinkBuffer);
        glGenBuffers(1, &traversalQueueA);
        glGenBuffers(1, &traversalQueueB);
        glGenBuffers(1, &traversalCounterBuffer);

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
        return true;
    }

    void cleanUp() {
        if (lastSubmittedFence) glDeleteSync(lastSubmittedFence);
        lastSubmittedFence = 0;
        if (childLinkBuffer) glDeleteBuffers(1, &childLinkBuffer);
        if (traversalCounterBuffer) glDeleteBuffers(1, &traversalCounterBuffer);
        if (traversalQueueB) glDeleteBuffers(1, &traversalQueueB);
        if (traversalQueueA) glDeleteBuffers(1, &traversalQueueA);
        if (rootBuffer) glDeleteBuffers(1, &rootBuffer);
        if (nodeBuffer) glDeleteBuffers(1, &nodeBuffer);
        if (indirectBuffer) glDeleteBuffers(1, &indirectBuffer);
        if (metadataBuffer) glDeleteBuffers(1, &metadataBuffer);
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = vbo = ebo = metadataBuffer = indirectBuffer = 0;
        nodeBuffer = rootBuffer = childLinkBuffer = 0;
        traversalQueueA = traversalQueueB = traversalCounterBuffer = 0;
        initialized = false;
        vertexCapacity = 0;
        indexCapacity = 0;
        freeVertexRanges.clear();
        freeIndexRanges.clear();
        metadataCapacityBytes = 0;
        indirectCapacityBytes = 0;
        nodeCapacityBytes = 0;
        rootCapacityBytes = 0;
        childLinkCapacityBytes = 0;
        traversalQueueCapacityBytes = 0;
    }

    void waitForSubmittedWork() {
        if (!lastSubmittedFence) return;
        for (;;) {
            GLenum result = glClientWaitSync(lastSubmittedFence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000);
            if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED) break;
            if (result == GL_WAIT_FAILED) {
                glFinish();
                break;
            }
        }
        glDeleteSync(lastSubmittedFence);
        lastSubmittedFence = 0;
    }

    void markSubmitted() {
        if (lastSubmittedFence) glDeleteSync(lastSubmittedFence);
        lastSubmittedFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    GeometryHandle upload(const std::vector<VoxelVertex>& vertices, const std::vector<uint32_t>& indices) {
        GeometryHandle handle;
        if (!initialized || vertices.empty() || indices.empty()) return handle;

        size_t vertexStart = allocateRange(freeVertexRanges, vertices.size());
        if (vertexStart == std::numeric_limits<size_t>::max()) {
            growVertexStorage(vertexCapacity + vertices.size());
            vertexStart = allocateRange(freeVertexRanges, vertices.size());
        }

        size_t indexStart = allocateRange(freeIndexRanges, indices.size());
        if (indexStart == std::numeric_limits<size_t>::max()) {
            growIndexStorage(indexCapacity + indices.size());
            indexStart = allocateRange(freeIndexRanges, indices.size());
        }

        if (vertexStart == std::numeric_limits<size_t>::max() ||
            indexStart == std::numeric_limits<size_t>::max()) {
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
        releaseRange(freeVertexRanges, handle.vertexOffset, handle.vertexCount);
        releaseRange(freeIndexRanges, handle.indexOffset, handle.indexCount);
    }

    void uploadDrawData(
        const std::vector<SectionGpuMetadata>& metadata,
        const std::vector<DrawElementsIndirectCommand>& commands
    ) {
        if (!initialized || metadata.size() != commands.size()) return;

        size_t metadataBytes = metadata.size() * sizeof(SectionGpuMetadata);
        size_t indirectBytes = commands.size() * sizeof(DrawElementsIndirectCommand);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, metadataBuffer);
        if (metadataBytes > metadataCapacityBytes) {
            metadataCapacityBytes = std::max(metadataBytes, std::max<size_t>(sizeof(SectionGpuMetadata), metadataCapacityBytes * 2));
            glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(metadataCapacityBytes), nullptr, GL_STREAM_DRAW);
        }
        if (metadataBytes > 0) {
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(metadataBytes), metadata.data());
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        if (indirectBytes > indirectCapacityBytes) {
            indirectCapacityBytes = std::max(indirectBytes, std::max<size_t>(sizeof(DrawElementsIndirectCommand), indirectCapacityBytes * 2));
            glBufferData(GL_DRAW_INDIRECT_BUFFER, static_cast<GLsizeiptr>(indirectCapacityBytes), nullptr, GL_STREAM_DRAW);
        }
        if (indirectBytes > 0) {
            glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, static_cast<GLsizeiptr>(indirectBytes), commands.data());
        }
    }

    void uploadTraversalData(
        const std::vector<TraversalNodeGpu>& nodes,
        const std::vector<uint32_t>& roots,
        const std::vector<uint32_t>& childLinks
    ) {
        if (!initialized) return;

        size_t nodeBytes = nodes.size() * sizeof(TraversalNodeGpu);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, nodeBuffer);
        if (nodeBytes > nodeCapacityBytes) {
            nodeCapacityBytes = std::max(nodeBytes, std::max<size_t>(sizeof(TraversalNodeGpu), nodeCapacityBytes * 2));
            glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(nodeCapacityBytes), nullptr, GL_DYNAMIC_DRAW);
        }
        if (nodeBytes > 0) {
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(nodeBytes), nodes.data());
        }

        size_t rootBytes = roots.size() * sizeof(uint32_t);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, rootBuffer);
        if (rootBytes > rootCapacityBytes) {
            rootCapacityBytes = std::max(rootBytes, std::max<size_t>(sizeof(uint32_t), rootCapacityBytes * 2));
            glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(rootCapacityBytes), nullptr, GL_DYNAMIC_DRAW);
        }
        if (rootBytes > 0) {
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(rootBytes), roots.data());
        }

        size_t childLinkBytes = childLinks.size() * sizeof(uint32_t);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, childLinkBuffer);
        if (childLinkBytes > childLinkCapacityBytes) {
            childLinkCapacityBytes = std::max(childLinkBytes, std::max<size_t>(sizeof(uint32_t), childLinkCapacityBytes * 2));
            glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(childLinkCapacityBytes), nullptr, GL_DYNAMIC_DRAW);
        }
        if (childLinkBytes > 0) {
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(childLinkBytes), childLinks.data());
        }

        // The traversal queue contains each resident node at most once. Two
        // buffers are alternated between levels so child expansion never
        // overwrites the queue currently being consumed.
        size_t queueBytes = std::max<size_t>(sizeof(uint32_t), nodes.size() * sizeof(uint32_t));
        if (queueBytes > traversalQueueCapacityBytes) {
            traversalQueueCapacityBytes = std::max(queueBytes, std::max<size_t>(sizeof(uint32_t), traversalQueueCapacityBytes * 2));
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, traversalQueueA);
            glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(traversalQueueCapacityBytes), nullptr, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, traversalQueueB);
            glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(traversalQueueCapacityBytes), nullptr, GL_DYNAMIC_DRAW);
        }
        if (rootBytes > 0) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, traversalQueueA);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(rootBytes), roots.data());
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, traversalCounterBuffer);
        uint32_t queueCounts[2] = {
            static_cast<uint32_t>(roots.size()),
            0u
        };
        glBufferData(
            GL_SHADER_STORAGE_BUFFER,
            static_cast<GLsizeiptr>(sizeof(queueCounts)),
            queueCounts,
            GL_DYNAMIC_DRAW
        );

        size_t metadataBytes = nodes.size() * sizeof(SectionGpuMetadata);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, metadataBuffer);
        if (metadataBytes > metadataCapacityBytes) {
            metadataCapacityBytes = std::max(metadataBytes, std::max<size_t>(sizeof(SectionGpuMetadata), metadataCapacityBytes * 2));
            glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(metadataCapacityBytes), nullptr, GL_STREAM_DRAW);
        }

        size_t indirectBytes = nodes.size() * sizeof(DrawElementsIndirectCommand);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        if (indirectBytes > indirectCapacityBytes) {
            indirectCapacityBytes = std::max(indirectBytes, std::max<size_t>(sizeof(DrawElementsIndirectCommand), indirectCapacityBytes * 2));
            glBufferData(GL_DRAW_INDIRECT_BUFFER, static_cast<GLsizeiptr>(indirectCapacityBytes), nullptr, GL_STREAM_DRAW);
        }
        // Reset the exact packed command range before the GPU traversal. The
        // compute clear remains as a second GPU-side guard, but this also
        // makes freshly orphaned storage deterministic on every driver.
        if (indirectBytes > 0) {
            std::vector<uint32_t> clearedCommandWords(nodes.size() * 5u, 0u);
            glBufferSubData(
                GL_DRAW_INDIRECT_BUFFER,
                0,
                static_cast<GLsizeiptr>(indirectBytes),
                clearedCommandWords.data()
            );
        }
    }

    void dispatchTraversal(
        GLuint computeProgram,
        const Mat4& view,
        const Mat4& viewProjection,
        float projectionY,
        int viewportWidth,
        int viewportHeight,
        float screenDiameterThreshold,
        size_t nodeCount,
        size_t rootCount
    ) const {
        if (!initialized || computeProgram == 0 || nodeCount == 0) return;
        glUseProgram(computeProgram);
        glUniformMatrix4fv(glGetUniformLocation(computeProgram, "uView"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(computeProgram, "uViewProjection"), 1, GL_FALSE, viewProjection.m);
        glUniform1f(glGetUniformLocation(computeProgram, "uProjectionY"), projectionY);
        glUniform2f(glGetUniformLocation(computeProgram, "uViewportSize"), static_cast<float>(viewportWidth), static_cast<float>(viewportHeight));
        glUniform1f(glGetUniformLocation(computeProgram, "uScreenDiameterThreshold"), screenDiameterThreshold);
        glUniform1ui(glGetUniformLocation(computeProgram, "uNodeCount"), static_cast<GLuint>(nodeCount));
        glUniform1ui(glGetUniformLocation(computeProgram, "uRootCount"), static_cast<GLuint>(rootCount));
        glUniform1ui(glGetUniformLocation(computeProgram, "uQueueCapacity"), static_cast<GLuint>(nodeCount));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, nodeBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, metadataBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indirectBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, rootBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, childLinkBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, traversalQueueA);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, traversalQueueB);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, traversalCounterBuffer);

        // First clear all indirect command slots. The bounded queue passes
        // only write slots that survive culling/LOD selection.
        glUniform1i(glGetUniformLocation(computeProgram, "uMode"), 0);
        glDispatchCompute(static_cast<GLuint>((nodeCount + 63) / 64), 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        GLuint currentQueue = traversalQueueA;
        GLuint nextQueue = traversalQueueB;
        for (GLuint level = 0; level < 5; ++level) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, currentQueue);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, nextQueue);
            glUniform1ui(glGetUniformLocation(computeProgram, "uLevel"), level);
            glUniform1i(glGetUniformLocation(computeProgram, "uMode"), 1);
            glDispatchCompute(static_cast<GLuint>((nodeCount + 63) / 64), 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            if (level < 4) {
                // Promote the child count to the next pass without a CPU
                // readback, then swap the ping-pong queues.
                glUniform1i(glGetUniformLocation(computeProgram, "uMode"), 2);
                glDispatchCompute(1, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                std::swap(currentQueue, nextQueue);
            }
        }
        glMemoryBarrier(
            GL_SHADER_STORAGE_BARRIER_BIT |
            GL_COMMAND_BARRIER_BIT |
            GL_BUFFER_UPDATE_BARRIER_BIT
        );
        glActiveTexture(GL_TEXTURE0);
    }

    void dispatchVisibility(
        GLuint computeProgram,
        const Mat4& viewProjection,
        size_t commandCount
    ) const {
        if (!initialized || computeProgram == 0 || commandCount == 0) return;

        glUseProgram(computeProgram);
        GLint viewProjectionLoc = glGetUniformLocation(computeProgram, "uViewProjection");
        GLint commandCountLoc = glGetUniformLocation(computeProgram, "uCommandCount");
        glUniformMatrix4fv(viewProjectionLoc, 1, GL_FALSE, viewProjection.m);
        glUniform1ui(commandCountLoc, static_cast<GLuint>(commandCount));

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, metadataBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, indirectBuffer);
        GLuint workgroups = static_cast<GLuint>((commandCount + 63) / 64);
        glDispatchCompute(workgroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
    }

    void drawIndirect(size_t commandCount) const {
        if (!initialized || commandCount == 0) return;
        glBindVertexArray(vao);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, metadataBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        glMultiDrawElementsIndirect(
            GL_TRIANGLES,
            GL_UNSIGNED_INT,
            nullptr,
            static_cast<GLsizei>(commandCount),
            static_cast<GLsizei>(sizeof(DrawElementsIndirectCommand))
        );
        glBindVertexArray(0);
    }

    IndirectCommandDiagnostics inspectIndirectCommands(size_t commandCount) const {
        IndirectCommandDiagnostics diagnostics;
        if (!initialized || commandCount == 0) return diagnostics;
        std::vector<DrawElementsIndirectCommand> commands(commandCount);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
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

    bool validateIndirectCommands(
        const std::vector<TraversalNodeGpu>& nodes,
        bool reportFailure
    ) const {
        if (!initialized || nodes.empty()) return true;
        std::vector<DrawElementsIndirectCommand> commands(nodes.size());
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        glGetBufferSubData(
            GL_DRAW_INDIRECT_BUFFER,
            0,
            static_cast<GLsizeiptr>(commands.size() * sizeof(DrawElementsIndirectCommand)),
            commands.data()
        );

        for (size_t i = 0; i < commands.size(); ++i) {
            const DrawElementsIndirectCommand& actual = commands[i];
            if (actual.count == 0) continue;
            const TraversalNodeGpu& expected = nodes[i];
            bool matches = actual.count == expected.draw0[0] &&
                actual.instanceCount == expected.draw0[1] &&
                actual.firstIndex == expected.draw0[2] &&
                actual.baseVertex == static_cast<int32_t>(expected.draw0[3]) &&
                actual.baseInstance == expected.draw1[0];
            if (!matches) {
                if (reportFailure) {
                    std::cerr << "GPU indirect command mismatch at slot " << i
                              << ": actual(count=" << actual.count
                              << ", instances=" << actual.instanceCount
                              << ", firstIndex=" << actual.firstIndex
                              << ", baseVertex=" << actual.baseVertex
                              << ", baseInstance=" << actual.baseInstance
                              << ") expected(count=" << expected.draw0[0]
                              << ", instances=" << expected.draw0[1]
                              << ", firstIndex=" << expected.draw0[2]
                              << ", baseVertex=" << static_cast<int32_t>(expected.draw0[3])
                              << ", baseInstance=" << expected.draw1[0] << ").\n";
                }
                return false;
            }
        }
        return true;
    }

    // One-shot startup diagnostic used to detect a driver that links the
    // traversal shader but does not produce indirect commands. Keeping this
    // out of the steady-state render loop avoids a permanent GPU readback.
    bool hasNonZeroIndirectCommand(size_t commandCount) const {
        return inspectIndirectCommands(commandCount).nonZeroCommands > 0;
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
