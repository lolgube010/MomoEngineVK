cmake_minimum_required (VERSION 3.25)

project ("MomoVK")

find_package(Vulkan REQUIRED COMPONENTS volk dxc glslangValidator)

add_subdirectory(third_party)

# Set base binary output (though we define specific shader paths below)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_subdirectory(src)
add_subdirectory(shaders)

add_dependencies(MomoVK Shaders)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT MomoVK)