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
