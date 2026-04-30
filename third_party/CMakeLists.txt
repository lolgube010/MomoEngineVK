# ---------------------------------------------------------------------------
# Vulkan SDK components needed inside third-party targets
# (Vulkan was already found in the root CMakeLists.txt; this call is a no-op
#  but documents which components the targets below depend on.)
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED COMPONENTS volk dxc)

# ---------------------------------------------------------------------------
# VMA (Vulkan Memory Allocator) — header-only, lives in the Vulkan SDK
# ---------------------------------------------------------------------------
find_path(VMA_INCLUDE_DIR NAMES vk_mem_alloc.h
    HINTS "$ENV{VULKAN_SDK}/Include"
)
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE "${VMA_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
# GLM — header-only, also ships with the Vulkan SDK
# ---------------------------------------------------------------------------
find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp
    HINTS "$ENV{VULKAN_SDK}/Include"
)
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE "${GLM_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
# SDL3 — pre-built, ships with the Vulkan SDK
# ---------------------------------------------------------------------------
find_path(SDL3_INCLUDE_DIR NAMES SDL3/SDL.h
    HINTS "$ENV{VULKAN_SDK}/Include"
)
find_library(SDL3_LIB NAMES SDL3
    HINTS "$ENV{VULKAN_SDK}/Lib" "$ENV{VULKAN_SDK}/lib"
)

add_library(SDL3::SDL3 SHARED IMPORTED GLOBAL)
target_include_directories(SDL3::SDL3 INTERFACE "${SDL3_INCLUDE_DIR}")

if(WIN32)
    set_target_properties(SDL3::SDL3 PROPERTIES IMPORTED_IMPLIB "${SDL3_LIB}")
    find_file(SDL3_DLL NAMES SDL3.dll HINTS "$ENV{VULKAN_SDK}/Bin")
    set_target_properties(SDL3::SDL3 PROPERTIES IMPORTED_LOCATION "${SDL3_DLL}")
else()
    set_target_properties(SDL3::SDL3 PROPERTIES IMPORTED_LOCATION "${SDL3_LIB}")
endif()

# ---------------------------------------------------------------------------
# vkbootstrap — statically compiled
# ---------------------------------------------------------------------------
add_library(vkbootstrap STATIC
    vkbootstrap/src/VkBootstrap.h
    vkbootstrap/src/VkBootstrap.cpp
)
target_include_directories(vkbootstrap PUBLIC vkbootstrap/src)
target_compile_features(vkbootstrap PRIVATE cxx_std_20)
target_link_libraries(vkbootstrap
    PUBLIC Vulkan::volk
           $<$<BOOL:${UNIX}>:${CMAKE_DL_LIBS}>
)

# ---------------------------------------------------------------------------
# ImGui — statically compiled with Vulkan + SDL3 backends
# ---------------------------------------------------------------------------
add_library(imgui STATIC
    imgui/imgui.h
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/backends/imgui_impl_vulkan.cpp
    imgui/backends/imgui_impl_sdl3.cpp
    imgui/misc/cpp/imgui_stdlib.h
    imgui/misc/cpp/imgui_stdlib.cpp
)
target_include_directories(imgui
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}"   # consumers: <imgui/imgui.h>, <imgui/backends/...>, etc.
    PUBLIC  imgui                            # backend/misc public headers contain #include "imgui.h"
)
target_link_libraries(imgui PUBLIC Vulkan::volk SDL3::SDL3)
target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_VULKAN_USE_VOLK
    "IMGUI_IMPL_VULKAN_VOLK_FILENAME=<Volk/volk.h>"
)

# ---------------------------------------------------------------------------
# stb_image — header-only
# ---------------------------------------------------------------------------
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE stb_image)

# ---------------------------------------------------------------------------
# RenderDoc in-app API — header-only
# ---------------------------------------------------------------------------
add_library(renderdoc_app INTERFACE)
target_include_directories(renderdoc_app INTERFACE renderdoc_app)

# ---------------------------------------------------------------------------
# fastgltf and fmtlib — have their own CMakeLists.txt
# ---------------------------------------------------------------------------
add_subdirectory(fastgltf)
add_subdirectory(fmt EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Tracy profiler client
# TRACY_ENABLE and TRACY_ON_DEMAND are declared in the root CMakeLists.txt
# so they are visible here as CMake cache variables.
# ---------------------------------------------------------------------------
if(NOT TARGET Tracy::TracyClient)
    add_subdirectory(tracy)
endif()

# ---------------------------------------------------------------------------
# Tracy standalone profiler UI (optional manual build target)
# ---------------------------------------------------------------------------
set(TRACY_PROFILER_SRC_DIR   "${PROJECT_SOURCE_DIR}/third_party/tracy/profiler")
set(TRACY_PROFILER_BUILD_DIR "${TRACY_PROFILER_SRC_DIR}/build/Release")

add_custom_target(TracyProfiler
    COMMAND ${CMAKE_COMMAND}
            -B "${TRACY_PROFILER_BUILD_DIR}"
            -S "${TRACY_PROFILER_SRC_DIR}"
            -G "${CMAKE_GENERATOR}"
            -A "${CMAKE_GENERATOR_PLATFORM}"
    COMMAND ${CMAKE_COMMAND}
            --build "${TRACY_PROFILER_BUILD_DIR}"
            --config Release
    WORKING_DIRECTORY "${TRACY_PROFILER_SRC_DIR}"
    COMMENT "Configuring and building the Tracy Profiler standalone..."
    VERBATIM
)
set_target_properties(TracyProfiler PROPERTIES FOLDER "Tools")

# ---------------------------------------------------------------------------
# DXC DLL path — resolved here so src/CMakeLists.txt can copy it at
# post-build time. find_package(Vulkan) does not expose the DLL as a target.
# ---------------------------------------------------------------------------
if(WIN32)
    find_file(DXC_DLL NAMES dxcompiler.dll HINTS "$ENV{VULKAN_SDK}/Bin")
endif()

# ---------------------------------------------------------------------------
# Doxygen + include-graph generation (requires Doxygen + Graphviz)
# ---------------------------------------------------------------------------
find_package(Doxygen)

if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found — setting up include-graph docs target.")

    set(DOXYGEN_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/doxygen")
    set(DOXYGEN_EXTRACT_ALL      YES)
    set(DOXYGEN_HAVE_DOT         YES)
    set(DOXYGEN_INCLUDE_GRAPH    YES)
    set(DOXYGEN_INCLUDED_BY_GRAPH YES)
    set(DOXYGEN_RECURSIVE        YES)
    set(DOXYGEN_EXCLUDE          "${PROJECT_SOURCE_DIR}/third_party")

    doxygen_add_docs(GenerateIncludeGraphs
        "${PROJECT_SOURCE_DIR}/src"
        COMMENT "Generating include-graph documentation"
    )
    set_target_properties(GenerateIncludeGraphs PROPERTIES FOLDER "Tools")
else()
    message(STATUS "Doxygen not found — 'GenerateIncludeGraphs' target will not be available.")
endif()
