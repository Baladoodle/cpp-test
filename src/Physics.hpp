#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include "MathUtils.hpp"
#include "Camera.hpp"
#include "ChunkManager.hpp"
#include <cmath>
#include <algorithm>

struct PlayerAABB {
    Vec3 minP;
    Vec3 maxP;

    static PlayerAABB getAt(Vec3 pos) {
        PlayerAABB b;
        b.minP = Vec3(pos.x - 0.3f, pos.y - 1.62f, pos.z - 0.3f);
        b.maxP = Vec3(pos.x + 0.3f, pos.y + 0.18f, pos.z + 0.3f);
        return b;
    }

    bool intersects(const Vec3& bMin, const Vec3& bMax) const {
        return (minP.x < bMax.x && maxP.x > bMin.x) &&
               (minP.y < bMax.y && maxP.y > bMin.y) &&
               (minP.z < bMax.z && maxP.z > bMin.z);
    }
};

class PhysicsController {
public:
    Vec3 velocity;
    bool isFlying = true; // Default to flight mode so player can explore infinite islands immediately!
    bool isGrounded = false;
    float walkSpeed = 7.0f;
    float flySpeed = 30.0f;
    float superFlySpeed = 160.0f;

    PhysicsController() : velocity(0, 0, 0) {}

    void update(Camera& camera, ChunkManager& chunkMgr, bool keys[1024], bool superSpeed, float dt) {
        if (dt > 0.1f) dt = 0.1f; // Cap max delta time

        Vec3 inputDir(0, 0, 0);
        Vec3 forward = Vec3(camera.front.x, 0.0f, camera.front.z).normalized();
        Vec3 right = camera.right;

        if (keys['W']) inputDir += forward;
        if (keys['S']) inputDir -= forward;
        if (keys['A']) inputDir -= right;
        if (keys['D']) inputDir += right;

        if (inputDir.lengthSq() > 0.001f) {
            inputDir = inputDir.normalized();
        }

        if (isFlying) {
            Vec3 flyDir = inputDir;
            if (keys[' ']) flyDir.y += 1.0f;
            if (keys['C'] || keys[GLFW_KEY_LEFT_SHIFT]) flyDir.y -= 1.0f;

            float curSpeed = superSpeed ? superFlySpeed : flySpeed;
            camera.position += flyDir * curSpeed * dt;
            velocity = Vec3(0, 0, 0);
            isGrounded = false;
            return;
        }

        // Walking / Gravity physics mode
        float curSpeed = superSpeed ? (walkSpeed * 1.8f) : walkSpeed;
        velocity.x = inputDir.x * curSpeed;
        velocity.z = inputDir.z * curSpeed;

        // Apply gravity
        velocity.y -= 26.0f * dt;

        // Jump
        if (isGrounded && keys[' ']) {
            velocity.y = 9.5f;
            isGrounded = false;
        }

        // Axis-by-axis collision resolution
        // 1. Move X
        camera.position.x += velocity.x * dt;
        resolveCollisions(camera.position, chunkMgr, 0);

        // 2. Move Y
        camera.position.y += velocity.y * dt;
        isGrounded = false;
        resolveCollisions(camera.position, chunkMgr, 1);

        // 3. Move Z
        camera.position.z += velocity.z * dt;
        resolveCollisions(camera.position, chunkMgr, 2);
    }

private:
    void resolveCollisions(Vec3& pos, ChunkManager& chunkMgr, int axis) {
        PlayerAABB playerBox = PlayerAABB::getAt(pos);

        int minX = static_cast<int>(std::floor(playerBox.minP.x));
        int maxX = static_cast<int>(std::floor(playerBox.maxP.x));
        int minY = static_cast<int>(std::floor(playerBox.minP.y));
        int maxY = static_cast<int>(std::floor(playerBox.maxP.y));
        int minZ = static_cast<int>(std::floor(playerBox.minP.z));
        int maxZ = static_cast<int>(std::floor(playerBox.maxP.z));

        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    if (chunkMgr.isBlockSolidAt(x, y, z)) {
                        Vec3 blockMin(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                        Vec3 blockMax = blockMin + Vec3(1.0f);

                        if (playerBox.intersects(blockMin, blockMax)) {
                            if (axis == 0) { // X axis
                                if (velocity.x > 0) pos.x = blockMin.x - 0.301f;
                                else if (velocity.x < 0) pos.x = blockMax.x + 0.301f;
                                velocity.x = 0;
                            } else if (axis == 1) { // Y axis
                                if (velocity.y > 0) {
                                    pos.y = blockMin.y - 0.181f;
                                    velocity.y = 0;
                                } else if (velocity.y < 0) {
                                    pos.y = blockMax.y + 1.621f;
                                    velocity.y = 0;
                                    isGrounded = true;
                                }
                            } else if (axis == 2) { // Z axis
                                if (velocity.z > 0) pos.z = blockMin.z - 0.301f;
                                else if (velocity.z < 0) pos.z = blockMax.z + 0.301f;
                                velocity.z = 0;
                            }
                            playerBox = PlayerAABB::getAt(pos);
                        }
                    }
                }
            }
        }
    }
};

#endif // PHYSICS_HPP
