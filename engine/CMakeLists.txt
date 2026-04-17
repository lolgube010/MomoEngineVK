cmake_minimum_required(VERSION 3.25)

project(MomoVK
    DESCRIPTION "MomoEngineVK - a modern Vulkan graphics engine"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# CMake options (declared at root so they appear in CMake-GUI / cmake -L)
# ---------------------------------------------------------------------------
option(MOMOVK_ENABLE_DEBUG_NAMES "Enable Vulkan object debug naming in Debug builds" OFF)
option(TRACY_ENABLE              "Enable Tracy profiling client"                      ON)
option(TRACY_ON_DEMAND           "Tracy on-demand mode (connect manually; disabling lets Tracy buffer startup data so you can profile init)"  OFF)

# ---------------------------------------------------------------------------
# Vulkan SDK — find once here; subdirectories use the resulting targets
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED COMPONENTS volk dxc glslangValidator)

# ---------------------------------------------------------------------------
# Third-party libraries
# ---------------------------------------------------------------------------
add_subdirectory(third_party)

# ---------------------------------------------------------------------------
# Output directories
# ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

# ---------------------------------------------------------------------------
# Engine and shader targets
# ---------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(shaders)

add_dependencies(MomoVK Shaders)

# ---------------------------------------------------------------------------
# IDE convenience
# ---------------------------------------------------------------------------
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT MomoVK)
