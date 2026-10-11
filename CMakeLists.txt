cmake_minimum_required(VERSION 3.24)

project(Rhaast LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
# volk is OFF by default on purpose: vulkan.hpp's dynamic dispatcher already
# replaces the loader trampoline with directly-loaded function pointers, so
# volk is redundant here. Flip it ON if you ever move to the plain C API.
option(RHAAST_USE_VOLK     "Load Vulkan through volk instead of the linked loader" OFF)
option(RHAAST_ENABLE_IMGUI "Build with the Dear ImGui debug overlay"               ON)
option(RHAAST_ENABLE_ASSET "Build with the glTF / OBJ / mesh-optimiser loaders"    ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# System dependencies (MSYS2 UCRT64 + Vulkan SDK)
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(vk-bootstrap REQUIRED)

if(RHAAST_ENABLE_ASSET)
    find_package(fastgltf REQUIRED)
    find_package(meshoptimizer REQUIRED)
endif()

# vk-bootstrap's package config pulls in Vulkan::Headers, which on this system
# is the pacman copy (VK_HEADER_VERSION 321) while the SDK ships 341. Two
# different vulkan/vulkan.h on one include path is a real hazard: vulkan_raii.hpp
# must see the vulkan_core.h it was generated against. Everything is pinned to
# the SDK below, and verified at configure time.
set(RHAAST_VULKAN_INCLUDE_DIR "${Vulkan_INCLUDE_DIR}")

file(STRINGS "${RHAAST_VULKAN_INCLUDE_DIR}/vulkan/vulkan_core.h"
     _hdr_ver REGEX "^#define VK_HEADER_VERSION [0-9]+")
string(REGEX MATCH "[0-9]+$" RHAAST_VK_HEADER_VERSION "${_hdr_ver}")
message(STATUS "Rhaast: Vulkan headers 1.4.${RHAAST_VK_HEADER_VERSION} from ${RHAAST_VULKAN_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
# volk (optional)
# ---------------------------------------------------------------------------
# The SDK ships volk.lib, but that is an MSVC-format archive and unusable from
# MinGW. volk.c is compiled into the build instead, which is the upstream
# recommendation anyway. Declared before ImGui so the backend can link it.
if(RHAAST_USE_VOLK)
    if(NOT EXISTS "${RHAAST_VULKAN_INCLUDE_DIR}/Volk/volk.c")
        message(FATAL_ERROR "RHAAST_USE_VOLK is ON but Volk/volk.c was not found in the SDK")
    endif()
    add_library(volk STATIC "${RHAAST_VULKAN_INCLUDE_DIR}/Volk/volk.c")
    target_include_directories(volk SYSTEM PUBLIC
        "${RHAAST_VULKAN_INCLUDE_DIR}" "${RHAAST_VULKAN_INCLUDE_DIR}/Volk")
    target_compile_definitions(volk PUBLIC VK_NO_PROTOTYPES VK_USE_PLATFORM_WIN32_KHR)
endif()

# ---------------------------------------------------------------------------
# Fetched dependencies
# ---------------------------------------------------------------------------
# Dear ImGui is not in the UCRT64 repos, so it comes from git, fetched as
# plain source (SOURCE_SUBDIR points at a directory with no CMakeLists.txt,
# which makes FetchContent skip add_subdirectory) and compiled into a target
# defined here. That keeps us clear of CMake 4.x's refusal to configure
# projects declaring cmake_minimum_required < 3.5 -- imgui's own is one of
# them. KTX2 below is fetched too (also not in the UCRT64 repos) but takes
# its own usable CMakeLists normally; see the comment at its FetchContent_Declare.
include(FetchContent)

if(RHAAST_ENABLE_IMGUI)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.92.9b-docking
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  no-cmake-here)
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
    target_include_directories(imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    # BEFORE so the SDK headers win over whatever glfw/vulkan-headers inject.
    target_include_directories(imgui SYSTEM BEFORE PRIVATE ${RHAAST_VULKAN_INCLUDE_DIR})
    target_link_libraries(imgui PUBLIC glfw)
    if(RHAAST_USE_VOLK)
        # The define alone is not enough -- the backend does #include <volk.h>,
        # so it needs volk's include dirs and VK_NO_PROTOTYPES as well.
        target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)
        target_link_libraries(imgui PUBLIC volk)
    endif()
endif()

if(RHAAST_ENABLE_ASSET)
    # No MSYS2 UCRT64 package for KTX2 (checked ucrt64.db directly -- fastgltf
    # and meshoptimizer above are packaged, this isn't). KTX-Software's own
    # CMakeLists takes a recent cmake_minimum_required, so unlike imgui and
    # the old tinyobjloader fetch above, this doesn't need the SOURCE_SUBDIR
    # no-cmake-here trick -- FetchContent just configures the subproject
    # normally. Submodules (basisu transcoder, dfdutils, ...) are left to the
    # default --recurse-submodules clone, unlike vma_hpp which opts out.
    FetchContent_Declare(ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG        v4.3.2)

    # Trim the build to just the library: no CLI tools, no tests, no docs,
    # no OpenGL/Vulkan upload helpers -- image upload goes through
    # rh::loadKTX2 (scene/texture.hpp) and the existing hostImageCopy path,
    # not libktx's own loader. Static, to keep this a plain link like
    # everything else here instead of a DLL to ship next to Rhaast.exe.
    set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_TOOLS ON CACHE BOOL "" FORCE) # TEMP: flipped on to build toktx for PNG->KTX2 conversion, flip back to OFF after
    set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_VULKAN OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ktx)
    # If configure complains about an unrecognised KTX_FEATURE_* name, this
    # checkout's option set has moved on since v4.3.2 -- `cmake -B
    # build/debug -LH | grep KTX_FEATURE` after a first attempt lists the
    # real set for whatever tag ends up pinned above.
endif()

# Pre-generated C++/RAII bindings over VMA: vma::raii::Buffer/Image combine a
# VkBuffer/VkImage and its VmaAllocation into one movable RAII object, instead
# of the two handles vmaCreateBuffer hands back separately. SOURCE_SUBDIR
# points straight at include/, skipping the repo's own top-level CMakeLists
# (its generator + submodule machinery) -- the .hpp files under include/ are
# already generated and checked in, so none of that is needed.
FetchContent_Declare(vma_hpp
    GIT_REPOSITORY https://github.com/YaaZ/VulkanMemoryAllocator-Hpp.git
    GIT_TAG        v3.4.0+2
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES ""
    SOURCE_SUBDIR  include)
FetchContent_MakeAvailable(vma_hpp)

# VMA-Hpp's generated bindings call VMA C functions (vmaCreateDedicatedBuffer,
# vmaAllocateDedicatedMemory, ...) added upstream after the vk_mem_alloc.h the
# Vulkan SDK bundles under include/vma/ -- that older copy won't compile
# against these bindings. Fetch the exact upstream commit v3.4.0+2 was
# generated against instead (pinned via VMA-Hpp's own .gitmodules).
# SOURCE_SUBDIR points at a directory with no CMakeLists.txt so only the
# sources are downloaded; the repo's own build is never configured.
FetchContent_Declare(vma_c
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        3aa921224c154a0d2c43912bc88e1c42ce1f7607
    SOURCE_SUBDIR  no-cmake-here)
FetchContent_MakeAvailable(vma_c)

# ---------------------------------------------------------------------------
# Rhaast
# ---------------------------------------------------------------------------
add_executable(Rhaast
    src/main.cpp
    src/impl.cpp

    # Headers are listed only so they appear in IDE project trees. CMake does
    # not compile them; they are pulled in by the two sources above.
    src/app.hpp
    src/core/context.hpp
    src/core/window.hpp
    src/core/swapchain.hpp
    src/core/frame.hpp
    src/core/buffer.hpp
    src/core/image.hpp
    src/scene/camera.hpp
    src/scene/mesh.hpp
    src/scene/loader.hpp
    src/scene/texture.hpp
    src/gfx/shader.hpp
    src/gfx/pipeline.hpp)

target_include_directories(Rhaast PRIVATE ${CMAKE_SOURCE_DIR}/src)

# SYSTEM BEFORE puts the SDK ahead of the pacman vulkan-headers that
# vk-bootstrap drags in. See the note next to RHAAST_VULKAN_INCLUDE_DIR.
target_include_directories(Rhaast SYSTEM BEFORE PRIVATE ${RHAAST_VULKAN_INCLUDE_DIR})

# VMA-Hpp's headers do a bare `#include "vk_mem_alloc.h"`, which must resolve
# to the fetched vma_c copy (see above), not the SDK's older bundled one --
# BEFORE puts it first so it wins if both are ever on the path.
target_include_directories(Rhaast SYSTEM BEFORE PRIVATE ${vma_c_SOURCE_DIR}/include)

# vma_hpp's own CMakeLists adds its include dir as a plain INTERFACE, not
# SYSTEM, so -Wall -Wextra -Wshadow above would otherwise apply to its headers
# same as our own code. Re-add the same path as SYSTEM for our target so the
# compiler suppresses warnings from a header we don't own.
target_include_directories(Rhaast SYSTEM BEFORE PRIVATE ${vma_hpp_SOURCE_DIR}/include)

target_compile_definitions(Rhaast PRIVATE
    # RAII objects carry their own dispatchers, but plain vk:: calls resolve
    # through VULKAN_HPP_DEFAULT_DISPATCHER. Turning this on makes the two
    # styles equally fast; src/impl.cpp holds the required storage.
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1

    # vk_mem_alloc.h is a single header pulled into more than one translation
    # unit, so these must be identical everywhere or the two copies disagree
    # about the same declarations (an ODR violation the linker won't report).
    # VMA loads every entry point itself from the vkGetInstanceProcAddr /
    # vkGetDeviceProcAddr handed to it in Context, rather than assuming linked
    # prototypes. VMA_IMPLEMENTATION stays local to src/impl.cpp.
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1

    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
    SHADER_DIR="${CMAKE_BINARY_DIR}/shaders/"
    $<$<CONFIG:Debug>:RHAAST_VALIDATION=1>)

target_link_libraries(Rhaast PRIVATE
    glfw
    glm::glm
    vk-bootstrap::vk-bootstrap
    VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp)

if(RHAAST_USE_VOLK)
    target_link_libraries(Rhaast PRIVATE volk)
    target_compile_definitions(Rhaast PRIVATE RHAAST_HAS_VOLK=1)
else()
    # Vulkan::Vulkan links the loader import library and provides prototypes,
    # which vk-bootstrap and VMA both use to bootstrap themselves.
    target_link_libraries(Rhaast PRIVATE Vulkan::Vulkan)
endif()

if(RHAAST_ENABLE_IMGUI)
    target_link_libraries(Rhaast PRIVATE imgui)
    target_compile_definitions(Rhaast PRIVATE RHAAST_HAS_IMGUI=1)
endif()

if(RHAAST_ENABLE_ASSET)
    target_link_libraries(Rhaast PRIVATE
        fastgltf::fastgltf meshoptimizer::meshoptimizer ktx)
    target_compile_definitions(Rhaast PRIVATE
        RHAAST_HAS_ASSET=1
        ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/")
endif()

target_compile_options(Rhaast PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wno-missing-field-initializers
    $<$<CONFIG:Debug>:-g3>)

# Editing app.hpp (or anything it pulls in) re-parses these on every rebuild
# otherwise -- this is effectively a unity build, see the README's
# "Header-only, on purpose" section. fastgltf/ktx.h joined vulkan_raii.hpp as
# the expensive ones once scene/loader.hpp and scene/texture.hpp landed.
set(RHAAST_PCH_HEADERS
    <vulkan/vulkan_raii.hpp>
    <vk_mem_alloc_raii.hpp>
    <GLFW/glfw3.h>
    <glm/glm.hpp>)
if(RHAAST_ENABLE_ASSET)
    list(APPEND RHAAST_PCH_HEADERS <fastgltf/core.hpp> <fastgltf/tools.hpp> <ktx.h>)
endif()
target_precompile_headers(Rhaast PRIVATE ${RHAAST_PCH_HEADERS})

# The single-header libraries are not warning-clean and never will be; their
# implementation unit opts out rather than drowning your own diagnostics.
# It also skips the PCH above: its #defines (VMA_IMPLEMENTATION, ...) have to
# land before its includes, and -w doesn't match the flags the PCH was built
# with, so reusing it here would be wrong either way.
set_source_files_properties(src/impl.cpp PROPERTIES
    COMPILE_OPTIONS "-w"
    SKIP_PRECOMPILE_HEADERS ON)

set_target_properties(Rhaast PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ---------------------------------------------------------------------------
# Shaders
# ---------------------------------------------------------------------------
# `ninja Shaders` rebuilds SPIR-V without relinking the executable. glslc -MD
# emits a depfile so #include'd .glsl fragments trigger rebuilds too; new shader
# *files* still need a configure re-run, since the glob runs at configure time.
find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin" REQUIRED)

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/shaders/*.vert
    ${CMAKE_SOURCE_DIR}/shaders/*.frag
    ${CMAKE_SOURCE_DIR}/shaders/*.comp
    ${CMAKE_SOURCE_DIR}/shaders/*.geom
    ${CMAKE_SOURCE_DIR}/shaders/*.tesc
    ${CMAKE_SOURCE_DIR}/shaders/*.tese)

set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(SPIRV_BINARIES "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPIRV "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC}
                --target-env=vulkan1.2
                $<IF:$<CONFIG:Debug>,-g,-O>
                -I${CMAKE_SOURCE_DIR}/shaders
                -MD -MF ${SPIRV}.d
                ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
        DEPFILE ${SPIRV}.d
        COMMENT "glslc ${SHADER_NAME}"
        VERBATIM)
    list(APPEND SPIRV_BINARIES ${SPIRV})
endforeach()

if(SPIRV_BINARIES)
    add_custom_target(Shaders DEPENDS ${SPIRV_BINARIES})
    add_dependencies(Rhaast Shaders)
else()
    add_custom_target(Shaders COMMENT "No shaders in shaders/ yet")
endif()
