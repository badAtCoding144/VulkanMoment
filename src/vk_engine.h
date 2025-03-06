// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_types.h>
#include <vk_initializers.h>

#include <vkBootstrap.h>


class VulkanEngine {
public:

	VkInstance _instance;						//Vulkan library
	VkDebugUtilsMessengerEXT _debug_messenger;	//debug output
	VkPhysicalDevice _chosenGPU;				//default Device
	VkDevice _device;							//Logical Device for commands	
	VkSurfaceKHR _surface;						//Vulkan window surface

	VkSwapchainKHR _swapchain;					//swapchain
	VkFormat _swapchainImageFormat;				//format of the swapchain

	std::vector<VkImage> _swapchainImages;			//required datastructures for swapchain
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	bool _isInitialized{ false };				//only init 1 engine
	int _frameNumber {0};
	bool stop_rendering{ false };				
	VkExtent2D _windowExtent{ 1700 , 900 };		//window size

	struct SDL_Window* _window{ nullptr };		//window handle

	static VulkanEngine& Get();					//get engine instance

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

private:
	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();

	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
};
