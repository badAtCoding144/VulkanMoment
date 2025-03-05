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
Frame buffer? - it comes from the extension VK_KHR_swapchain - everyone mentioning swapchain I'll have to see what it's about.

### VkSemaphore
Synchronizes execution of commands (GPU->GPU), used to sync multiple command buffer submissions in succession.

### VkFence
Synchronizes GPU to CPU exection of commands. Used to confirm if a command buffer has finished executing.


