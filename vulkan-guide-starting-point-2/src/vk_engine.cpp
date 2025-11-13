//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include <chrono>
#include <thread>

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get()
{
	return *loadedEngine;
}

void VulkanEngine::init()
{
	// only one engine initialization is allowed with the application.
	assert(loadedEngine == nullptr);
	loadedEngine = this;

	// We initialize SDL and create a window with it.
	SDL_Init(SDL_INIT_VIDEO);

	auto window_flags = SDL_WINDOW_VULKAN;

	_window = SDL_CreateWindow(
							   "Vulkan Engine",
							   SDL_WINDOWPOS_UNDEFINED,
							   SDL_WINDOWPOS_UNDEFINED,
							   _windowExtent.width,
							   _windowExtent.height,
							   window_flags);

	// everything went fine
	_isInitialized = true;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized)
	{
		SDL_DestroyWindow(_window);
	}
	// clear engine pointer
	loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
	// nothing yet
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	// main loop
	while (!bQuit)
	{
		// Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			// close the window when user alt-f4s or clicks the X button
			if (e.type == SDL_QUIT)
			{
				bQuit = true;
			}

			if (e.type == SDL_WINDOWEVENT)
			{
				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
				{
					stop_rendering = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED)
				{
					stop_rendering = false;
				}
			}

			processInput(e);
		}

		// do not draw if we are minimized
		if (stop_rendering)
		{
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		draw();
	}
}

void VulkanEngine::processInput(SDL_Event& anE)
{
	auto& e = anE;
	switch (e.type)
	{
		// ------------------- KEYBOARD -------------------
		case SDL_KEYDOWN:
			if (!e.key.repeat)
			{
				switch (e.key.keysym.sym)
				{
					case SDLK_w: fmt::print("W pressed\n");
						break;
					case SDLK_s: fmt::print("S pressed\n");
						break;
					case SDLK_a: fmt::print("A pressed\n");
						break;
					case SDLK_d: fmt::print("D pressed\n");
						break;
					case SDLK_LEFT: fmt::print("Left arrow\n");
						break;
					case SDLK_RIGHT: fmt::print("Right arrow\n");
						break;
					case SDLK_UP: fmt::print("Up arrow\n");
						break;
					case SDLK_DOWN: fmt::print("Down arrow\n");
						break;
					case SDLK_SPACE: fmt::print("Space pressed\n");
						break;
						// Add more keys as needed
				}
			}
			break;

		case SDL_KEYUP:
			switch (e.key.keysym.sym)
			{
				case SDLK_w: fmt::print("W released\n");
					break;
				case SDLK_s: fmt::print("S released\n");
					break;
				case SDLK_a: fmt::print("A released\n");
					break;
				case SDLK_d: fmt::print("D released\n");
					break;
				case SDLK_LEFT: fmt::print("Left arrow up\n");
					break;
				case SDLK_RIGHT: fmt::print("Right arrow up\n");
					break;
				case SDLK_UP: fmt::print("Up arrow up\n");
					break;
				case SDLK_DOWN: fmt::print("Down arrow up\n");
					break;
				case SDLK_SPACE: fmt::print("Space released\n");
					break;
			}
			break;

		// ------------------- MOUSE MOTION -------------------
		case SDL_MOUSEMOTION:
			fmt::print("Mouse at: ({}, {})\n", e.motion.x, e.motion.y);
			// e.motion.xrel, e.motion.yrel for relative movement
			break;

		// ------------------- MOUSE BUTTONS -------------------
		case SDL_MOUSEBUTTONDOWN:
			if (e.button.button == SDL_BUTTON_LEFT)
			{
				fmt::print("Left click DOWN at ({}, {})\n", e.button.x, e.button.y);
			}
			else if (e.button.button == SDL_BUTTON_RIGHT)
			{
				fmt::print("Right click DOWN at ({}, {})\n", e.button.x, e.button.y);
			}
			else if (e.button.button == SDL_BUTTON_MIDDLE)
			{
				fmt::print("Middle click DOWN\n");
			}
			break;

		case SDL_MOUSEBUTTONUP:
			if (e.button.button == SDL_BUTTON_LEFT)
			{
				fmt::print("Left click UP\n");
			}
			else if (e.button.button == SDL_BUTTON_RIGHT)
			{
				fmt::print("Right click UP\n");
			}
			else if (e.button.button == SDL_BUTTON_MIDDLE)
			{
				fmt::print("Middle click UP\n");
			}
			break;

		// ------------------- MOUSE WHEEL -------------------
		case SDL_MOUSEWHEEL:
			fmt::print("Mouse wheel: x={} y={}\n", e.wheel.x, e.wheel.y);
			break;
	}
}
