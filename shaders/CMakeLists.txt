# ---------------------------------------------------------------------------
# Shader compilation — GLSL via glslangValidator, HLSL via DXC
# Output: shaders/bin/debug/  and  shaders/bin/release/
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Output directories (created at configure time so they always exist)
# ---------------------------------------------------------------------------
set(SHADER_DIR_DEBUG   "${PROJECT_SOURCE_DIR}/shaders/bin/debug")
set(SHADER_DIR_RELEASE "${PROJECT_SOURCE_DIR}/shaders/bin/release")

file(MAKE_DIRECTORY "${SHADER_DIR_DEBUG}")
file(MAKE_DIRECTORY "${SHADER_DIR_RELEASE}")

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
set(GLSL_FLAGS_DBG -g -Od)
set(GLSL_FLAGS_REL)   # intentionally empty

set(DXC_BASE_FLAGS -spirv -HV 2021 -fspv-target-env=vulkan1.3 -fvk-use-scalar-layout)
set(DXC_FLAGS_DBG  -Zi -Qembed_debug -Od -fspv-debug=vulkan-with-source)
set(DXC_FLAGS_REL  -O3)

# Master list of all SPIR-V outputs — populated by the loops below
set(SPIRV_BINARY_FILES)

# ---------------------------------------------------------------------------
# Header dependency tracking
# *.glsl  — included by GLSL shaders
# *.hlsli — included by HLSL shaders
# ---------------------------------------------------------------------------
file(GLOB_RECURSE GLSL_INCLUDE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.glsl"
)
file(GLOB_RECURSE HLSL_INCLUDE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hlsli"
)

# ---------------------------------------------------------------------------
# GLSL compilation (.vert / .frag / .comp)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE GLSL_SOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.comp"
)

foreach(GLSL IN LISTS GLSL_SOURCE_FILES)
    get_filename_component(FILE_NAME "${GLSL}" NAME)
    message(STATUS "Configuring GLSL shader: ${FILE_NAME}")

    set(SPV_DBG "${SHADER_DIR_DEBUG}/${FILE_NAME}.spv")
    set(SPV_REL "${SHADER_DIR_RELEASE}/${FILE_NAME}.spv")

    add_custom_command(
        OUTPUT  "${SPV_DBG}"
        COMMAND Vulkan::glslangValidator -V ${GLSL_FLAGS_DBG} "${GLSL}" -o "${SPV_DBG}"
        DEPENDS "${GLSL}" ${GLSL_INCLUDE_FILES}
        COMMENT "Compiling ${FILE_NAME} (Debug)"
        VERBATIM
    )
    add_custom_command(
        OUTPUT  "${SPV_REL}"
        COMMAND Vulkan::glslangValidator -V ${GLSL_FLAGS_REL} "${GLSL}" -o "${SPV_REL}"
        DEPENDS "${GLSL}" ${GLSL_INCLUDE_FILES}
        COMMENT "Compiling ${FILE_NAME} (Release)"
        VERBATIM
    )

    list(APPEND SPIRV_BINARY_FILES "${SPV_DBG}" "${SPV_REL}")
endforeach()

# ---------------------------------------------------------------------------
# HLSL compilation macro
#
# compile_hlsl(<file-list-var> <dxc-profile> <entry-point>)
#
# Must be a macro (not a function) so that list(APPEND SPIRV_BINARY_FILES ...)
# modifies the variable in the calling scope.
# ---------------------------------------------------------------------------
macro(compile_hlsl FILE_LIST PROFILE ENTRY_POINT)
    foreach(HLSL IN LISTS ${FILE_LIST})
        get_filename_component(FILE_NAME "${HLSL}" NAME)
        message(STATUS "Configuring HLSL shader: ${FILE_NAME} [${PROFILE}]")

        set(SPV_DBG "${SHADER_DIR_DEBUG}/${FILE_NAME}.spv")
        set(SPV_REL "${SHADER_DIR_RELEASE}/${FILE_NAME}.spv")

        add_custom_command(
            OUTPUT  "${SPV_DBG}"
            COMMAND Vulkan::dxc_exe
                    -T ${PROFILE} -E ${ENTRY_POINT}
                    ${DXC_BASE_FLAGS} ${DXC_FLAGS_DBG}
                    "${HLSL}" -Fo "${SPV_DBG}"
            DEPENDS "${HLSL}" ${HLSL_INCLUDE_FILES}
            COMMENT "Compiling ${FILE_NAME} (Debug) [${PROFILE}]"
            VERBATIM
        )
        add_custom_command(
            OUTPUT  "${SPV_REL}"
            COMMAND Vulkan::dxc_exe
                    -T ${PROFILE} -E ${ENTRY_POINT}
                    ${DXC_BASE_FLAGS} ${DXC_FLAGS_REL}
                    "${HLSL}" -Fo "${SPV_REL}"
            DEPENDS "${HLSL}" ${HLSL_INCLUDE_FILES}
            COMMENT "Compiling ${FILE_NAME} (Release) [${PROFILE}]"
            VERBATIM
        )

        list(APPEND SPIRV_BINARY_FILES "${SPV_DBG}" "${SPV_REL}")
    endforeach()
endmacro()

# ---------------------------------------------------------------------------
# HLSL source file discovery and compilation
# Add a new stage here (e.g. mesh shaders): glob *.mesh.hlsl, call compile_hlsl
# ---------------------------------------------------------------------------
file(GLOB_RECURSE HLSL_VERT_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.vert.hlsl")
file(GLOB_RECURSE HLSL_FRAG_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.frag.hlsl")
file(GLOB_RECURSE HLSL_COMP_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.comp.hlsl")

compile_hlsl(HLSL_VERT_FILES "vs_6_6" "main")
compile_hlsl(HLSL_FRAG_FILES "ps_6_6" "main")
compile_hlsl(HLSL_COMP_FILES "cs_6_6" "main")

# ---------------------------------------------------------------------------
# Shaders custom target
# ---------------------------------------------------------------------------
set(ALL_SHADER_SOURCES
    ${GLSL_SOURCE_FILES}
    ${GLSL_INCLUDE_FILES}
    ${HLSL_VERT_FILES}
    ${HLSL_FRAG_FILES}
    ${HLSL_COMP_FILES}
    ${HLSL_INCLUDE_FILES}
)

# Show shaders in Visual Studio's Solution Explorer under a "Shaders" folder,
# and prevent VS from attempting to compile them natively.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Shaders" FILES ${ALL_SHADER_SOURCES})
set_source_files_properties(${ALL_SHADER_SOURCES} PROPERTIES
    HEADER_FILE_ONLY TRUE
    VS_TOOL_OVERRIDE  "None"
)

add_custom_target(Shaders ALL
    DEPENDS ${SPIRV_BINARY_FILES}
    SOURCES ${ALL_SHADER_SOURCES}
)
