#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 0) out vec4 outColor;

// Normal shading only -- no descriptor sets, nothing sampled. A single
// hardcoded directional light plus a flat ambient term, enough to read the
// shape of loaded geometry.
const vec3 LIGHT_DIR = normalize(vec3(0.5, 1.0, 0.3));
const vec3 BASE_COLOR = vec3(0.8);
const float AMBIENT = 0.15;

void main() {
    const vec3 n = normalize(inNormal);
    const float diffuse = max(dot(n, LIGHT_DIR), 0.0);
    outColor = vec4(BASE_COLOR * (AMBIENT + (1.0 - AMBIENT) * diffuse), 1.0);
}
