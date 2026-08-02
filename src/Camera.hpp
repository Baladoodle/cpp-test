#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "MathUtils.hpp"
#include <cmath>
#include <algorithm>

class Camera {
public:
    Vec3 position;
    Vec3 front;
    Vec3 up;
    Vec3 right;
    Vec3 worldUp;

    float yaw;
    float pitch;

    float fov;
    float mouseSensitivity;

    Camera(Vec3 startPos = Vec3(0.0f, 60.0f, 0.0f), Vec3 startUp = Vec3(0.0f, 1.0f, 0.0f), float startYaw = -90.0f, float startPitch = 0.0f)
        : position(startPos), worldUp(startUp), yaw(startYaw), pitch(startPitch), fov(70.0f), mouseSensitivity(0.1f) {
        updateCameraVectors();
    }

    Mat4 getViewMatrix() const {
        // Pure orientation matrix (view relative to camera position)
        return Mat4::lookAt(Vec3(0, 0, 0), front, up);
    }

    Mat4 getProjectionMatrix(float aspectRatio, float nearVal = 0.1f, float farVal = 20000.0f) const {
        return Mat4::perspective(fov * DEG2RAD, aspectRatio, nearVal, farVal);
    }

    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw   += xoffset;
        pitch += yoffset;

        if (constrainPitch) {
            pitch = std::clamp(pitch, -89.0f, 89.0f);
        }

        updateCameraVectors();
    }

    void updateCameraVectors() {
        Vec3 f;
        f.x = std::cos(yaw * DEG2RAD) * std::cos(pitch * DEG2RAD);
        f.y = std::sin(pitch * DEG2RAD);
        f.z = std::sin(yaw * DEG2RAD) * std::cos(pitch * DEG2RAD);
        front = f.normalized();

        right = Vec3::cross(front, worldUp).normalized();
        up    = Vec3::cross(right, front).normalized();
    }
};

#endif // CAMERA_HPP
