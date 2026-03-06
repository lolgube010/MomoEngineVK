find_package(Vulkan REQUIRED)

add_library(vkbootstrap STATIC)
add_library(glm INTERFACE)
add_library(vma INTERFACE)

add_library(stb_image INTERFACE)
add_library(renderdoc_app INTERFACE)

add_subdirectory(fastgltf)
add_subdirectory(fmt EXCLUDE_FROM_ALL)
add_subdirectory(SDL EXCLUDE_FROM_ALL)

target_sources(vkbootstrap PRIVATE 
  vkbootstrap/VkBootstrap.h
  vkbootstrap/VkBootstrap.cpp
  )

target_include_directories(vkbootstrap PUBLIC vkbootstrap)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
set_property(TARGET vkbootstrap PROPERTY CXX_STANDARD 20)

#both vma and glm and header only libs so we only need the include path
target_include_directories(vma INTERFACE vma)

target_include_directories(glm INTERFACE glm)

#add_library(sdl2 INTERFACE)

#target_include_directories(sdl2 INTERFACE $ENV{VULKAN_SDK}/Include/SDL2 )
#target_link_directories(sdl2 INTERFACE $ENV{VULKAN_SDK}/Lib )

#target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)

target_sources(imgui PRIVATE 
  imgui/imgui.h
  imgui/imgui.cpp

  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_impl_vulkan.cpp
  imgui/imgui_impl_sdl2.cpp
  )

target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL2::SDL2)

target_include_directories(stb_image INTERFACE stb_image)
target_include_directories(renderdoc_app INTERFACE renderdoc_app)

if(NOT TARGET Tracy::TracyClient)
	option(TRACY_ENABLE "Enable Tracy profiling" ON)
	option(TRACY_ON_DEMAND "Tracy on-demand mode (connect manually)" ON)   # ← often useful
	add_subdirectory(tracy)
endif()

option(TRACY_HAS_VULKAN "Enable Vulkan support in Tracy" ON)

# Define the paths based on your tracy subdirectory
set(TRACY_PROFILER_SRC_DIR "${PROJECT_SOURCE_DIR}/third_party/tracy/profiler")
set(TRACY_PROFILER_BUILD_DIR "${TRACY_PROFILER_SRC_DIR}/build/Release")

# Create a target that essentially runs your manual cmd instructions
add_custom_target(TracyProfiler
    # Step 1: Configure
    # We use ${CMAKE_COMMAND} to call the exact same CMake executable that is currently running.
    # We also pass your current Generator and Architecture so it matches the main project seamlessly.
    COMMAND ${CMAKE_COMMAND} 
            -B "${TRACY_PROFILER_BUILD_DIR}" 
            -S "${TRACY_PROFILER_SRC_DIR}" 
            -G "${CMAKE_GENERATOR}" 
            -A "${CMAKE_GENERATOR_PLATFORM}"

    # Step 2: Build (Always forcing Release mode since it's a heavy UI tool)
    COMMAND ${CMAKE_COMMAND} 
            --build "${TRACY_PROFILER_BUILD_DIR}" 
            --config Release

    WORKING_DIRECTORY "${TRACY_PROFILER_SRC_DIR}"
    COMMENT "Configuring and building the Tracy Profiler standalone..."
    VERBATIM
)

# Optional: Group it nicely in a "Tools" folder in Visual Studio
set_target_properties(TracyProfiler PROPERTIES FOLDER "Tools")

# --- DXC Runtime Support ---

# 1. Find the header (dxc/dxcapi.h)
find_path(DXC_INCLUDE_DIR 
    NAMES dxc/dxcapi.h 
    HINTS "$ENV{VULKAN_SDK}/Include"
)

# 2. Find the library (.lib on Windows, .so on Linux)
find_library(DXC_LIB
    NAMES dxcompiler
    HINTS "$ENV{VULKAN_SDK}/Lib" "$ENV{VULKAN_SDK}/lib"
)

# 3. Create the imported target
add_library(dxcompiler SHARED IMPORTED GLOBAL)

# Set the include headers (so you can do #include <dxc/dxcapi.h>)
target_include_directories(dxcompiler INTERFACE "${DXC_INCLUDE_DIR}")

if(WIN32)
    # On Windows, we link against the .lib but need the .dll at runtime
    set_target_properties(dxcompiler PROPERTIES
        IMPORTED_IMPLIB "${DXC_LIB}"
    )

    # Find the DLL specifically for copying later
    find_file(DXC_DLL
        NAMES dxcompiler.dll
        HINTS "$ENV{VULKAN_SDK}/Bin"
    )
    
    # Associate the DLL with the target
    set_target_properties(dxcompiler PROPERTIES
        IMPORTED_LOCATION "${DXC_DLL}"
    )
else()
    # On Linux, the .so file is used for both linking and runtime
    set_target_properties(dxcompiler PROPERTIES
        IMPORTED_LOCATION "${DXC_LIB}"
    )
endif()

# doxygen and include graphs:
# 1. Ask CMake to find Doxygen and Graphviz on your system
find_package(Doxygen)

if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found! Setting up include graph generation.")

    # 2. Set your Doxygen settings directly in CMake
    set(DOXYGEN_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/doxyGen")
    
    # Graphing settings
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_HAVE_DOT YES)
    set(DOXYGEN_INCLUDE_GRAPH YES)
    set(DOXYGEN_INCLUDED_BY_GRAPH YES)
    
    # Input settings
    set(DOXYGEN_RECURSIVE YES)
    # Exclude third party to keep the graphs readable!
    set(DOXYGEN_EXCLUDE "${PROJECT_SOURCE_DIR}/third_party")

    # 3. Create the command to generate the docs
    # Notice the 'ALL' keyword: This is what makes it auto-run!
    doxygen_add_docs(
        GenerateIncludeGraphs          # The name of the target in Visual Studio
        ${PROJECT_SOURCE_DIR}/src      # The input folder to scan
    )
else()
    message(WARNING "Doxygen not found. Include graphs will not be generated.")
endif()