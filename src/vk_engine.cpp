﻿//> includes
#include "vk_engine.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_scancode.h>

#include <vk_initializers.h>
#include <vk_types.h>
#include <fmt/core.h> // Add this include at the top of the file



//bootstrap library
#include "VkBootstrap.h"

#include <chrono>
#include <thread>

#include<vk_images.h>
 
constexpr bool bUseValidationLayers = true;

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

    //use vkbootstrap to get a graphics queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

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
    //create a command pool for commands submitted to graphics q
    //want the pool to allow for resetting individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = {}; //allocating a vulkan struct to 0 is usually safe -> dont leave uninitialized data in the struct
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = nullptr;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; //allow individual command buffer resets -> alternatively we can reset the entire pool
	commandPoolInfo.queueFamilyIndex = _graphicsQueueFamily; //command pool will create commands that are compatible with queues that have graphics capability

    for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool)); //create command pool -> vkcheck ensures it succeeds or aborts
        //commandpool will be overwritten if this succeeds


        //allocate default command buffer to use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = {};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; //I had to change this to get rid of some errors because the type wrong
        //we are allocating command buffers but never calling the actual function so we could remove some of this -> look at  file vk_initializers.cpp -> 
        //VkCommandBufferAllocateInfo vkinit::command_buffer_allocate_info

        cmdAllocInfo.pNext = nullptr;
		cmdAllocInfo.commandPool = _frames[i]._commandPool; //specify which command pool to allocate from
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; 
		cmdAllocInfo.commandBufferCount = 1; //allocate a single command buffer -> can do more if needed -> make sure we have enough space though
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer)); //allocate command buffer -> the parent pool must be specified

     
    
    }

}

void VulkanEngine::init_sync_structures() {

    //create synchronization structs
    //one fence to control gpu render finish
    //2 semaphores to synchronize the swapchains 2 frames
    //fence to start signalled so we wait for the first frame
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));
    
    }

}






//usually destroying the engine is done in the reverse order of creation, if we know what we doing we could deviate from this but not now
//in this case swapchain -> device -> surface -> instance -> SDL Window
void VulkanEngine::cleanup()
{
    if (_isInitialized) {

        //make sure gpu is done executing
        vkDeviceWaitIdle(_device);

        for (int i = 0; i < FRAME_OVERLAP; i++) {

            //already written from before
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

            //destroy sync objects
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);

            //flush
			_frames[i]._DeletionQueue.flush();
        }

        //flush the main deletion queue 2
        _mainDeletionQueue.flush();

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
    //wait for the gpu to finish rendering the last frame (timeout at 1 sec)
	VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));

    //delete previous frames stuff.
    get_current_frame()._DeletionQueue.flush();

	VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    //request image from the swapchain
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore,nullptr, &swapchainImageIndex));

    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    //now we are sure the commands finish executing and we reset it to begin recording again
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    //being buffer recording -> we will use this exactly once
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    //start command buffer recording
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));


    //make the swapchain image into writeable mode before render
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    //make clear color from fram number - will flash with 120 frame period
    VkClearColorValue clearValue;
    float flash = std::abs(std::sin(_frameNumber / 120.f));
	clearValue = { {0.0f, 0.0f, flash, 1.0f} };

	VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
	vkCmdClearColorImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
	//transition image back so the swapchain can present it
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //finallize the command buffer so we cant add commands but we execute
	VK_CHECK(vkEndCommandBuffer(cmd));

    //prepare submission to queue
	//wait on _presentSemaphore, signal on _renderSemaphore
    //we will signal _renderSemaphore to signal render done
	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);

	VkSubmitInfo2 submitInfo = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    //submit command buffer to q and execute it
    //_renderr fence will now block until execution is done
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, get_current_frame()._renderFence));


    //prepare info 
    //this will put the image we rendered into the window
    //we wait for the _renderSemaphore ot signal
    VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	//prepare for next frame
	_frameNumber++;



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