# what is this?
* engine built using vkguide as a base. will continue on this and add whatever I find interesting. the idea is to eventually make a low-scope-game with this. (wow!)

# features
* hlsl and glsl shader support.
* Buffer Device Address and Vertex Pulling.
* dynamic rendering & descriptor indexing (bindless textures). 
* GLTF loading (models & textures).
* Basic Frustum Culling & Draw Sorting
* Png Texture Loading & Mipmaps.

# planned features
* everything unreal&unity has but better and cooler and faster and more awesomer
* no but, whatever I fixate on is what I'll implement. we'll see. I have too many features planned to write them down, really. 

# How to Build:
* [Install Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
* Maybe enable some debug stuff in the config. you can always do this later.
* [Install CMake](https://cmake.org/)
* Open CMake-gui
* Fill in the source code / build location like this, then click configure and then generate.
<img width="1148" height="392" alt="image" src="https://github.com/user-attachments/assets/87c68351-2c45-4b2e-862e-c9cc5f492be4" />

* open the .slnx in /build
* set 'MomoVK' as startup project, and then compile.

# tracy how to set up
you might need to enable long paths on your system:
* git config --system core.longpaths true

build the tracyProfiler target. 

# depenency graph / doxygen & graphviz
install doxygen & graphvis
build the doxygen target
open index.html in build/doxygen. 

# dependencies
* based on vkguide.
* fastgltf, fmt, glm, imgui, sdl, stb_image, tracy, vkbootstrap, vma, volk.
