# How to Build:
* [Install Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
* Maybe enable some debug stuff in the config. you can always do this later.
* [Install CMake](https://cmake.org/)
* Open CMake-gui
* Fill in the vars like this, then click generate and then build.
<img width="946" height="170" alt="image" src="https://github.com/user-attachments/assets/5de9a994-abd2-40f4-ada4-40d3c68e479d" />

* open the .slnx in /build
* set 'MomoVK' as startup project, and build

# tracy how to set up
cd path/to/tracy/profiler
cmake -B build -S . -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
