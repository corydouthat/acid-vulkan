// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Main Engine Class

#pragma once

#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// TODO: singleton?
template <typename T>
class phVkEngine
{
private:
    // PRIVATE DATA
    SDL_Window* window = nullptr;

public:
    // PUBLIC FUNCTIONS

    bool initWindow(uint32_t width, uint32_t height, std::string title);

    void cleanup();

    phVkEngine() { ; }
    ~phVkEngine() { cleanup(); }

private:
    // PRIVATE FUNCTIONS

};


template <typename T>
bool phVkEngine<T>::initWindow(uint32_t width, uint32_t height, std::string title)
{
	// Initialize SDL and create a window with it
	SDL_Init(SDL_INIT_VIDEO);

	// SDL window flags
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    // Create the window
    window = SDL_CreateWindow(
        title.c_str(),
        width,
        height,
        window_flags);

    if (window == nullptr)
    {
        return false;
    }
    else
    {
        return true;
    }
}


template <typename T>
void phVkEngine<T>::cleanup()
{
    // Destroy the window
	SDL_DestroyWindow(window);
	
    // Quit SDL
	SDL_Quit();
}
