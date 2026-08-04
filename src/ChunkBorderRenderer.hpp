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
