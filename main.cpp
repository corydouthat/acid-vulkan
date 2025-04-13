// Copyright (c) 2025, Cory Douthat
//
// Acid Game Engine - Vulkan
// Testbench

#include "phvk_engine.h"

int main()
{
	phVkEngine engine;

	engine.init();

	engine.run();

	engine.cleanup();

	return 0;
}
