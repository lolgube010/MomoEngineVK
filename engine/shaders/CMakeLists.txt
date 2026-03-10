# --- CONFIGURATION & FLAGS ---

set(SHADER_DIR_DEBUG   "${PROJECT_SOURCE_DIR}/shaders/bin/debug")
set(SHADER_DIR_RELEASE "${PROJECT_SOURCE_DIR}/shaders/bin/release")

file(MAKE_DIRECTORY ${SHADER_DIR_DEBUG})
file(MAKE_DIRECTORY ${SHADER_DIR_RELEASE})

set(GLSL_FLAGS_DBG -g -Od)
set(GLSL_FLAGS_REL) 

# Initialize list to hold all compiled SPIR-V files
set(SPIRV_BINARY_FILES)

# recurse for headers (.glsl / .hlsli)

file(GLOB_RECURSE GLSL_INCLUDE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.glsl"
)

file(GLOB_RECURSE HLSL_INCLUDE_FILES CONFIGURE_DEPENDS 
	"${CMAKE_CURRENT_SOURCE_DIR}/*.hlsli"
)

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
        COMMAND Vulkan::glslangValidator -V ${GLSL_FLAGS_DBG} ${GLSL} -o ${SPV_DBG}
        DEPENDS ${GLSL} ${GLSL_INCLUDE_FILES}
    )

    # Release
    add_custom_command(
        OUTPUT ${SPV_REL}
        COMMAND Vulkan::glslangValidator -V ${GLSL_FLAGS_REL} ${GLSL} -o ${SPV_REL}
        DEPENDS ${GLSL} ${GLSL_INCLUDE_FILES}
    )

    list(APPEND SPIRV_BINARY_FILES ${SPV_DBG} ${SPV_REL})
endforeach(GLSL)

# --- HLSL COMPILATION ---
# -fvk-use-scalar-layout for BDA / uint64_t
set(DXC_BASE_FLAGS -spirv -HV 2021 -fspv-target-env=vulkan1.3 -fvk-use-scalar-layout)
set(DXC_FLAGS_DBG -Zi -Qembed_debug -Od -fspv-debug=vulkan-with-source)
set(DXC_FLAGS_REL -O3) 

macro(compile_hlsl FILE_LIST PROFILE ENTRY_POINT)
    foreach(HLSL ${${FILE_LIST}})
        get_filename_component(FILE_NAME ${HLSL} NAME)
        message(STATUS "Configuring HLSL Shader: ${FILE_NAME}")
        set(SPV_DBG "${SHADER_DIR_DEBUG}/${FILE_NAME}.spv")
        set(SPV_REL "${SHADER_DIR_RELEASE}/${FILE_NAME}.spv")
        
        # Debug
        add_custom_command(
            OUTPUT ${SPV_DBG}
            COMMAND Vulkan::dxc_exe -T ${PROFILE} -E ${ENTRY_POINT} ${DXC_BASE_FLAGS} ${DXC_FLAGS_DBG} ${HLSL} -Fo ${SPV_DBG}
            DEPENDS ${HLSL} ${HLSL_INCLUDE_FILES}
            COMMENT "Compiling ${FILE_NAME} (Debug) [${PROFILE}]"
            VERBATIM
        )

        # Release
        add_custom_command(
            OUTPUT ${SPV_REL}
            COMMAND Vulkan::dxc_exe -T ${PROFILE} -E ${ENTRY_POINT} ${DXC_BASE_FLAGS} ${DXC_FLAGS_REL} ${HLSL} -Fo ${SPV_REL}
            DEPENDS ${HLSL} ${HLSL_INCLUDE_FILES}
            COMMENT "Compiling ${FILE_NAME} (Release) [${PROFILE}]"
            VERBATIM
        )
        
        list(APPEND SPIRV_BINARY_FILES ${SPV_DBG} ${SPV_REL})
    endforeach()
endmacro()

# Scan specifically inside the current directory
file(GLOB_RECURSE HLSL_VERT_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.vert.hlsl")
compile_hlsl(HLSL_VERT_FILES "vs_6_6" "main")

file(GLOB_RECURSE HLSL_FRAG_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.frag.hlsl")
compile_hlsl(HLSL_FRAG_FILES "ps_6_6" "main")

file(GLOB_RECURSE HLSL_COMP_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.comp.hlsl")
compile_hlsl(HLSL_COMP_FILES "cs_6_6" "main")

# --- FINAL TARGET ---

set(ALL_SHADER_SOURCES
    ${GLSL_SOURCE_FILES}
    ${GLSL_INCLUDE_FILES}
    ${HLSL_VERT_FILES}
    ${HLSL_FRAG_FILES}
    ${HLSL_COMP_FILES}
    ${HLSL_INCLUDE_FILES}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Shaders" FILES ${ALL_SHADER_SOURCES})

set_source_files_properties(${ALL_SHADER_SOURCES} PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(${ALL_SHADER_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)

add_custom_target(
	Shaders
	ALL # makes shaders build automatically when the solution is built.
    DEPENDS ${SPIRV_BINARY_FILES}
	SOURCES ${ALL_SHADER_SOURCES}
)

add_custom_command(
    TARGET Shaders
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "--- Shaders Built (Debug & Release) ---"
    VERBATIM
)