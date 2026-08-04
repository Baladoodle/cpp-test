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
            // Atmospheric fog is temporarily disabled; restore this flag to re-enable it.
            const bool fogEnabled = false;
            float fogFactor = fogEnabled ? smoothstep(2000.0, 4608.0, vDistance) : 0.0;
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
