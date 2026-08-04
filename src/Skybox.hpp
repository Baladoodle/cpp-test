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
