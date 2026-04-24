# ---------------------------------------------------------------------------
# Source files — CONFIGURE_DEPENDS re-checks the folder for new files
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
)

add_executable(MomoVK ${SOURCE_FILES})

# ---------------------------------------------------------------------------
# Language standard
# ---------------------------------------------------------------------------
target_compile_features(MomoVK PRIVATE cxx_std_20)
set_target_properties(MomoVK PROPERTIES CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
target_include_directories(MomoVK PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
target_compile_definitions(MomoVK
    PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        VK_NO_PROTOTYPES          # Required: routes all Vulkan calls through volk
    PRIVATE
        # Tracy: 1 when enabled, 0 when disabled — keeps the macro defined either way
        TRACY_ENABLE=$<IF:$<BOOL:${TRACY_ENABLE}>,1,0>
)

# Debug-names feature flag (Debug config only, opt-in via CMake option)
if(MOMOVK_ENABLE_DEBUG_NAMES)
    target_compile_definitions(MomoVK PRIVATE
        $<$<CONFIG:Debug>:MOMOVK_ENABLE_DEBUG_NAMES>
    )
endif()

# RenderDoc in-app API (all configs — you may want to capture release builds too)
if(MOMOVK_ENABLE_RENDERDOC)
    target_compile_definitions(MomoVK PRIVATE MOMOVK_ENABLE_RENDERDOC)
endif()

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
target_link_libraries(MomoVK
    PUBLIC
        Vulkan::volk
        Vulkan::dxc_lib
        vma
        glm
        fmt::fmt
        stb_image
        SDL2::SDL2
        vkbootstrap
        imgui
        fastgltf::fastgltf
    PRIVATE
        Tracy::TracyClient
)

if(MOMOVK_ENABLE_RENDERDOC)
    target_link_libraries(MomoVK PRIVATE renderdoc_app)
endif()

# ---------------------------------------------------------------------------
# Precompiled headers
# ---------------------------------------------------------------------------
target_precompile_headers(MomoVK PUBLIC
    <optional>
    <vector>
    <memory>
    <string>
    <unordered_map>
    <filesystem>
    <chrono>
    <format>
	<vector>
	<functional>
	<deque>
    <glm/mat4x4.hpp>
    <glm/vec4.hpp>
    <Volk/volk.h>
    <vulkan/vk_enum_string_helper.h>
    <fmt/core.h>
    <fastgltf/types.hpp>
    <vma/vk_mem_alloc.h>
    <ranges>
)

# ---------------------------------------------------------------------------
# Linker flags
# ---------------------------------------------------------------------------
if(MSVC)
    # Suppress "missing PDB" warning from volk's static lib (volkd.pdb)
    target_link_options(MomoVK PRIVATE /IGNORE:4099)
endif()

# ---------------------------------------------------------------------------
# Post-build: copy runtime DLLs next to the executable (Windows only)
# ---------------------------------------------------------------------------
if(WIN32)
    add_custom_command(TARGET MomoVK POST_BUILD
        # Copies all DLLs that CMake knows about (e.g. imported shared targets)
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:MomoVK>
            $<TARGET_FILE_DIR:MomoVK>
        # DXC is not an imported shared target, so copy it manually
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DXC_DLL}"
            $<TARGET_FILE_DIR:MomoVK>
        # SDL2 IMPORTED_LOCATION points to the DLL on Windows
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_PROPERTY:SDL2::SDL2,IMPORTED_LOCATION>
            $<TARGET_FILE_DIR:MomoVK>
        COMMAND_EXPAND_LISTS
    )
endif()
