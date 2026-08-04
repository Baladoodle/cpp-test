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
    vec3 directionalHorizon = mix(
        uSkySurfaceHorizonColor,
        uSkyFloatingHorizonColor,
        layerBlend
    );
    vec3 horizonColor = mix(
        directionalHorizon,
        uSkyHorizonColor,
        0.35
    );
    float h = max(dir.y, 0.0);
    vec3 skyColor = mix(horizonColor, uSkyTopColor, pow(h, 0.6));

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
