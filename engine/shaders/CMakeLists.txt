# --- CONFIGURATION & FLAGS ---

# 1. Define Output Directories (Relative to the ROOT project, not this folder)
set(SHADER_DIR_DEBUG   "${PROJECT_SOURCE_DIR}/shaders/bin/debug")
set(SHADER_DIR_RELEASE "${PROJECT_SOURCE_DIR}/shaders/bin/release")

# Create directories immediately
file(MAKE_DIRECTORY ${SHADER_DIR_DEBUG})
file(MAKE_DIRECTORY ${SHADER_DIR_RELEASE})

# 2. Define Compiler Flags
set(GLSL_FLAGS_DBG -g -Od)
set(GLSL_FLAGS_REL) 

set(DXC_FLAGS_DBG -Zi -Od -fspv-debug=vulkan-with-source)
set(DXC_FLAGS_REL -O3) 

# 3. Find Tools
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
find_program(DXC_COMPILER dxc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

# Initialize list to hold all compiled SPIR-V files
set(SPIRV_BINARY_FILES)

# --- GLSL COMPILATION ---

# Note: CMAKE_CURRENT_SOURCE_DIR refers to "root/shaders" now
file(GLOB_RECURSE GLSL_SOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.comp"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    message(STATUS "Configuring GLSL Shader: ${FILE_NAME}")

    set(SPV_DBG "${SHADER_DIR_DEBUG}/${FILE_NAME}.spv")
    set(SPV_REL "${SHADER_DIR_RELEASE}/${FILE_NAME}.spv")

    # Debug
    add_custom_command(
        OUTPUT ${SPV_DBG}
        COMMAND ${GLSL_VALIDATOR} -V ${GLSL_FLAGS_DBG} ${GLSL} -o ${SPV_DBG}
        DEPENDS ${GLSL}
    )

    # Release
    add_custom_command(
        OUTPUT ${SPV_REL}
        COMMAND ${GLSL_VALIDATOR} -V ${GLSL_FLAGS_REL} ${GLSL} -o ${SPV_REL}
        DEPENDS ${GLSL}
    )

    list(APPEND SPIRV_BINARY_FILES ${SPV_DBG} ${SPV_REL})
endforeach(GLSL)

# --- HLSL COMPILATION ---

macro(compile_hlsl FILE_LIST PROFILE ENTRY_POINT)
    foreach(HLSL ${${FILE_LIST}})
        get_filename_component(FILE_NAME ${HLSL} NAME)
        
        set(SPV_DBG "${SHADER_DIR_DEBUG}/${FILE_NAME}.spv")
        set(SPV_REL "${SHADER_DIR_RELEASE}/${FILE_NAME}.spv")
        
        message(STATUS "Configuring HLSL Shader: ${FILE_NAME} as ${PROFILE}")

        # Debug
        add_custom_command(
            OUTPUT ${SPV_DBG}
            COMMAND ${DXC_COMPILER} -spirv -T ${PROFILE} -E ${ENTRY_POINT} ${DXC_FLAGS_DBG} ${HLSL} -Fo ${SPV_DBG}
            DEPENDS ${HLSL}
        )

        # Release
        add_custom_command(
            OUTPUT ${SPV_REL}
            COMMAND ${DXC_COMPILER} -spirv -T ${PROFILE} -E ${ENTRY_POINT} ${DXC_FLAGS_REL} ${HLSL} -Fo ${SPV_REL}
            DEPENDS ${HLSL}
        )
        
        list(APPEND SPIRV_BINARY_FILES ${SPV_DBG} ${SPV_REL})
    endforeach()
endmacro()

# Scan specifically inside the current directory
file(GLOB_RECURSE HLSL_VERT_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.vert.hlsl")
compile_hlsl(HLSL_VERT_FILES "vs_6_0" "main")

file(GLOB_RECURSE HLSL_FRAG_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.frag.hlsl")
compile_hlsl(HLSL_FRAG_FILES "ps_6_0" "main")

file(GLOB_RECURSE HLSL_COMP_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.comp.hlsl")
compile_hlsl(HLSL_COMP_FILES "cs_6_0" "main")

# --- FINAL TARGET ---

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_custom_command(
    TARGET Shaders
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "--- Shaders Built (Debug & Release) ---"
    VERBATIM
)