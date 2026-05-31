cmake_minimum_required(VERSION 3.25)

project(MomoVK
    DESCRIPTION "MomoEngineVK - a modern Vulkan graphics engine"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# set up library grouping.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Global Linter Configuration (Enforce Includes & Styles)
# ---------------------------------------------------------------------------
find_program(CLANG_TIDY_EXE 
    NAMES clang-tidy
    #HINTS "C:/Program Files/LLVM/bin" # Force CMake to check here on Windows
)

if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    # Forces clang-tidy to run on all C++ targets (like MomoVK) during compilation
    set(CMAKE_CXX_CLANG_TIDY 
        ${CLANG_TIDY_EXE}
        "-extra-arg=-Wno-unknown-warning-option"
    )
else()
    message(WARNING "clang-tidy not found! Header enforcement will not run.")
endif()

# ---------------------------------------------------------------------------
# CMake options (declared at root so they appear in CMake-GUI / cmake -L)
# ---------------------------------------------------------------------------
option(MOMOVK_USE_PCH            "Use precompiled headers (turn OFF to verify headers are self-contained)" ON)
option(MOMOVK_ENABLE_DEBUG_NAMES "Enable Vulkan object debug naming in Debug builds" OFF)
option(MOMOVK_ENABLE_RENDERDOC   "Enable RenderDoc in-app API integration"           OFF)
option(TRACY_ENABLE              "Enable Tracy profiling client"                      OFF)
option(TRACY_ON_DEMAND           "Tracy on-demand mode (connect manually; disabling lets Tracy buffer startup data so you can profile init)"  OFF)
option(TRACY_GPU_ENABLE          "Enable Tracy Vulkan GPU zones (vkGetQueryPoolResults has ~10ms/frame driver overhead on some GPUs; disable for clean CPU profiling)" OFF)

# ---------------------------------------------------------------------------
# Vulkan SDK — find once here; subdirectories use the resulting targets
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED COMPONENTS volk dxc glslangValidator)

find_program(Vulkan_SLANGC_EXECUTABLE NAMES slangc HINTS "$ENV{VULKAN_SDK}/Bin")
if(Vulkan_SLANGC_EXECUTABLE)
    message(STATUS "Found slangc: ${Vulkan_SLANGC_EXECUTABLE}")
    add_executable(Vulkan::slangc IMPORTED GLOBAL)
    set_property(TARGET Vulkan::slangc PROPERTY IMPORTED_LOCATION "${Vulkan_SLANGC_EXECUTABLE}")
else()
    message(STATUS "slangc not found — Slang shader compilation disabled")
endif()

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

add_dependencies(MomoVK MomoShaders)

# ---------------------------------------------------------------------------
# IDE convenience
# ---------------------------------------------------------------------------
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT MomoVK)
