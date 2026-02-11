# what is this?
* engine built using vkguide as a base. will continue on this and add whatever I find interesting. the idea is to eventually make a low-scope-game with this. (wow!)

# features
* hlsl and glsl

#planned features
* everything unreal&unity has but better and cooler and faster and more awesomer
* no but, whatever I fixate on is what I'll implement. we'll see.

# How to Build:
* [Install Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
* Maybe enable some debug stuff in the config. you can always do this later.
* [Install CMake](https://cmake.org/)
* Open CMake-gui
* Fill in the source code / build location like this, then click generate and then build.
<img width="1148" height="392" alt="image" src="https://github.com/user-attachments/assets/87c68351-2c45-4b2e-862e-c9cc5f492be4" />

* open the .slnx in /build
* set 'MomoVK' as startup project, and build

# tracy how to set up
cd path/to/tracy/profiler
cmake -B build -S . -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
