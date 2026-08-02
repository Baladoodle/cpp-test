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
    void init() {
        // Vertex code for sky dome / cube
        const char* vShader = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            out vec3 vWorldPos;
            uniform mat4 uProjection;
            uniform mat4 uView;
            void main() {
                vWorldPos = aPos;
                mat4 rotView = mat4(mat3(uView)); // Remove translation
                vec4 clipPos = uProjection * rotView * vec4(aPos, 1.0);
                gl_Position = clipPos.xyww; // Force depth to 1.0 (far plane)
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

            void main() {
                vec3 dir = normalize(vWorldPos);

                // Sky horizon gradient
                float h = max(dir.y, 0.0);
                vec3 skyColor = mix(uSkyHorizonColor, uSkyTopColor, pow(h, 0.6));

                // Sun disk
                float sunDot = max(dot(dir, normalize(uSunDir)), 0.0);
                float sunDisk = pow(sunDot, 800.0) * 3.0;
                float sunGlow = pow(sunDot, 12.0) * 0.4;

                // Moon disk (opposite sun)
                float moonDot = max(dot(dir, -normalize(uSunDir)), 0.0);
                float moonDisk = pow(moonDot, 1200.0) * 1.5;

                vec3 finalSky = skyColor + uSunColor * (sunDisk + sunGlow) + vec3(0.8, 0.9, 1.0) * moonDisk;
                FragColor = vec4(finalSky, 1.0);
            }
        )";

        shader.compile(vShader, fShader);

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

    void draw(const Mat4& projection, const Mat4& view, Vec3 sunDir, Vec3 sunColor, Vec3 skyTop, Vec3 skyHorizon) {
        glDepthFunc(GL_LEQUAL);
        shader.use();
        shader.setMat4("uProjection", projection);
        shader.setMat4("uView", view);
        shader.setVec3("uSunDir", sunDir);
        shader.setVec3("uSunColor", sunColor);
        shader.setVec3("uSkyTopColor", skyTop);
        shader.setVec3("uSkyHorizonColor", skyHorizon);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);
    }
};

#endif // SKYBOX_HPP
