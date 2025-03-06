﻿//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#include <SDL_scancode.h>

#include <vk_initializers.h>
#include <vk_types.h>
#include <fmt/core.h> // Add this include at the top of the file

// --- other includes ---
#include <vk_types.h>
#include <vk_initializers.h>

//bootstrap library
#include "VkBootstrap.h"

#include <chrono>
#include <thread>

constexpr bool bUseValidationLayers = false;

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }


void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    init_vulkan();
    init_swapchain();
    init_commands();
    init_sync_structures();

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::init_vulkan() {

    vkb::InstanceBuilder builder;

    //make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Vulkan Application")
        .request_validation_layers(bUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    vkb::Instance vkb_inst = inst_ret.value();


    //grab instance
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;


    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features.dynamicRendering = true;
    features.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

	vkb::PhysicalDeviceSelector selector{ vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features)
        .set_required_features_12(features12)
        .set_surface(_surface)
        .select()
        .value();

	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

    vkb::Device vkbDevice = deviceBuilder.build().value();

    _device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;

}



void VulkanEngine::destroy_swapchain() {
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);    //first delete swapchain object which deletes the images held internally

		for (int i = 0; i < _swapchainImageViews.size(); i++) {
			vkDestroyImageView(_device, _swapchainImageViews[i], nullptr); //destroy the imageView for every image
		}
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {

	vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU, _device, _surface };
    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        //use vsync present mode
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width,height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    _swapchainExtent = vkbSwapchain.extent;
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();

}


void VulkanEngine::init_swapchain() {
	create_swapchain(_windowExtent.width, _windowExtent.height);
}

void VulkanEngine::init_commands() {
}

void VulkanEngine::init_sync_structures() {

}




//usually destroying the engine is done in the reverse order of creation, if we know what we doing we could deviate from this but not now
//in this case swapchain -> device -> surface -> instance -> SDL Window
void VulkanEngine::cleanup()
{
    if (_isInitialized) {

        destroy_swapchain();

		vkDestroyDevice(_device, nullptr);

		vkDestroySurfaceKHR(_instance, _surface, nullptr);


        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger, nullptr); //idk if this can be put in different order tbh = accidental validation layer test mayb

		vkDestroyInstance(_instance, nullptr);


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
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }
            else if (e.type == SDL_KEYDOWN) {

                // Replace the problematic line with the following:
                fmt::print("{}\n", e.key.keysym.sym);
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    bQuit = true;
                }
            }

            // do not draw if we are minimized
            if (stop_rendering) {
                // throttle the speed to avoid the endless spinning
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            draw();
        }
    }
}