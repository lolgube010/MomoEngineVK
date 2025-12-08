# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("vulkan_guide")


find_package(Vulkan REQUIRED)

add_subdirectory(third_party)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_subdirectory(src)

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
find_program(DXC_COMPILER dxc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

# Initialize list to hold all compiled SPIR-V files
set(SPIRV_BINARY_FILES)

# --- GLSL COMPILATION ---
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

# --- HLSL COMPILATION ---
# Helper macro to reduce repetition for HLSL
# USAGE: compile_hlsl(FILE_LIST PROFILE ENTRY_POINT)
macro(compile_hlsl FILE_LIST PROFILE ENTRY_POINT)
    foreach(HLSL ${${FILE_LIST}})
        get_filename_component(FILE_NAME ${HLSL} NAME)
        # Output: shader.vert.hlsl -> shader.vert.hlsl.spv
        set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
        
        message(STATUS "Configuring HLSL Shader: ${FILE_NAME} as ${PROFILE}")

        add_custom_command(
            OUTPUT ${SPIRV}
            # DXC Flags:
            # -spirv : Output SPIR-V
            # -T : Target Profile (vs_6_0, ps_6_0, etc.)
            # -E : Entry point (defaulting to 'main' here)
            # -Fo : Output file
            COMMAND ${DXC_COMPILER} -spirv -T ${PROFILE} -E ${ENTRY_POINT} ${HLSL} -Fo ${SPIRV}
            DEPENDS ${HLSL}
        )
        
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach()
endmacro()

# 1. Vertex Shaders (*.vert.hlsl) -> vs_6_0
file(GLOB_RECURSE HLSL_VERT_FILES "${PROJECT_SOURCE_DIR}/shaders/*.vert.hlsl")
compile_hlsl(HLSL_VERT_FILES "vs_6_0" "main")

# 2. Fragment/Pixel Shaders (*.frag.hlsl) -> ps_6_0
file(GLOB_RECURSE HLSL_FRAG_FILES "${PROJECT_SOURCE_DIR}/shaders/*.frag.hlsl")
compile_hlsl(HLSL_FRAG_FILES "ps_6_0" "main")

# 3. Compute Shaders (*.comp.hlsl) -> cs_6_0
file(GLOB_RECURSE HLSL_COMP_FILES "${PROJECT_SOURCE_DIR}/shaders/*.comp.hlsl")
compile_hlsl(HLSL_COMP_FILES "cs_6_0" "main")

# --- FINAL TARGET ---
add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )
