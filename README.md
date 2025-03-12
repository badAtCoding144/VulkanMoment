# Vulkan Notes

# Main Render Loop logic

we make a `FrameData` Struct that will contain the objects we need to synchronize the CPU and GPU execution in the main loop - it looks as follows

```
struct FrameData{
    VkSemaphore _swapchainSemaphore, _renderSemaphore; 
    VkFence _renderFence;
}
```
We need 2 semaphores -> `_swapchainSemaphore` is to wait for our swapchain image request by our render commands
`_renderSemaphore` will be used to control presenting and image to the OS once the drawing finishes.
Our `_renderFence` will let us wait for the draw commands of a frame to finish.

For our fence we use the flag `VK_FENCE_CREATE_SIGNALED_BIT`. This allows us to wait on an initialized fence without causing errors - this will block the thread when calling WaitFences the first frame when not set.

`vkWaitForFences` and `vkResetFences` - always reset a fence after using it.

Nanoseconds for the timeout if you use it with timeout 0 you can use it to know if GPU is still executing or not?.

request an index `vkAcquireNextImageKHR` will request the index from the swapchain and if it doesn't have an image it will block the thread for timeout set - in NANOseconds.

We send in `_swapchainSemaphore` this is to make sure we can sync other operations with holding a ready image to display.

I don't think we used this initialization function in the code so go check out line 171 in file `vk_engine.cpp` I think we should call the function mentioned there to set our command buffer params but I made it work.

We start our draw section of the renderloop by resetting our command buffers `cmd = get_current_frame()._mainCommandBuffer -> vkResetComandBuffer(cmd,0)` this removes all commands and frees the buffers memory.
We then create a `VkCommandBufferBeginInfo` and give it the flage `VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT` -> slight speedup apparently from command encoding if we tell the drivers it will only be submitted and executed once, this works because we do one submit per frame.
We then use the aforementioned `cmd` and `VkCommandBufferBeginInfo` to `vkBeginCommandBuffer` which starts recording commands into the buffer again.

First we transition (you can find this function in `vk_images.cpp/h`) the swapchain image to a drawable layout and then perform `VkCmdClear` on it and then transition it back for a display optimal layout -> take note of how we use a pipeline barrier here (`VkImageMemoryBarrier2`).
In this we set the old and new layouts of the image, in the `StageMask` we are doing `ALL_COMMANDS`. It's quite inefficient -> TODO: figure out how to use more accurate `StageMask` for example in a post-process chain.
`ALL_COMMANDS` just means that the barrier will block the gpu completely at the barrier - it's possible to overlap the GPU pipeline across the barrier if you know what you're doing -> `AccessMask` is similar to `StageMask`.

We set `VK_ACCESS_2_MEMORY_READ_BIT` and `VK_ACCESS_2_MEMORY_WRITE_BIT` as our source and destination -> these are generics.

READ THIS: https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples

As part of the barrier we need a `VkImageSubresourceRange` -> there is an init function for this in `vk_initializers.h/cpp` -> the `aspectMask` is the most important parameter to pass and will be `VK_IMAGE_ASPECT_COLOR_BIT` or `VK_IMAGE_ASPECT_DEPTH_BIT` we will use the former since we not doing depth buffers and shadow maps and stuff yet - just show color image boss.

We will keep it as image except when the target layout is `VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL` - which we use with depth buffers.

Once we have the range and the barrier, we put them in a `VkDependencyInfo` and call `VkCmdPipelineBarrier2` - it's possible to layout transition multiple images at once by sending more `imageMemoryBarrier`s which could improve performance if we are doing transitions or barriers for multiple things at once.

We then start drawing using `vk_images.h` in `vk_engine.cpp` 

We begin by transitioning the swapchain image. `VK_IMAGE_LAYOUT_UNDEFINED` Is the “dont care” layout. Its also the layout newly created images will be at. We use it when we dont care about the data that is already in the image, and we are fine with the GPU destroying it.

The target we want is the `VK_IMAGE_LAYOUT_GENERAL`. This is a general purpose layout allowing reading and writing from the image. It's not optimal for rendering but we want to use `vkCmdClearColorImage`. This is what we use if we want to write an image from a compute shader -> for read only images or an image to be used with rasterization commands there are other options.

https://docs.vulkan.org/spec/latest/chapters/resources.html#resources-image-layouts

We calculate a clear color through a basic formula with the _frameNumber - cycling through a sine functions works for now - interpolating through a black and blue color.

`vkCmdClearColorImage` requires 3 main parameters - an image, a clear color and a subresource range for what part of the image to clear (kind of like a stencil??)

After executing the clear command we need to transition the image to `VK_IMAGE_LAYOUT_PRESENT_KHR` which is the only image layout that the swapchain allows for presenting to screen - at the end we call `vkEndCommandBuffer`

Next we want to connect the synchronization structures for the logic to interact correctly with the swapchain before we go and call `VkQueueSubmit2` (part of synchronization2).

We require a `VkSubmitInfo2` which contains information on the semaphores used as part of the submit -> but we require a `VkSemaphoreSubmitInfo` for each of the semaphores it uses, and a `VkCommandBufferSubmitInfo` for the command buffers that will be enqueued - check out `vk_initializers.cpp`.


`VkCommandBufferSubmitiInfo` only needs the command buffer handle - we are leaving the device mask as 0. -> https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkCommandBufferSubmitInfo

`VkSemaphoreSubmitInfo` requires a stage mask which is the same as with `transition_image` -> other than that we only need a semaphore handle. -> https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkSemaphoreSubmitInfo

The device index param is only used for multi-gpu semaphore use cases.

Value is used for timeline semaphores which work through a counter instead of a binary state - we will not use them so we can default it to 1.

`VkSubmitInfo2` arranges everything together. It needs the comand submit info, and the semaphore wait and signal infos. -> https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkSubmitInfo2

We are using 1 semaphore each forr waiting and signaling -> it is possible to signal or wait on multiple at once.

After creating each of the different info structs we call `vkQueueSubmit2` -> for command info we send the command we just recorded.

Our wait info we will use the swapchain semaphore of the current frame. 

`vkAcquireNextImageKHR` will set the same semaphore to be signalled -> we make sure that the commands executed here wont begin until the swapchain image is ready.

For the fence we are using `current_frame()._renderFence`, at the start of the draw loop we waited for this fence to be ready.

Lastly we present the frame using `vkQeueuPresent` which has a similar param strructure as the queue submit -> it also has the pointers for the semaphores, but it has an image index and swapchain index.

We wait on the _renderSemaphore and connect it to the swapchain.

At the end of the function we increment our frame counter.



# Deletion Queue

As we add more vulkan structs we need to handle their cleanup. 

We are adding a new struct to the engine called a `DeletionQueue` -> this is a common approach that involves adding things we want to delete into a queue and then running through the queue to delete them in the correct order, sparing us the effort of writing the long cleanup function and having to keep it synchronized.

We are keeping it simple and having it store std::function callbacks in a deque.

We are using a FIFO so that when we flush the deletion it destroys the objects added last first.

The std::function stores a lambda, and we can use it to store a callback with some data.

Doing callbacks similar to this is innefficient at scale, because we are storing whole std::functions for every object we are deleting.

A better implementation would be to store arrays of vulkan handles and delete them from a loop.

We are adding a `_deletionQueue` and `_mainDeletionQueue` to our `FrameData` struct and `VulkanEngine` class.

We then call it from 2 places - right after we wait on the Fence per frame and from the `cleanup()` function after the `WaitIdle` call.

By flushing it right after the fence we make sure that the GPU finishes execution for that frame so we can delete our objects for that specific frame safely. We also want to make sure we free those per frame resources when destorying the rest of the frame data.

Now whenever we add new vulkan objects we just add them to our deletionqueue






# Image Layouts
GPUs store images in different formats for different needs in their memory. An image layout is the vulkan abstraction over these formats. A image that is read-only is not going to be on the same layout as one that will be written to. To change the layout of an image, vulkan uses pipeline barriers. A pipeline barrier is a way to syncronize the commands from a single command buffer, but it also can do things such as convert the image layouts. How the layouts are implemented varies, and some transitions will actually be a no-op on some hardware. To get the transitions right, it is imperative to use validation layers, which are going to check that images are on their correct layouts for any given GPU operation. If not done, its very common to have code that works completely fine on NVidia hardware, but causes glitches on AMD, or the opposite.

The image we get from the swapchain is going to be in an invalid state, so if we want to use a `VkCmdDraw` on it, or any other draw operation, we need to first transition the image into a writeable layout. And once we are done with the draw commands, we need to transition it into the layout that the swapchain wants for screen output.

On older versions of vulkan, these layout transitions would be done as part of a RenderPass.Not on vulkan 1.3 howevver so we use dynamic rendering, which means we will do those transitions manually, on the other side, we save all of the work and complexity of building a full renderpass.

https://docs.vulkan.org/spec/latest/chapters/resources.html#resources-image-layouts

# Render Loop:
I am using double-buffered render structures. This way while the gpu is busy executing a frame worth of commands, the CPU can continue with the next frame. But once the next frame is calculated, we need to stop the CPU until the first frame is executed so that we can record its commands again.

For the render work, we need to syncronize it with the swapchain structure. If we were doing headless drawing where we dont need to sync with the screen, we wouldnt need this. But we are drawing into a window, so we need to request the OS for an image to draw, then draw on it, and then signal the OS that we want to display that image on the screen.

# Vulkan Objects and their uses:

# VkInstance
The Vulkan API context
The root of all evil, you can enable things like validation layers, instance extensions and loggers for when we debuggenning, evaluating performance or checking branch execution. 
Main thing is validation layers and instance extensions.
We only create a single per application since it is the global context for the app.

# VkPhysicalDevice
This cannot be destroyed in cleanup since it is not a vulkan resource - just a handle.
Our GPU - used to get specifics about our hardware.
Once we have a `VkInstance` we can query it for what GPUs are in the system - vulkan lets us get a list of the GPUs represented by the `VkPhysicalDevice` handle - reference to the GPU.
We lowkey want the user to be able to choose since if they want to use an integrated GPU to save power they should be able to.
We can check the memory size and extensions available for our GPU to measure performance.

## Vulkan physical device Features : 
https://docs.vulkan.org/spec/latest/chapters/limits.html#VkPhysicalDeviceFeatures

https://docs.vulkan.org/spec/latest/chapters/limits.html#VkPhysicalDeviceVulkan12Features

In our initialization code we declare a `vkb::Instancebuilder` and initialize it by setting its name and validation layers, debug messenger, the api version and then call .build().
Grabbing the instance and setting our engines(object) instance and debug messenger to the new Vulkan instances, we can then set features by calling constructors for `VkPhysicalDeviceVulkan13Features` and  `VkPhysicalDeviceVulkan12Features`.

## Some features from Vulkan 13 and 12 and what they do:

### features.dynamicRendering(13)
This allows us to render without needing a `VkRenderPass` and `VkFrameBuffer`.
Simplifies rendering for noobs like me and letting me specify render targets directly in `VkRenderingInfo` instead of creating `VkRenderPass` before.
For more flexible render pipelines
### features.sunchronization2(13)
Enables a more flexible and readable synchronization API that replaces `VkPipelineStateFlags` with `VkPipelineStateFlags2` for better synchro and barriers.
Better multi-threading and GPU workload synchronization.
### features.bufferDeviceAddress(12)
Allows Gpu pointers to be used for buffer memory - ray tracing, linked lists and more advanced GPU operations?
Essential for ray tracing, gpu driven renders and mesh shaders
### features.descriptorIndexing(12)
Enables non uniform indexing and bindless resources, letting shaders dynamically index into large descriptor arrays.
Good for reducing the number of descriptor sets and increasing flexibility in managing GPU resources.
Useful in deferred rendering, bindless textures and GPU-driven pipelines.

### Remember
enabling vulkan 1.3 features does not automatically enable Vulakn 1.2 features so we need to create 2 `VkPhysicalDeviceVulkan(n)Features` or more depending on the sitch.



# VkDevice
The logical GPU that we execute things on.
We create the `VkDevice` using the `VkPhysicalDevice` - this is the GPU driver.
Most Vulkan commands outside debug utils and initialization needs a VkDevice - we create this device and add a list of extensions we want to enable - don't enable extensions you don't need since unnecessary checks will slow our program down.
We can create multiple  `VkDevice`(s) which correspond to different GPUs and use multiple GPUs for the same program - maybe running compute shaders across multiple GPUs.
When we have a physical device we make a `VkDeviceBuilder` by passing the `VkPhysicalDevice` to its constructor and returing the .value field - we can then set our engines chosen GPU and device.


# VkBuffer
GPU visible memory

# VkImage
A texture we can read from and write to

# VkPipeline
Holds our render pipeline -> shaders, rasterization options, depth buffers/stencils or face culling settings for example

# VkCommandBuffer
This records all of our commands to be executed by the GPU followed by submitting them for execution using `VkQueueSubmit`, after these steps execution starts.
When in the Ready state (default after initialization), you can call `vkBeginCommandBuffer()` to put it into the Recording state.
Now you can start inputting commands into it with `vkCmdXXXXX` functions.
When done, call `vkEndCommandBuffer()` to finish the recording the commands and put it in the Executable state where it is ready to be submitted into the GPU.
vkQueueSubmit also accepts submitting multiple command buffers together. Any command buffer that is submitted is put in the Pending state.
Once submitted our commandbuffer is still 'alive', and being consumed by the GPU, at this point it is NOT safe to reset the command buffer yet.
Make sure execution is done (with a fence or some other synchronization tool) and reset the command buffer after using `vkResetCommandBuffer()`.
As we will want to continue drawing the next frame while the command buffer is executed, we are going to double-buffer the commands. 
This way, while the gpu is busy rendering and processing one buffer, we can write into a different one.
https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkCommandBufferAllocateInfo

for more detailed descriptions and specifications regarding the lifecycle of a commandbuffer and its states visit:

https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#commandbuffers-lifecycle



We encode our GPU commands in here - anything not executed in the driver.
The flow of executing a command goes
-> allocate a `VkCommandBuffer` from a `VkCommandPool` 
-> record commands into the command buffer, using `VkCmdXXXXX` functions.
-> submit the command buffer into a `VkQueue`, which starts executing the commands.

in Vulkan recording commands is cheap but submitting them is usually more costly - VkQueueSubmit calls.
We can record commandbuffers in parallel (goated) -> recording multiple commandbuffer from multiple threads is safe
but you need to have 1 `VkCommandPool` and 1 `VkCommandBuffer` per thread (minimum), and make sure that each thread only uses their own command buffers & pools - not actually memory safe.
Once that is done we submit our commandbuffer to out queue but this is not thread safe.
It's common to do the submit in a seperate background thread so we can continue running the render loop.

When we use a command buffer in the main render loop of our program we also need to initialize a `VkCommandBufferBeginInfo` containing flags of the structure type - > we need to pass flags too (VkCOmmandBufferUsageFlags)
we can leave info.pNext and pInheritanceInfo as nullptr tho. Heres a link for commandbufferbegininfo: https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkCommandBufferBeginInfo


# VkQueue
`VkQueueSubmit` is not thread safe.
port for commands to execute - GPUs will have sets of queues with different properties. 
Some allow only certain kinds of commands - command buffers are executed by submitting them to our queue, copying the render commands onto the GPU for execution.
Commands submitted to separate queues may execute at once.
It's possible to create a `VkQueue` specifically for background work and have it separated from the normal rendering.
All queues have a queue family (queue type) in vulkan and this just defines what kinds of commands the queue supports.
Different GPUs support different Queue Families - here is the queue families my GPU supports -> https://vulkan.gpuinfo.org/displayreport.php?id=34212#queuefamilies
It is common to see engines using 3 queue families. One for drawing the frame, other for async compute, and other for data transfer, I will stick to 1 queue for now since n00b.

# VkDescriptorSet
Holds information that connects shader inputs and `VkBuffer` resources and `VkImage` textures. (A set of gpu side pointers that we bind once??? idk sounds like our uniform and input buffer definitions at the top of a shader?)

#VkCommandPool
A `VkCommandPool` is generated using `VkDevice` - we also need an index of the queue family this commandpool will generate commands from.
They said to think of it as an allocator for `VkCommandBuffers`.
You can allocate as many `VkCommandBuffer` as you want from a given pool, but you can only record commands from one thread at a time. 
If you want multithreaded command recording, you need more `VkCommandPool` objects.
So in practice we would probably want to create pair of `VkCommandPools` and `VkCommandBuffers` - but it depends.
https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkCommandPoolCreateInfo


# VkSwapChainKHR
Frame buffer? - it comes from the extension `VK_KHR_swapchain` - a set of framebuffers in opengl terms?

# Swapchain
We use the swapchain to render - it is a OS/windowing structure with images we can draw and display on the screen.
Holds a list of images and image-views accessible by the operating system for display. You usually want to have a swapchain with 2 - 3 images for double/triple buffering
Swapchains are not in the core Vulkan spec as they are optional - for example doing compute shader calculations or offline rendering does not require a swapchain.
Creating a swapchain on a given window size causes us to have to remake it when the window size changes.
The format the swapchain uses for images can be different between platforms and GPUs - so store the image format that the swapchain is set to otherwise we will have artifacts and crashes for sure.
ALWAYS select Present Mode, this controls how the swapchain syncronizes to the screen display.

```
// Provided by VK_KHR_surface
typedef enum VkPresentModeKHR {
    VK_PRESENT_MODE_IMMEDIATE_KHR = 0,                            //SWAPCHAIN DOES NOT WAIT FOR ANYTING - ACCEPTS INSTANT PUSHING OF IMAGES - SCREEN TEAR MOMENT
    VK_PRESENT_MODE_MAILBOX_KHR = 1,                              //HAS A LIST OF IMAGES AND WHILE WE DISPLAY ONE WE CONTINUOUSLY RENDER TO THE OTHERS IN THE LIST - WHEN WE DISPLAY WE SELECT THE MOST RECENT ONE - THIS IS FOR TRIPLE BUFFERING WITHOUT HARD VSYNC
    VK_PRESENT_MODE_FIFO_KHR = 2,                                 //QUEUE OF IMAGES TO PRESENT ON REFRESH INTERVALS, IF THE QUEUE IS FULL WE HAVE TO WAIT FOR IT TO POP BY DISPLAYING IMAGES - STRONG VSYNC TO SCREEN FPS
    VK_PRESENT_MODE_FIFO_RELAXED_KHR = 3,                         //FIFO VSYNC BUT VSYNC IS ADAPTIVE - IF THE APPLICATIONS FPS IS LOWER THAN THE SCREENS IT PUSHES IMAGES IMMEDIATELY CAUSING TEARING.
  // Provided by VK_KHR_shared_presentable_image
    VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR = 1000111000,
  // Provided by VK_KHR_shared_presentable_image
    VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR = 1000111001,
  // Provided by VK_EXT_present_mode_fifo_latest_ready
    VK_PRESENT_MODE_FIFO_LATEST_READY_EXT = 1000361000,
} VkPresentModeKHR;
```


# VkSemaphore
Synchronizes execution of commands (GPU->GPU), used to sync multiple command buffer submissions in succession.
They define operation orders for GPU commands and execute them sequentially. Some Vulkan objects support either signal or wait semaphores.
Multiple operations can wait on a semaphore but only one operation can signal a semaphores completion.

### Pseudocode for 3 operations being executed in order using semaphores:
```
VkSemaphore Task1Semaphore;
VkSemaphore Task2Semaphore;

VkOperationInfo OpAlphaInfo;
// Operation Alpha will signal the semaphore 1
OpAlphaInfo.signalSemaphore = Task1Semaphore;

VkDoSomething(OpAlphaInfo);

VkOperationInfo OpBetaInfo;

// Operation Beta signals semaphore 2, and waits on semaphore 1
OpBetaInfo.signalSemaphore = Task2Semaphore;
OpBetaInfo.waitSemaphore = Task1Semaphore;

VkDoSomething(OpBetaInfo);

VkOperationInfo OpGammaInfo;
//Operation gamma waits on semaphore 2
OpGammaInfo.waitSemaphore = Task2Semaphore;

VkDoSomething(OpGammaInfo);

```
This Vulkan object is obviously used to execute tasks/operations that have dependencies between them.

# VkFence
Synchronizes GPU to CPU exection of commands. Used to confirm if a command buffer has finished executing.
This is an optional parameter for things like `VkQueueSubmit` that can be used to confirm finished execution and synchronize the CPU render loop and the GPU.
The fence should be signalled when we submit it as part of a command using `VkWaitForFences` this is blocking the CPU to wait for GPU execution to finish.

### here is some pseudo code for the logic:
```
//we have a fence object created from somewhere
VkFence myFence;

//start some operation on the GPU
VkSomeOperation(whatever, myFence);

// block the CPU until the GPU operation finishes
VkWaitForFences(myFence);
//fences always have to be reset before they can be used again
VkResetFences(myFence);

```

# Links to docs containing info about the intialization of buffer submit info - semaphore submit nfo - `vksubmitinfo2`

https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkCommandBufferSubmitInfo

https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkSemaphoreSubmitInfo

https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkSubmitInfo2

# Synchronization using image barriers for different use cases : 

https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples




