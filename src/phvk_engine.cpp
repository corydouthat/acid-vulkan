// Copyright (c) 2025, Cory Douthat
//
// Acid Game Engine - Vulkan
// Engine class

#include "phvk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "phvk_initializers.h"
#include "phvk_types.h"

#include <chrono>
#include <thread>

// Singleton instance of the engine
phVkEngine* loaded_engine = nullptr;

phVkEngine& phVkEngine::getLoadedEngine() 
{ 
    return *loaded_engine; 
}

void phVkEngine::init()
{
    // Only one engine initialization is allowed
    assert(loaded_engine == nullptr);
    loaded_engine = this;

    // Initialize SDL and create a window with it
    SDL_Init(SDL_INIT_VIDEO);

    // SDL window flags
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

	// Create the window
    window = SDL_CreateWindow(
        "Acid Engine (Vulkan)",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        window_extent.width,
        window_extent.height,
        window_flags);

    is_initialized = true;
}

void phVkEngine::cleanup()
{
    if (is_initialized) {

        SDL_DestroyWindow(window);
    }

    // Clear engine pointer
    loaded_engine = nullptr;
}

void phVkEngine::draw()
{
    // TODO
}

void phVkEngine::run()
{
    SDL_Event sdl_event;
    bool sdl_quit = false;

    // main loop
    while (!sdl_quit) {

        // Handle queued events
        while (SDL_PollEvent(&sdl_event) != 0) {
            
			// Close the window on user close action (Alt+F4, X button, etc.)
            if (sdl_event.type == SDL_QUIT)
                sdl_quit = true;

            // Handle minimize and restore events
            if (sdl_event.type == SDL_WINDOWEVENT) {
                if (sdl_event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (sdl_event.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }
        }

        // Do not draw if minimized
        if (stop_rendering) 
        {
            // Sleep thread
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}