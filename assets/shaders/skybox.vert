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
