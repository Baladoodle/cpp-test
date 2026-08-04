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
