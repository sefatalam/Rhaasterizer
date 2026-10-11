// Single implementation unit for every single-header library in the project.
//
// These headers put their actual code behind an *_IMPLEMENTATION define, which
// must be expanded in exactly one translation unit. Keeping them together here
// means they compile once, they don't slow down incremental builds of your own
// code, and CMake can turn warnings off for the whole file (see CMakeLists.txt)
// without hiding warnings in code you wrote.
//
// Nothing here should need editing day to day.

#include <vulkan/vulkan.hpp>

// Storage for VULKAN_HPP_DEFAULT_DISPATCHER. Required in exactly one
// translation unit whenever VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1.
// vk::raii::* objects do NOT use this dispatcher -- they carry their own --
// but every plain vk:: call does, which is what makes dropping out of RAII
// into the non-RAII C++ bindings work at full speed. main.cpp has the
// matching init() calls.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

// VMA_STATIC_VULKAN_FUNCTIONS / VMA_DYNAMIC_VULKAN_FUNCTIONS live in
// CMakeLists.txt, because context.hpp includes this header too and both
// translation units have to agree. VMA_IMPLEMENTATION belongs here and
// only here -- it is the one that must expand exactly once.
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.hpp>
#include <vk_mem_alloc_raii.hpp>

// KTX2 (libktx) and glTF (fastgltf) are both real compiled libraries, not
// single-header libs hiding behind an *_IMPLEMENTATION define, so neither
// needs anything here -- they link normally, same as vk-bootstrap and GLFW.
// stb_image and tinyobjloader used to have their implementation blocks in
// this spot; both are gone now that scene/texture.hpp and scene/loader.hpp
// cover image and mesh loading.
