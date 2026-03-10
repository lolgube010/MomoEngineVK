# what is this?
* This is a Vulkan Game Engine built using [vkguide.dev](https://vkguide.dev/) as a base. The primary purpose is for me to continue learning Graphics Programming. I'll be working on this continuously, adding what I find to be interesting, and eventually making a game out of those parts.

# features
* HLSL and GLSL shader support.
* Buffer Device Address and Vertex Pulling.
* dynamic rendering, descriptor indexing (bindless) & sync2. 
* GLTF loading (models & textures).
* Basic Frustum Culling & Draw Sorting
* Png Texture Loading & Mipmaps.
* renderdoc API.
* super basic lighting and pbr.

# planned features
I have a lot of things planned for this engine, but overarching goals are...
* More modern rendering techniques. (full PBR, support for multiple setups like deferred, forward+, clustered etc). 
* More recent Vulkan extensions.
* More "Game-Engine support" (ECS etc).

# How to Build:
* [Install Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
* [Install CMake](https://cmake.org/)
* Open CMake-gui
* Fill in the source code / build location like this, then click configure and then generate.
<img width="1148" height="392" alt="image" src="https://github.com/user-attachments/assets/87c68351-2c45-4b2e-862e-c9cc5f492be4" />

* open the .slnx in /build and compile

# tracy how to set up
you might need to enable long paths on your system:
* git config --system core.longpaths true

build the tracyProfiler target. 
run tracy-profiler.exe

# depenency graph / doxygen & graphviz
* install doxygen & graphvis
* build the doxygen target
* open index.html in build/doxygen. 

# dependencies
* based on vkguide.
* fastgltf, fmt, glm, imgui, sdl, stb_image, tracy, vkbootstrap, vma, volk, renderdoc. 
