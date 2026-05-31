# ---------------------------------------------------------------------------
# Source files — CONFIGURE_DEPENDS re-checks the folder for new files
# ---------------------------------------------------------------------------
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE SOURCE_CPP_FILES CONFIGURE_DEPENDS "*.cpp")

# The gameplay module compiles into its own hot-reloadable DLL (MomoGame, defined at
# the bottom of this file), so its sources (and headers, for IDE grouping) are pulled
# out of the engine executable. Headers still resolve in the exe via the include dir;
# this only moves where they appear in Solution Explorer.
file(GLOB_RECURSE GAME_CPP_FILES    CONFIGURE_DEPENDS "game/*.cpp")
file(GLOB_RECURSE GAME_HEADER_FILES CONFIGURE_DEPENDS "game/*.h")
list(REMOVE_ITEM SOURCE_CPP_FILES ${GAME_CPP_FILES})
list(REMOVE_ITEM HEADER_FILES     ${GAME_HEADER_FILES})

set(SOURCE_FILES ${HEADER_FILES} ${SOURCE_CPP_FILES})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files" FILES ${HEADER_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${SOURCE_CPP_FILES})
source_group("CMake" REGULAR_EXPRESSION "cmake_pch")

if(WIN32)
	list(APPEND SOURCE_FILES "${CMAKE_SOURCE_DIR}/assets/ico/resource.rc")
endif()

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
		GLM_ENABLE_EXPERIMENTAL
		VK_NO_PROTOTYPES          # Required: routes all Vulkan calls through volk
	PRIVATE
		# Tracy: 1 when enabled, 0 when disabled — keeps the macro defined either way
		TRACY_ENABLE=$<IF:$<BOOL:${TRACY_ENABLE}>,1,0>
		$<$<AND:$<BOOL:${TRACY_ENABLE}>,$<BOOL:${TRACY_GPU_ENABLE}>>:TRACY_GPU_ENABLE>
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
		SDL3::SDL3
		vkbootstrap
		imgui
		fastgltf::fastgltf
	PRIVATE
		Tracy::TracyClient
		Jolt
)

if(MOMOVK_ENABLE_RENDERDOC)
	target_link_libraries(MomoVK PRIVATE renderdoc_app)
endif()

# ---------------------------------------------------------------------------
# Precompiled headers
# ---------------------------------------------------------------------------
# Added heavier headers to the PCH based on Build Insights hot-headers (fmt, glm)
# Keep list minimal and stable to avoid forcing rebuilds when app headers change.
target_precompile_headers(MomoVK PUBLIC
	<chrono>
	<cstdint>
	<deque>
	<fastgltf/types.hpp>
	<filesystem>
	<fmt/core.h>
	<fmt/format.h>
	<format>
	<functional>
	<glm/glm.hpp>
	<glm/gtx/quaternion.hpp>
	<glm/mat4x4.hpp>
	<glm/vec4.hpp>
	<memory>
	<optional>
	<ranges>
	<string>
	<unordered_map>
	<vector>
	<vma/vk_mem_alloc.h>
	<Volk/volk.h>
	<vulkan/vk_enum_string_helper.h>
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
		# SDL3 IMPORTED_LOCATION points to the DLL on Windows
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_PROPERTY:SDL3::SDL3,IMPORTED_LOCATION>
			$<TARGET_FILE_DIR:MomoVK>
		COMMAND_EXPAND_LISTS
	)
endif()

# ---------------------------------------------------------------------------
# MomoGame — hot-reloadable gameplay module (DLL)
#
# Loaded at runtime by the host via LoadLibrary/GetProcAddress (see GameModule),
# never linked against the executable. Output lands in bin/<config> next to the
# exe automatically (CMAKE_RUNTIME_OUTPUT_DIRECTORY), so no copy step is needed.
# ---------------------------------------------------------------------------
add_library(MomoGame SHARED ${GAME_CPP_FILES} ${GAME_HEADER_FILES})

# Group the game files in Solution Explorer (mirrors the MomoVK grouping above).
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files" FILES ${GAME_HEADER_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${GAME_CPP_FILES})

target_compile_features(MomoGame PRIVATE cxx_std_20)
set_target_properties(MomoGame PROPERTIES CXX_EXTENSIONS OFF)

# Same source root so #include "game/...", "input/...", "api/..." resolve as in the exe.
target_include_directories(MomoGame PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# MOMO_GAME_EXPORTS flips GAME_API to __declspec(dllexport) for this build only;
# the host sees the same header with the macro empty.
target_compile_definitions(MomoGame 
PUBLIC
	GLM_ENABLE_EXPERIMENTAL
PRIVATE
	MOMO_GAME_EXPORTS
	GLM_FORCE_DEPTH_ZERO_TO_ONE
)

# The game links its OWN copy of ImGui (separate globals — hence the context +
# allocator handshake in Game_Init). glm for the camera math; SDL3 for the input
# scancode enums used by the camera controller.
target_link_libraries(MomoGame PRIVATE
	imgui
	glm
	SDL3::SDL3
)

# Build the game module whenever the engine builds.
add_dependencies(MomoVK MomoGame)
