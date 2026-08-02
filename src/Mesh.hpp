#ifndef MESH_HPP
#define MESH_HPP

#include <vector>
#include <cstdint>
#include <epoxy/gl.h>
#include <GLFW/glfw3.h>
#include "MathUtils.hpp"

struct VoxelVertex {
    float x, y, z;        // Position (relative to camera or chunk origin)
    float nx, ny, nz;     // Normal
    float u, v;           // UV Texture Coords
    float texIndex;       // Texture Atlas ID
    float ao;             // Ambient Occlusion (0.2 - 1.0)
    float blockLight;     // Emissive Block Light (0.0 - 1.0)
    float lodLevel;       // LOD level for shader fog/fade
};

class Mesh {
public:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    uint32_t indexCount = 0;
    bool uploaded = false;

    Mesh() = default;
    ~Mesh() {
        cleanUp();
    }

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept {
        vao = other.vao;
        vbo = other.vbo;
        ebo = other.ebo;
        indexCount = other.indexCount;
        uploaded = other.uploaded;

        other.vao = 0;
        other.vbo = 0;
        other.ebo = 0;
        other.indexCount = 0;
        other.uploaded = false;
    }

    Mesh& operator=(Mesh&& other) noexcept {
        if (this != &other) {
            cleanUp();
            vao = other.vao;
            vbo = other.vbo;
            ebo = other.ebo;
            indexCount = other.indexCount;
            uploaded = other.uploaded;

            other.vao = 0;
            other.vbo = 0;
            other.ebo = 0;
            other.indexCount = 0;
            other.uploaded = false;
        }
        return *this;
    }

    void cleanUp() {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = vbo = ebo = 0;
        indexCount = 0;
        uploaded = false;
    }

    void upload(const std::vector<VoxelVertex>& vertices, const std::vector<uint32_t>& indices) {
        cleanUp();
        if (vertices.empty() || indices.empty()) return;

        indexCount = static_cast<uint32_t>(indices.size());

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VoxelVertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

        // Attribute 0: Position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, x));

        // Attribute 1: Normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, nx));

        // Attribute 2: UV Coords
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, u));

        // Attribute 3: Texture Index
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, texIndex));

        // Attribute 4: AO
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, ao));

        // Attribute 5: Block Light
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, blockLight));

        // Attribute 6: LOD Level
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void*)offsetof(VoxelVertex, lodLevel));

        glBindVertexArray(0);
        uploaded = true;
    }

    void draw() const {
        if (uploaded && indexCount > 0) {
            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        }
    }
};

#endif // MESH_HPP
