
# 1. Create a variable called SOURCE_FILES containing all .cpp and .h files
# CONFIGURE_DEPENDS ensures the build system re-checks the folder for new files
file(GLOB SOURCE_FILES CONFIGURE_DEPENDS 
    "*.cpp" 
    "*.h"
)

# Add source to this project's executable.
add_executable (MomoVK ${SOURCE_FILES})

set_property(TARGET MomoVK PROPERTY CXX_STANDARD 20)
target_compile_definitions(MomoVK PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(MomoVK PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(MomoVK PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)

target_precompile_headers(MomoVK PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <vulkan/vulkan.h>)

if(WIN32)
  add_custom_command(TARGET MomoVK POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:MomoVK> $<TARGET_FILE_DIR:MomoVK>
    COMMAND_EXPAND_LISTS
    )
endif()