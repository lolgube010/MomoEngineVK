# ---------------------------------------------------------------------------
# Shader compilation — GLSL via glslangValidator, HLSL via DXC, Slang via slangc
# Source layout:  shaders/{glsl,hlsl,slang}/{vertex,fragment,compute}/
# Output layout:  shaders/bin/{debug,release}/{glsl,hlsl,slang}/{vertex,fragment,compute}/
# ---------------------------------------------------------------------------

set(SHADER_DIR_DEBUG   "${PROJECT_SOURCE_DIR}/shaders/bin/debug")
set(SHADER_DIR_RELEASE "${PROJECT_SOURCE_DIR}/shaders/bin/release")

foreach(LANG glsl hlsl slang)
    foreach(TYPE vertex fragment compute)
        file(MAKE_DIRECTORY "${SHADER_DIR_DEBUG}/${LANG}/${TYPE}")
        file(MAKE_DIRECTORY "${SHADER_DIR_RELEASE}/${LANG}/${TYPE}")
    endforeach()
endforeach()

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
set(GLSL_FLAGS_DBG -g -Od)
set(GLSL_FLAGS_REL)   # intentionally empty

set(DXC_BASE_FLAGS -spirv -HV 2021 -fspv-target-env=vulkan1.3 -fvk-use-scalar-layout)
set(DXC_FLAGS_DBG  -Zi -Qembed_debug -Od -fspv-debug=vulkan-with-source)
set(DXC_FLAGS_REL  -O3)

set(SLANGC_BASE_FLAGS -target spirv -profile spirv_1_5 -emit-spirv-directly)
set(SLANGC_FLAGS_DBG  -g -O0)
set(SLANGC_FLAGS_REL  -O3)

set(GLSL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glsl/include")
set(SLANG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/slang/include")

set(SPIRV_BINARY_FILES)

# ---------------------------------------------------------------------------
# Header dependency tracking
# ---------------------------------------------------------------------------
file(GLOB_RECURSE GLSL_INCLUDE_FILES  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/glsl/include/*.glsl")
file(GLOB_RECURSE HLSL_INCLUDE_FILES  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/hlsl/include/*.hlsli")
file(GLOB_RECURSE SLANG_INCLUDE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/slang/include/*.slang")

# ---------------------------------------------------------------------------
# Source file discovery
# ---------------------------------------------------------------------------
file(GLOB_RECURSE GLSL_VERT_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/glsl/vertex/*.vert")
file(GLOB_RECURSE GLSL_FRAG_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/glsl/fragment/*.frag")
file(GLOB_RECURSE GLSL_COMP_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/glsl/compute/*.comp")

file(GLOB_RECURSE HLSL_VERT_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/hlsl/vertex/*.hlsl")
file(GLOB_RECURSE HLSL_FRAG_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/hlsl/fragment/*.hlsl")
file(GLOB_RECURSE HLSL_COMP_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/hlsl/compute/*.hlsl")

file(GLOB_RECURSE SLANG_VERT_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/slang/vertex/*.slang")
file(GLOB_RECURSE SLANG_FRAG_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/slang/fragment/*.slang")
file(GLOB_RECURSE SLANG_COMP_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/slang/compute/*.slang")

# ---------------------------------------------------------------------------
# compile_glsl(<file-list-var> <type-subdir>)
# ---------------------------------------------------------------------------
macro(compile_glsl FILE_LIST TYPE_SUBDIR)
    foreach(GLSL IN LISTS ${FILE_LIST})
        get_filename_component(FILE_NAME "${GLSL}" NAME)
        message(STATUS "Configuring GLSL shader: ${FILE_NAME}")

        set(SPV_DBG "${SHADER_DIR_DEBUG}/glsl/${TYPE_SUBDIR}/${FILE_NAME}.spv")
        set(SPV_REL "${SHADER_DIR_RELEASE}/glsl/${TYPE_SUBDIR}/${FILE_NAME}.spv")

        add_custom_command(
            OUTPUT  "${SPV_DBG}"
            COMMAND Vulkan::glslangValidator -V ${GLSL_FLAGS_DBG} -I${GLSL_INCLUDE_DIR} "${GLSL}" -o "${SPV_DBG}"
            DEPENDS "${GLSL}" ${GLSL_INCLUDE_FILES}
            COMMENT "Compiling ${FILE_NAME} (Debug)"
            VERBATIM
        )
        add_custom_command(
            OUTPUT  "${SPV_REL}"
            COMMAND Vulkan::glslangValidator -V ${GLSL_FLAGS_REL} -I${GLSL_INCLUDE_DIR} "${GLSL}" -o "${SPV_REL}"
            DEPENDS "${GLSL}" ${GLSL_INCLUDE_FILES}
            COMMENT "Compiling ${FILE_NAME} (Release)"
            VERBATIM
        )

        list(APPEND SPIRV_BINARY_FILES "${SPV_DBG}" "${SPV_REL}")
    endforeach()
endmacro()

# ---------------------------------------------------------------------------
# compile_hlsl(<file-list-var> <type-subdir> <dxc-profile> <entry-point>)
# ---------------------------------------------------------------------------
macro(compile_hlsl FILE_LIST TYPE_SUBDIR PROFILE ENTRY_POINT)
    foreach(HLSL IN LISTS ${FILE_LIST})
        get_filename_component(FILE_NAME "${HLSL}" NAME)
        message(STATUS "Configuring HLSL shader: ${FILE_NAME} [${PROFILE}]")

        set(SPV_DBG "${SHADER_DIR_DEBUG}/hlsl/${TYPE_SUBDIR}/${FILE_NAME}.spv")
        set(SPV_REL "${SHADER_DIR_RELEASE}/hlsl/${TYPE_SUBDIR}/${FILE_NAME}.spv")

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
# compile_slang(<file-list-var> <type-subdir> <stage>)
# stage: vertex | fragment | compute
# ---------------------------------------------------------------------------
macro(compile_slang FILE_LIST TYPE_SUBDIR STAGE)
    foreach(SLANG IN LISTS ${FILE_LIST})
        get_filename_component(FILE_NAME "${SLANG}" NAME)
        message(STATUS "Configuring Slang shader: ${FILE_NAME} [${STAGE}]")

        set(SPV_DBG "${SHADER_DIR_DEBUG}/slang/${TYPE_SUBDIR}/${FILE_NAME}.spv")
        set(SPV_REL "${SHADER_DIR_RELEASE}/slang/${TYPE_SUBDIR}/${FILE_NAME}.spv")

        add_custom_command(
            OUTPUT  "${SPV_DBG}"
            COMMAND Vulkan::slangc
                    ${SLANGC_BASE_FLAGS} ${SLANGC_FLAGS_DBG}
                    -stage ${STAGE} -entry main
                    -I "${SLANG_INCLUDE_DIR}"
                    "${SLANG}" -o "${SPV_DBG}"
            DEPENDS "${SLANG}" ${SLANG_INCLUDE_FILES}
            COMMENT "Compiling ${FILE_NAME} (Debug) [${STAGE}]"
            VERBATIM
        )
        add_custom_command(
            OUTPUT  "${SPV_REL}"
            COMMAND Vulkan::slangc
                    ${SLANGC_BASE_FLAGS} ${SLANGC_FLAGS_REL}
                    -stage ${STAGE} -entry main
                    -I "${SLANG_INCLUDE_DIR}"
                    "${SLANG}" -o "${SPV_REL}"
            DEPENDS "${SLANG}" ${SLANG_INCLUDE_FILES}
            COMMENT "Compiling ${FILE_NAME} (Release) [${STAGE}]"
            VERBATIM
        )

        list(APPEND SPIRV_BINARY_FILES "${SPV_DBG}" "${SPV_REL}")
    endforeach()
endmacro()

compile_glsl(GLSL_VERT_FILES "vertex")
compile_glsl(GLSL_FRAG_FILES "fragment")
compile_glsl(GLSL_COMP_FILES "compute")

compile_hlsl(HLSL_VERT_FILES "vertex"   "vs_6_6" "main")
compile_hlsl(HLSL_FRAG_FILES "fragment" "ps_6_6" "main")
compile_hlsl(HLSL_COMP_FILES "compute"  "cs_6_6" "main")

if(TARGET Vulkan::slangc)
    compile_slang(SLANG_VERT_FILES "vertex"   "vertex")
    compile_slang(SLANG_FRAG_FILES "fragment" "fragment")
    compile_slang(SLANG_COMP_FILES "compute"  "compute")
else()
    message(STATUS "slangc not found — Slang shaders will not be compiled")
endif()

# ---------------------------------------------------------------------------
# Shaders custom target
# ---------------------------------------------------------------------------
set(ALL_SHADER_SOURCES
    ${GLSL_VERT_FILES} ${GLSL_FRAG_FILES} ${GLSL_COMP_FILES} ${GLSL_INCLUDE_FILES}
    ${HLSL_VERT_FILES} ${HLSL_FRAG_FILES} ${HLSL_COMP_FILES} ${HLSL_INCLUDE_FILES}
    ${SLANG_VERT_FILES} ${SLANG_FRAG_FILES} ${SLANG_COMP_FILES} ${SLANG_INCLUDE_FILES}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Shaders" FILES ${ALL_SHADER_SOURCES})
source_group("CMake" REGULAR_EXPRESSION "CMakeFiles|CMakeLists\\.txt")
set_source_files_properties(${ALL_SHADER_SOURCES} PROPERTIES
    HEADER_FILE_ONLY TRUE
    VS_TOOL_OVERRIDE  "None"
)

add_custom_target(MomoShaders ALL
    DEPENDS ${SPIRV_BINARY_FILES}
    SOURCES ${ALL_SHADER_SOURCES}
)

# put target in momo folder
set_target_properties(MomoShaders PROPERTIES FOLDER "Momo")
