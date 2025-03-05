# Vulkan Notes

## Vulkan Objects that I know about and their use:

### VkInstance
The Vulkan API context
The root of all evil, you can enable things like validation layers, instance extensions and loggers for when we debuggenning, evaluating performance or checking branch execution. 
Main thing is validation layers and instance extensions.
We only create a single per application since it is the global context for the app.

### VkPhysicalDevice
Our GPU - used to get specifics about our hardware.
Once we have a VkInstance we can query it for what GPUs are in the system - vulkan lets us get a list of the GPUs represented by the VkPhysicalDevice handle - reference to the GPU.
We lowkey want the user to be able to choose since if they want to use an integrated GPU to save power they should be able to.
We can check the memory size and extensions available for our GPU to measure performance.

### VkDevice
The logical GPU that we execute things on.
We create the VkDevice using the VkPhysical device - this is the GPU driver.
Most Vulkan commands outside debug utils and initialization needs a VkDevice - we create this device and add a list of extensions we want to enable - don't enable extensions you don't need since unnecessary checks will slow our program down.
We can create multiple  VkDevices which correspond to different GPUs and use multiple GPUs for the same program - maybe running compute shaders across multiple GPUs.

### VkBuffer
GPU visible memory

### VkImage
A texture we can read from and write to

### VkPipeline
Holds our render pipeline -> shaders, rasterization options, depth buffers/stencils or face culling settings for example

### VkCommandBuffer
We encode our GPU commands in here - anything not executed in the driver.

### VkQueue
port for commands to execute - GPUs will have sets of queues with different properties. 
Some allow only certain kinds of commands - command buffers are executed by submitting them to our queue, copying the render commands onto the GPU for execution.

### VkDescriptorSet
Holds information that connects shader inputs and VkBuffer resources and VkImage textures. (A set of gpu side pointers that we bind once??? idk sounds like our uniform and input buffer definitions at the top of a shader?)

### VkSwapChainKHR
Frame buffer? - it comes from the extension VK_KHR_swapchain - a set of framebuffers in opengl terms?

###Swapchain
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


### VkSemaphore
Synchronizes execution of commands (GPU->GPU), used to sync multiple command buffer submissions in succession.

### VkFence
Synchronizes GPU to CPU exection of commands. Used to confirm if a command buffer has finished executing.


# Vulkan Notes

## Vulkan Objects that I know about and their use:

### VkInstance
The Vulkan context

### VkPhysicalDevice
Our GPU - used to get specifics about our hardware

### VkDevice
The logical GPU that we execute things on

### VkBuffer
GPU visible memory

### VkImage
A texture we can read from and write to

### VkPipeline
Holds our render pipeline -> shaders, rasterization options, depth buffers/stencils or face culling settings for example

### VkCommandBuffer
We encode our GPU commands in here - anything not executed in the driver.

### VkQueue
port for commands to execute - GPUs will have sets of queues with different properties. 
Some allow only certain kinds of commands - command buffers are executed by submitting them to our queue, copying the render commands onto the GPU for execution.

### VkDescriptorSet
Holds information that connects shader inputs and VkBuffer resources and VkImage textures. (A set of gpu side pointers that we bind once??? idk sounds like our uniform and input buffer definitions at the top of a shader?)

### VkSwapChainKHR
Frame buffer? - it comes from the extension VK_KHR_swapchain - a set of framebuffers in opengl terms?

### VkSemaphore
Synchronizes execution of commands (GPU->GPU), used to sync multiple command buffer submissions in succession.

### VkFence
Synchronizes GPU to CPU exection of commands. Used to confirm if a command buffer has finished executing.


