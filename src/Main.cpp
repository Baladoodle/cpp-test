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
float lastX = 640.0f, lastY = 360.0f;
bool cursorLocked = true;
bool showHUD = true;

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
        layout (location = 7) in float aLodLevel;
        layout (location = 8) in float aWindWeight;

        out vec3 vNormal;
        out vec2 vTexCoord;
        out float vTexIndex;
        flat out int vCutoutTile;
        out vec2 vCutoutLocalUV;
        out float vAO;
        out vec3 vBlockRGB;
        out float vSkyLight;
        out float vDistance;
        out float vLodLevel;
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
            vTexIndex = aTexIndex;
            vCutoutTile = int(aTexIndex + 0.5);
            vec2 atlasTile = vec2(
                mod(aTexIndex, 16.0),
                floor(aTexIndex / 16.0)
            );
            vCutoutLocalUV = aTexCoord * 16.0 - atlasTile;
            vAO = aAO;
            vBlockRGB = aBlockRGB;
            vSkyLight = aSkyLight;
            vDistance = length(relPos);
            vLodLevel = section.chunkMinLod.w;
            vWorldPosRelative = relPos;
        }
    )";

    const char* fShaderSrc = R"(
        #version 430 core
        in vec3 vNormal;
        in vec2 vTexCoord;
        in float vTexIndex;
        flat in int vCutoutTile;
        in vec2 vCutoutLocalUV;
        in float vAO;
        in vec3 vBlockRGB;
        in float vSkyLight;
        in float vDistance;
        in float vLodLevel;
        in vec3 vWorldPosRelative;

        out vec4 FragColor;

        uniform sampler2D uTextureAtlas;
        uniform vec3 uSunDir;
        uniform vec3 uSunColor;
        uniform vec3 uSkyAmbientColor;
        uniform vec3 uAbyssAmbientColor;
        uniform vec3 uSkyTint;
        uniform vec3 uFogColor;
        uniform float uTime;

        void main() {
            vec4 texColor;
            bool isLeafTile = vCutoutTile == 7 || vCutoutTile == 13 ||
                vCutoutTile == 16 || vCutoutTile == 19 ||
                vCutoutTile == 22 || vCutoutTile == 23 ||
                vCutoutTile == 26 || vCutoutTile == 27 ||
                (vCutoutTile >= 38 && vCutoutTile <= 57);
            bool isGrassTile = vCutoutTile >= 29 && vCutoutTile <= 37;
            if (isLeafTile || isGrassTile) {
                int tileID = vCutoutTile;
                ivec2 tileBase = ivec2((tileID % 16) * 16, (tileID / 16) * 16);
                ivec2 localPixel = ivec2(
                    clamp(int(vCutoutLocalUV.x * 16.0), 0, 15),
                    clamp(int(vCutoutLocalUV.y * 16.0), 0, 15)
                );
                texColor = texelFetch(uTextureAtlas, tileBase + localPixel, 0);
            } else {
                texColor = texture(uTextureAtlas, vTexCoord);
            }
            if (texColor.a < 0.1) discard;

            // 1. Hemispheric Ambient Shading (Top vs Underside Glow)
            float hemiFactor = clamp(vNormal.y * 0.5 + 0.5, 0.0, 1.0);
            vec3 hemiAmbient = mix(uAbyssAmbientColor, uSkyAmbientColor, hemiFactor);

            // 2. Directional Sun Shading
            float diff = max(dot(vNormal, normalize(uSunDir)), 0.0);
            vec3 directSun = uSunColor * (diff * 0.65 + 0.35);

            // 3. Emissive RGB Block Light (Glow Crystals, Sky Quartz, Lava)
            vec3 emissiveRGB = vBlockRGB * 1.4;

            // 4. Subsurface Scattering Translucency & Rim Lighting for Foliage
            vec3 viewDir = normalize(-vWorldPosRelative);
            vec3 sunDirNorm = normalize(uSunDir);
            vec3 extraFoliageLight = vec3(0.0);
            if (isLeafTile || isGrassTile) {
                float backLighting = max(0.0, dot(-viewDir, sunDirNorm));
                float sss = pow(backLighting, 3.0) * 0.65;
                float rim = pow(1.0 - max(0.0, dot(viewDir, vNormal)), 3.5) * 0.25;
                extraFoliageLight = uSunColor * (sss + rim);
            }

            // Total Combined Surface Illumination
            vec3 totalLight = hemiAmbient + directSun * 0.8 + emissiveRGB + extraFoliageLight;

            // Soft Ease-Out Contact Ambient Occlusion
            float aoShadow = 1.0 - vAO;
            float smoothAO = 1.0 - pow(aoShadow, 1.35) * 0.72;
            vec3 baseColor = texColor.rgb * totalLight * smoothAO;

            // Atmospheric Fog blending distant islands into sky color
            float fogStart = 2000.0;
            float fogEnd = 4608.0;
            float fogFactor = smoothstep(fogStart, fogEnd, vDistance);

            FragColor = vec4(mix(baseColor, uFogColor, fogFactor), texColor.a);
        }
    )";

    const char* visibilityShaderSrc = R"(
        #version 430 core
        layout(local_size_x = 64) in;

        struct TraversalNode {
            vec4 chunkMinLod;
            vec4 sectionBounds;
            uvec4 topology;
            uvec4 draw0;
            uvec4 draw1;
        };
        layout(std430, binding = 0) readonly buffer TraversalNodeBuffer {
            TraversalNode nodes[];
        };

        struct SectionMetadata {
            vec4 chunkMinLod;
            vec4 sectionBounds;
        };
        layout(std430, binding = 1) buffer VisibleMetadataBuffer {
            SectionMetadata visibleMetadata[];
        };

        layout(std430, binding = 2) buffer VisibleCommandBuffer {
            // An indirect draw command is 20 tightly packed bytes. A GLSL
            // std430 array of structs would use a 32-byte stride because the
            // struct's base alignment is rounded up to 16 bytes, which does
            // not match GL_DRAW_INDIRECT_BUFFER. Address the five words
            // explicitly so the SSBO and indirect-draw views share the same
            // byte layout.
            uint visibleCommandWords[];
        };

        layout(std430, binding = 3) readonly buffer RootBuffer {
            uint roots[];
        };
        layout(std430, binding = 4) readonly buffer ChildLinkBuffer {
            uint childLinks[];
        };

        layout(std430, binding = 5) readonly buffer CurrentQueueBuffer {
            uint currentQueue[];
        };
        layout(std430, binding = 6) writeonly buffer NextQueueBuffer {
            uint nextQueue[];
        };
        layout(std430, binding = 7) buffer QueueCounterBuffer {
            uint currentCount;
            uint nextCount;
        } queueCounters;

        uniform mat4 uView;
        uniform mat4 uViewProjection;
        uniform float uProjectionY;
        uniform vec2 uViewportSize;
        uniform float uScreenDiameterThreshold;
        uniform uint uNodeCount;
        uniform uint uRootCount;
        uniform uint uQueueCapacity;
        uniform uint uLevel;
        uniform int uMode;

        bool outsideFrustum(vec3 minP, float size) {
            for (int plane = 0; plane < 6; ++plane) {
                bool allOutside = true;
                for (int corner = 0; corner < 8; ++corner) {
                    vec3 offset = vec3(
                        ((corner & 1) != 0) ? size : 0.0,
                        ((corner & 2) != 0) ? size : 0.0,
                        ((corner & 4) != 0) ? size : 0.0
                    );
                    vec4 clip = uViewProjection * vec4(minP + offset, 1.0);
                    if (clip.w <= 0.0) {
                        allOutside = false;
                        break;
                    }
                    bool outside = false;
                    if (plane == 0) outside = clip.x < -clip.w;
                    if (plane == 1) outside = clip.x >  clip.w;
                    if (plane == 2) outside = clip.y < -clip.w;
                    if (plane == 3) outside = clip.y >  clip.w;
                    if (plane == 4) outside = clip.z < -clip.w;
                    if (plane == 5) outside = clip.z >  clip.w;
                    allOutside = allOutside && outside;
                }
                if (allOutside) return true;
            }
            return false;
        }


        bool shouldDescend(TraversalNode node) {
            if (node.topology.y == 0u || node.topology.z == 0u) return false;
            vec3 center = node.chunkMinLod.xyz + vec3(node.sectionBounds.x * 0.5);
            float distance = max(1.0, -((uView * vec4(center, 1.0)).z));
            float radius = node.sectionBounds.x * 0.8660254;
            float diameter = radius * uProjectionY * uViewportSize.y / distance;
            return diameter > uScreenDiameterThreshold;
        }

        void emitNode(uint nodeIndex) {
            TraversalNode node = nodes[nodeIndex];
            if (node.topology.w == 0u) return;
            visibleMetadata[nodeIndex].chunkMinLod = node.chunkMinLod;
            visibleMetadata[nodeIndex].sectionBounds = node.sectionBounds;
            uint commandBase = nodeIndex * 5u;
            visibleCommandWords[commandBase + 0u] = node.draw0.x;
            visibleCommandWords[commandBase + 1u] = node.draw0.y;
            visibleCommandWords[commandBase + 2u] = node.draw0.z;
            visibleCommandWords[commandBase + 3u] = node.draw0.w;
            visibleCommandWords[commandBase + 4u] = node.draw1.x;
        }

        void main() {
            uint invocation = gl_GlobalInvocationID.x;
            if (uMode == 0) {
                if (invocation < uNodeCount) {
                    visibleCommandWords[invocation * 5u] = 0u;
                }
                return;
            }
            if (uMode == 2) {
                if (invocation == 0u) {
                    queueCounters.currentCount = min(queueCounters.nextCount, uQueueCapacity);
                    queueCounters.nextCount = 0u;
                }
                return;
            }

            uint activeCount = min(queueCounters.currentCount, uQueueCapacity);
            if (invocation >= activeCount) return;

            uint nodeIndex = currentQueue[invocation];
            if (nodeIndex >= uNodeCount) return;
            TraversalNode node = nodes[nodeIndex];
            if (outsideFrustum(node.chunkMinLod.xyz, node.sectionBounds.x)) return;

            if (uLevel < 4u && node.topology.y > 0u && shouldDescend(node)) {
                uint firstChild = node.topology.x;
                uint childCount = node.topology.y;
                uint appendBase = atomicAdd(queueCounters.nextCount, childCount);
                if (appendBase + childCount <= uQueueCapacity) {
                    for (uint child = 0u; child < childCount; ++child) {
                        nextQueue[appendBase + child] = childLinks[firstChild + child];
                    }
                    // A malformed/overfull queue must retain the coarser
                    // parent rather than producing a visible hole.
                    emitNode(nodeIndex);
                }
            } else {
                emitNode(nodeIndex);
            }
        }
    )";

    Shader voxelShader(vShaderSrc, fShaderSrc);
    Shader visibilityShader;
    visibilityShader.compileCompute(visibilityShaderSrc);
    if (visibilityShader.programID == 0) {
        std::cerr << "GPU traversal compute shader unavailable; the renderer will not use a CPU fallback.\n";
    }

    // Initialize Skybox & HUD
    Skybox skybox;
    skybox.init();

    HUD hud;
    hud.init();

    // Chunk Manager
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
    ChunkManager chunkMgr(windowWidth, windowHeight);
    chunkMgr.setDiagnosticsEnabled(diagnosticsMode);

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
            diagnosticsFile << "frame,elapsed_s,update_ms,traversal_ms,hud_ms,swap_ms,readback_ms,total_to_swap_ms,total_loop_ms,loaded_chunks,uploaded_meshes,pending_tasks,nodes,roots,candidate_indices,largest_candidate,emitted_commands,emitted_indices,largest_emitted,gpu_verified,command_valid,gl_error,gl_start,gl_update,gl_traversal,gl_hud,gl_swap,gl_readback\n";
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

        // Vertical Altitude & Abyss Atmospheric Tinting
        float camY = camera.position.y;
        float altitudeFactor = std::clamp((camY + 150.0f) / 600.0f, 0.0f, 1.0f);

        Vec3 sunColor(1.0f, 0.95f, 0.85f);
        Vec3 skyTop(0.2f, 0.5f, 0.9f);
        Vec3 skyHorizon = Vec3::lerp(Vec3(0.35f, 0.32f, 0.55f), Vec3(0.70f, 0.85f, 1.00f), altitudeFactor);
        Vec3 fogColor = skyHorizon;

        Vec3 skyAmbientColor = Vec3::lerp(Vec3(0.35f, 0.45f, 0.65f), Vec3(0.55f, 0.72f, 0.95f), altitudeFactor);
        Vec3 abyssAmbientColor = Vec3::lerp(Vec3(0.10f, 0.08f, 0.18f), Vec3(0.22f, 0.25f, 0.35f), altitudeFactor);
        // Night time adjustments
        if (sunDir.y < 0.0f) {
            float nightFactor = std::clamp(-sunDir.y * 2.0f, 0.0f, 1.0f);
            skyTop = Vec3::lerp(skyTop, Vec3(0.02f, 0.03f, 0.08f), nightFactor);
            skyHorizon = Vec3::lerp(skyHorizon, Vec3(0.05f, 0.08f, 0.15f), nightFactor);
            fogColor = skyHorizon;
            skyAmbientColor = Vec3::lerp(skyAmbientColor, Vec3(0.1f, 0.12f, 0.2f), nightFactor);
            abyssAmbientColor = Vec3::lerp(abyssAmbientColor, Vec3(0.05f, 0.06f, 0.1f), nightFactor);
            sunColor = Vec3::lerp(sunColor, Vec3(0.2f, 0.2f, 0.3f), nightFactor);
        }

        // Match the skybox to the fog color so distant geometry fades into a
        // seamless background rather than a different sky gradient.
        skyTop = fogColor;
        skyHorizon = fogColor;

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
        skybox.draw(projection, view, sunDir, sunColor, skyTop, skyHorizon);

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

        glBindTexture(GL_TEXTURE_2D, atlasTexture);
        voxelShader.setInt("uTextureAtlas", 0);

        auto traversalStart = std::chrono::high_resolution_clock::now();
        if (!chunkMgr.render(
            frustum,
            camera.position,
            camera.fov * DEG2RAD,
            windowWidth,
            windowHeight,
            voxelShader.programID,
            visibilityShader.programID,
            projection.m[5],
            view,
            viewProjection
        )) {
            traversalFailure = true;
            std::cerr << "GPU traversal verification failed; stopping instead of using CPU traversal.\n";
            break;
        }
        auto traversalEnd = std::chrono::high_resolution_clock::now();
        GLenum traversalGlError = diagnosticsMode ? drainOpenGLErrors() : GL_NO_ERROR;

        // 3. Render HUD UI Overlay
        auto hudStart = std::chrono::high_resolution_clock::now();
        if (showHUD) {
            int totalChunks = 0, uploadedMeshes = 0, pendingTasks = 0;
            chunkMgr.getStats(totalChunks, uploadedMeshes, pendingTasks);

            std::stringstream ss1, ss2, ss3, ss4, ss5, ss6;
            ss1 << "INFINITE VOXEL ENGINE (LOD 0..4 RENDER DISTANCE)";
            ss2 << "FPS: " << static_cast<int>(currentFPS) << " | Frame Time: " << std::fixed << std::setprecision(1) << (1000.0f / currentFPS) << " ms";
            ss3 << "XYZ: " << std::fixed << std::setprecision(1) << camera.position.x << " / " << camera.position.y << " / " << camera.position.z;
            ss4 << "Effective Render Distance: ~4,608 blocks (LODs 0..4)";
            ss5 << "Chunks: " << totalChunks << " loaded | Meshes: " << uploadedMeshes << " | Queued Tasks: " << pendingTasks;
            ss6 << "Mode: " << (physics.isFlying ? "FLYING (WASD + Space/Shift)" : "WALKING (Physics Collision)")
                << (superSpeed ? " [SUPER SPEED 160m/s]" : "") << " | [F] Toggle Fly | [H] HUD";
            hud.renderText(ss1.str(), 16.0f, 16.0f, 1.8f, Vec3(1.0f, 0.9f, 0.2f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss2.str(), 16.0f, 44.0f, 1.5f, Vec3(0.3f, 1.0f, 0.4f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss3.str(), 16.0f, 68.0f, 1.5f, Vec3(0.9f, 0.9f, 0.9f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss4.str(), 16.0f, 92.0f, 1.5f, Vec3(0.4f, 0.8f, 1.0f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss5.str(), 16.0f, 116.0f, 1.4f, Vec3(0.8f, 0.8f, 0.8f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss6.str(), 16.0f, 140.0f, 1.4f, Vec3(1.0f, 0.5f, 0.2f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
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
                << (frameDiagnostics.gpuTraversalVerified ? 1 : 0) << ','
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
