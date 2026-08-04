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
