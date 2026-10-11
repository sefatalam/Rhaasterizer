#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

// _pad matches the three uint32_t in the C++ Instance (scene/mesh.hpp), so
// both sides agree on an 80-byte stride.
struct Instance {
    mat4 transform;
    uint meshIndex;
    uint _pad[3];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer InstanceBuffer {
    Instance instances[];
};

// One transformMatrices record — the CPU side (scene/camera.hpp) already
// offsets the address to the current frame-in-flight slot before pushing it,
// so this is never indexed as an array.
layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer CameraBuffer {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
};

layout(push_constant) uniform PushConstants {
    VertexBuffer   vertexBuffer;
    CameraBuffer   camera;
    InstanceBuffer instances;
} pc;

layout(location = 0) out vec3 outNormal;

void main() {
    // gl_VertexIndex already includes the draw's vertexOffset and
    // gl_InstanceIndex its firstInstance -- Vulkan adds both before the
    // shader runs, so neither needs to be pushed.
    Vertex v = pc.vertexBuffer.vertices[gl_VertexIndex];
    Instance inst = pc.instances.instances[gl_InstanceIndex];

    gl_Position = pc.camera.viewProjection * inst.transform * vec4(v.position, 1.0);

    // mat3() of the transform is correct for rigid and uniform-scale nodes;
    // non-uniform scale would need the inverse-transpose.
    outNormal = mat3(inst.transform) * v.normal;
}
