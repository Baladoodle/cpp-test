#include <epoxy/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip>
#include <sstream>
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
int activeShaderEffect = 0;

const char* shaderEffectName(int effect) {
    switch (effect) {
        case 1: return "TOON FORGE";
        case 2: return "AURORA PULSE";
        case 3: return "THERMAL SCAN";
        case 4: return "CRT DITHER";
        case 5: return "VOID INVERT";
        default: return "NORMAL";
    }
}

// Key callback
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            keys[key] = true;

            // Number keys select a shader effect. Pressing the active key again
            // returns to the normal Minecraft-style lighting path.
            if (key >= GLFW_KEY_1 && key <= GLFW_KEY_5) {
                int selectedEffect = key - GLFW_KEY_0;
                activeShaderEffect = activeShaderEffect == selectedEffect ? 0 : selectedEffect;
            }

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
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--test") {
            testMode = true;
        }
    }

    std::cout << "Starting Infinite Voxel Renderer" << (testMode ? " [TEST MODE]" : "") << "...\n";

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    int windowWidth = 1280;
    int windowHeight = 720;
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Infinite Voxel Engine (8192+ Render Distance)", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window!\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Disable VSync for unconstrained performance benchmarking
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    { // Inner scope to ensure all OpenGL objects destruct while context is alive
        // Create procedural texture atlas
        GLuint atlasTexture = TextureAtlas::createProceduralAtlas();
        std::cout << "Texture atlas created (ID: " << atlasTexture << ")\n";

        // Main Voxel Shader
        const char* vShaderSrc = R"(
        #version 330 core
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
        uniform vec3 uChunkMin; // Chunk min relative to camera
        uniform vec3 uCameraWorldPos;
        uniform float uTime;

        void main() {
            vec3 relPos = uChunkMin + aPos;
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
            vLodLevel = aLodLevel;
            vWorldPosRelative = relPos;
        }
    )";

    const char* fShaderSrc = R"(
        #version 330 core
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
        uniform int uShaderEffect;
        uniform vec2 uViewportSize;

        vec3 thermalPalette(float value) {
            value = clamp(value, 0.0, 1.0);
            if (value < 0.25) {
                return mix(vec3(0.015, 0.02, 0.18), vec3(0.0, 0.75, 1.0), value * 4.0);
            }
            if (value < 0.5) {
                return mix(vec3(0.0, 0.75, 1.0), vec3(0.1, 1.0, 0.15), (value - 0.25) * 4.0);
            }
            if (value < 0.75) {
                return mix(vec3(0.1, 1.0, 0.15), vec3(1.0, 0.85, 0.02), (value - 0.5) * 4.0);
            }
            return mix(vec3(1.0, 0.85, 0.02), vec3(0.95, 0.03, 0.015), (value - 0.75) * 4.0);
        }

        vec3 applyShaderEffect(vec3 color, vec3 viewDir) {
            if (uShaderEffect == 1) {
                // Hard light bands and a restrained grazing-angle ink edge.
                float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
                float band = floor(clamp(luminance * 1.15, 0.0, 1.0) * 4.0) / 4.0;
                vec3 toonColor = color * (0.32 + band * 0.92) / max(luminance, 0.08);
                float grazing = 1.0 - max(dot(viewDir, normalize(vNormal)), 0.0);
                float ink = smoothstep(0.64, 0.96, grazing);
                return mix(toonColor, toonColor * vec3(0.18, 0.21, 0.28), ink * 0.42);
            }

            if (uShaderEffect == 2) {
                // A world-anchored cyan/magenta pulse with a view-dependent rim.
                float phase = uTime * 2.2 + dot(vWorldPosRelative, vec3(0.075, 0.11, 0.06));
                float pulse = 0.5 + 0.5 * sin(phase);
                vec3 aurora = mix(vec3(0.05, 0.85, 1.0), vec3(1.0, 0.08, 0.62), pulse);
                float rim = pow(1.0 - max(dot(viewDir, normalize(vNormal)), 0.0), 2.0);
                float shimmer = 0.5 + 0.5 * sin(phase * 1.7 + vNormal.y * 5.0);
                return color * (0.68 + pulse * 0.24) + aurora * (rim * (0.65 + shimmer * 0.35) + pulse * 0.08);
            }

            if (uShaderEffect == 3) {
                // False-color heat map driven by the lit surface and block light.
                float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
                float heat = clamp(luminance * 0.78 + length(vBlockRGB) * 0.18 + vSkyLight * 0.12, 0.0, 1.0);
                vec3 thermal = thermalPalette(heat);
                float contour = 1.0 - smoothstep(0.02, 0.08, abs(fract(heat * 8.0) - 0.5));
                return thermal + vec3(1.0, 0.72, 0.16) * contour * 0.16;
            }

            if (uShaderEffect == 4) {
                // Green phosphor, scanlines, ordered dither, and a soft CRT vignette.
                vec2 screenUV = gl_FragCoord.xy / max(uViewportSize, vec2(1.0));
                float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
                float scanline = 0.88 + 0.12 * sin(gl_FragCoord.y * 1.15 + uTime * 9.0);
                float noise = fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233)) + floor(uTime * 12.0)) * 43758.5453);
                float dither = step(0.47, noise) * 0.045;
                float grille = step(0.94, fract(gl_FragCoord.x * 0.25)) * 0.18;
                vec3 phosphor = vec3(luminance * 0.16, luminance, luminance * 0.42) * scanline;
                phosphor += vec3(0.0, dither, dither * 0.4);
                phosphor *= 1.0 - grille;
                vec2 centered = screenUV * 2.0 - 1.0;
                float vignette = 1.0 - dot(centered, centered) * 0.14;
                return phosphor * max(vignette, 0.72);
            }

            if (uShaderEffect == 5) {
                // Solarized negative palette with a slow void-energy rim.
                float luminance = clamp(dot(color, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
                float pulse = 0.5 + 0.5 * sin(uTime * 1.45 + vDistance * 0.035);
                vec3 voidColor = mix(vec3(0.025, 0.008, 0.08), vec3(0.48, 0.025, 0.72), luminance);
                voidColor = mix(voidColor, vec3(1.0, 0.22, 0.035), smoothstep(0.62, 1.0, luminance));
                float rim = pow(1.0 - max(dot(viewDir, normalize(vNormal)), 0.0), 2.0);
                voidColor += vec3(0.12, 0.48, 1.0) * rim * (0.45 + pulse * 0.65);
                return voidColor * (0.84 + pulse * 0.18);
            }

            return color;
        }

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
            float fogStart = 0.0;
            float fogEnd = 4608.0 / 4.0;
            float fogFactor = smoothstep(fogStart, fogEnd, vDistance);

            vec3 sceneColor = mix(baseColor, uFogColor, fogFactor);
            FragColor = vec4(applyShaderEffect(sceneColor, viewDir), texColor.a);
        }
    )";

    Shader voxelShader(vShaderSrc, fShaderSrc);

    // Initialize Skybox & HUD
    Skybox skybox;
    skybox.init();

    HUD hud;
    hud.init();

    // Chunk Manager
    ChunkManager chunkMgr;

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

    while (!glfwWindowShouldClose(window)) {
        auto frameStart = std::chrono::high_resolution_clock::now();

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

        if (testMode) {
            testFrameCount++;
            camera.position.x += 150.0f * 0.016f;
            camera.position.y += 10.0f * 0.016f;
            camera.position.z += 150.0f * 0.016f;
        }
        glfwPollEvents();
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
        if (windowWidth < 1) windowWidth = 1;
        if (windowHeight < 1) windowHeight = 1;
        float aspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);

        // Update Physics & Camera
        bool superSpeed = keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_TAB];
        physics.update(camera, chunkMgr, keys, superSpeed, deltaTime);

        // Update Chunk Manager (loads/unloads & queues multithreaded LOD chunks)
        chunkMgr.update(camera.position);

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
        Frustum frustum = Frustum::extract(projection * view);

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
        voxelShader.setFloat("uTime", currentFrame);
        voxelShader.setInt("uShaderEffect", activeShaderEffect);
        voxelShader.setVec2("uViewportSize", static_cast<float>(windowWidth), static_cast<float>(windowHeight));

        glBindTexture(GL_TEXTURE_2D, atlasTexture);
        voxelShader.setInt("uTextureAtlas", 0);

        GLint uChunkMinLoc = glGetUniformLocation(voxelShader.programID, "uChunkMin");
        chunkMgr.render(frustum, camera.position, voxelShader.programID, uChunkMinLoc);

        // 3. Render HUD UI Overlay
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
            std::stringstream ss7;
            ss7 << "Shader FX: " << shaderEffectName(activeShaderEffect)
                << " | [1] Toon [2] Aurora [3] Thermal [4] CRT [5] Void";
            hud.renderText(ss1.str(), 16.0f, 16.0f, 1.8f, Vec3(1.0f, 0.9f, 0.2f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss2.str(), 16.0f, 44.0f, 1.5f, Vec3(0.3f, 1.0f, 0.4f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss3.str(), 16.0f, 68.0f, 1.5f, Vec3(0.9f, 0.9f, 0.9f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss4.str(), 16.0f, 92.0f, 1.5f, Vec3(0.4f, 0.8f, 1.0f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss5.str(), 16.0f, 116.0f, 1.4f, Vec3(0.8f, 0.8f, 0.8f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss6.str(), 16.0f, 140.0f, 1.4f, Vec3(1.0f, 0.5f, 0.2f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            hud.renderText(ss7.str(), 16.0f, 164.0f, 1.4f, Vec3(0.8f, 0.55f, 1.0f), static_cast<float>(windowWidth), static_cast<float>(windowHeight));
        }

        glfwSwapBuffers(window);

        auto frameEnd = std::chrono::high_resolution_clock::now();
        double frameTimeMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
        if (testFrameCount > 30) { // Skip first 30 warmup frames
            totalFrameTimeMs += frameTimeMs;
            benchmarkFrameCount++;
        }

        if (testMode && testFrameCount >= 600) {
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
        glDeleteTextures(1, &atlasTexture);
    } // End inner scope (all GL shaders, meshes, textures, chunk manager destructed here while context is active)
    std::cout << "Shutting down renderer...\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
