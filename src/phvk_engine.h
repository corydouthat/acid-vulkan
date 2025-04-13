// Copyright (c) 2025, Cory Douthat
//
// Acid Game Engine - Vulkan
// Engine class

#pragma once

#include <phvk_types.h>

class phVkEngine {
public:

	// TODO: make data private and add accessor functions?
	bool is_initialized { false };
	int frame_number { 0 };
	bool stop_rendering { false };
	VkExtent2D window_extent { 1700 , 900 };

	struct SDL_Window* window { nullptr };

	// Gets the loaded engine instance
	static phVkEngine& getLoadedEngine();

	// Initializes engine
	void init();

	// Shut down and cleanup
	void cleanup();

	// Draw loop
	void draw();

	// Execute main loop (including draw)
	void run();
};