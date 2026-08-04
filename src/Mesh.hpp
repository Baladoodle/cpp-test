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
