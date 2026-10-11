#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

// Currently using bounding spheres -> change to use bounding boxes later

namespace rh {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct MeshInfo {
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t vertexOffset;
    glm::vec3 center;
    float radius;
};

struct Instance {
    glm::mat4 transform;
    uint32_t meshIndex;
    uint32_t _pad[3];
};

static_assert(sizeof(MeshInfo) == 28, "MeshInfo layout must match the GLSL scalar-layout struct");
static_assert(sizeof(Instance) == 80, "Instance layout must match the GLSL scalar-layout struct");

struct SceneData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<MeshInfo> meshes;
    std::vector<Instance> instances;

    uint32_t appendMesh(const std::vector<Vertex>& verts, const std::vector<uint32_t>& idx) {
        MeshInfo info{};
        info.firstIndex = static_cast<uint32_t>(indices.size());
        info.indexCount = static_cast<uint32_t>(idx.size());
        info.vertexOffset = static_cast<int32_t>(vertices.size());

        glm::vec3 center { 0.0f };
        for (const auto& v : verts) center += v.position;
        center /= static_cast<float>(verts.size());

        float radius = 0.0f;
        for (const auto& v : verts) {
            radius = std::max(radius, glm::distance(center, v.position));
        }

        info.center = center;
        info.radius = radius;

        vertices.insert(vertices.end(), verts.begin(), verts.end());
        indices.insert(indices.end(), idx.begin(), idx.end());

        meshes.push_back(info);
        return static_cast<uint32_t>(meshes.size() - 1);
    }

    void addInstance(uint32_t meshIndex, const glm::mat4& transform) {
        instances.push_back(Instance {
            .transform = transform,
            .meshIndex = meshIndex,
            ._pad = {}
        });
    }
};
}