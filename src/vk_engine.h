#pragma once

#include "vk_types.h"

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};

	vk::Extent2D _windowExtent{ 1920 , 1080 };

	struct SDL_Window* _window{ nullptr };

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();
};
