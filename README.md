# what is this?
* This is a Vulkan Game Engine built using [vkguide.dev](https://vkguide.dev/) as a base. The primary purpose is for me to continue learning Graphics Programming. I'll be working on this continuously, adding what I find to be interesting, and eventually making a game out of those parts.

# features
* HLSL and GLSL shader support.
* Buffer Device Address and Vertex Pulling.
* Dynamic rendering, descriptor indexing (bindless) & sync2.
* GLTF loading (models & textures).
* Frustum Culling & Draw Sorting
* bindless textures & material system.
* Png Texture Loading with Mipmaps. 
* Everything's annotated in debug & support for the renderdoc API.
* super basic lighting and pbr.

# planned features
* lots of project structure stuff first and foremost. after that it's adding slang & doing the neural shading course. 

# How to Build:
* [Install Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
* [Install CMake](https://cmake.org/)
* Open CMake-gui
* Fill in the source code / build location like this, then click configure and then generate. you can also check some custom debug params here if you want. my options are prefixed with MOMOVK_
<img width="1148" height="392" alt="image" src="https://github.com/user-attachments/assets/87c68351-2c45-4b2e-862e-c9cc5f492be4" />

* open the .slnx in /build and compile

# tracy how to set up
you might need to enable long paths on your system: `git config --system core.longpaths true`
* build the tracyProfiler target. 
* run tracy-profiler.exe
* make sure TRACY_ON_DEMAND is set to off in cmake, if it's on, rebuild tracy client
* connect, run game, close game.

# depenency graph / doxygen & graphviz
* install [doxygen](https://doxygen.nl/download.html) & [graphvis](https://graphviz.org/download/)
* build the doxygen target
* open index.html in build/doxygen. 

# dependencies
* based on vkguide.
* fastgltf, fmt, glm, imgui, sdl, stb_image, tracy, vkbootstrap, vma, volk, renderdoc. 
