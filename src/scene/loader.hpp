#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include "scene/mesh.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <stdexcept>
#include <vector>

namespace rh {

inline void loadGltf(SceneData& out, const std::filesystem::path& path) {
    auto gltfFile = fastgltf::GltfDataBuffer::FromPath(path);
    if (!gltfFile) {
        throw std::runtime_error("loadGltf: failed to read " + path.string());
    }

    fastgltf::Parser parser;
    auto asset = parser.loadGltf(gltfFile.get(), path.parent_path(),
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices);
    if (!asset) {
        throw std::runtime_error("loadGltf: failed to parse " + path.string());
    }

    std::vector<std::vector<uint32_t>> primitivesOfMesh(asset->meshes.size());

    for (size_t meshIdx = 0; meshIdx < asset->meshes.size(); ++meshIdx) {
        for (const auto& prim : asset->meshes[meshIdx].primitives) {
            auto posIt = prim.findAttribute("POSITION");
            if (posIt == prim.attributes.end() || !prim.indicesAccessor) {
                continue; //skip indexless primitive
            }

            const auto& posAccessor = asset->accessors[posIt->accessorIndex];
            std::vector<Vertex> vertices(posAccessor.count);
            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(), posAccessor,
                [&](glm::vec3 pos, size_t i) { vertices[i].position = pos; });

            if (auto normalIt = prim.findAttribute("NORMAL"); normalIt != prim.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(),
                    asset->accessors[normalIt->accessorIndex],
                    [&](glm::vec3 n, size_t i) { vertices[i].normal = n; });
            }
            if (auto uvIt = prim.findAttribute("TEXCOORD_0"); uvIt != prim.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset.get(),
                    asset->accessors[uvIt->accessorIndex],
                    [&](glm::vec2 uv, size_t i) { vertices[i].uv = uv; });
            }

            const auto& idxAccessor = asset->accessors[*prim.indicesAccessor];
            std::vector<uint32_t> indices(idxAccessor.count);
            fastgltf::copyFromAccessor<uint32_t>(asset.get(), idxAccessor, indices.data());

            primitivesOfMesh[meshIdx].push_back(out.appendMesh(vertices, indices));
        }
    }

    // fmat4x4 is column-major and contiguous like glm::mat4 -- no transpose
    const size_t sceneIndex = asset->defaultScene.value_or(0);
    if (sceneIndex >= asset->scenes.size()) {
        throw std::runtime_error("loadGltf: no scene to instantiate in " + path.string());
    }

    fastgltf::iterateSceneNodes(asset.get(), sceneIndex, fastgltf::math::fmat4x4(),
        [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& world) {
            if (!node.meshIndex) return; // joints, cameras, empties
            const glm::mat4 transform = glm::make_mat4(world.data());
            for (uint32_t prim : primitivesOfMesh[*node.meshIndex]) {
                out.addInstance(prim, transform);
            }
        });
}

}
