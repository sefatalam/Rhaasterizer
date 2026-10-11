#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rh {

class Shader {
public:
    explicit Shader(const std::string& name) {
        const std::string path = std::string(SHADER_DIR) + name + ".spv";

        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file) {
            throw std::runtime_error("Shader: cannot open " + path);
        }

        const auto size = static_cast<size_t>(file.tellg());
        if (size == 0 || size % 4 != 0) {
            throw std::runtime_error("Shader: " + path + " is not a whole number of SPIR-V words");
        }

        _code.resize(size / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(_code.data()), static_cast<std::streamsize>(size));
        if (!file) {
            throw std::runtime_error("Shader: short read on " + path);
        }
    }

    vk::ShaderModuleCreateInfo moduleInfo() const {
        return vk::ShaderModuleCreateInfo{
            .codeSize = _code.size() * sizeof(uint32_t),
            .pCode    = _code.data(),
        };
    }

private:
    std::vector<uint32_t> _code;
};
}
