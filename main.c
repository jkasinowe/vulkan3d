#define SDL_MAIN_USE_CALLBACKS 1
#define VK_NO_PROTOTYPES
#include <volk.h>
#include <shaderc/shaderc.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_log.h>
#include <shaderc/env.h>
#include <shaderc/status.h>
#include <stdint.h>
#include <string.h>


#define ARRLEN(x) (sizeof(x) / sizeof(*x))

#define APP_COLOR_FORMAT VK_FORMAT_B8G8R8A8_SRGB
#define APP_DEPTH_FORMAT VK_FORMAT_D32_SFLOAT
#define APP_SHADER_PATH "src/shaders/"
#define APP_MAX_FRAMES_IN_FLIGHT 2



struct Window {
  SDL_Window *sdl_window;
  uint32_t width;
  uint32_t height;
};

struct Swapchain {
  uint32_t width;
  uint32_t height;
  VkFormat format;
  VkSwapchainKHR vk_swapchain;
  uint32_t image_count;
  VkImage *images;
  uint32_t image_view_count;
  VkImageView *image_views;
  uint32_t semaphore_count;
  VkSemaphore *semaphores;
  bool requireRecreate;
};

#define APP_WAIT "WAIT:  "
#define APP_OK "OK:    "
#define APP_ERROR "ERROR: "

struct Shader {
  VkShaderModule vertex;
  VkShaderModule fragment;
};

struct FrameResources {
  VkCommandPool commandPool;
  VkCommandBuffer commandBuffer;
  VkSemaphore imageAcquiredSemaphore;
};


struct Vulkan {
  uint32_t version;
  uint32_t instanceExtensionCount;
  const char **instanceExtensionNames;
  VkInstance instance;
  VkSurfaceKHR surface;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  struct Shader shader;
  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;
  VkSemaphore timelineSemapthore;
  struct FrameResources framesInFlight[APP_MAX_FRAMES_IN_FLIGHT];
  uint64_t nextSignalValue;  
};

struct Depth {
  VkFormat format;
  VkImage image;
  VmaAllocation image_allocation;
  VkImageView image_view;
};


struct Renderer {
  struct Window window;
  struct Vulkan vulkan;
  uint32_t graphicsQueueFamilyId;
  VkQueue graphicsQueue;
  VmaAllocator allocator;
  struct Swapchain swapchain;
  struct Depth depth;
  uint32_t frameIndex;
  
};

struct Renderer Renderer = {0};

static VKAPI_ATTR VkBool32 VKAPI_CALL
LOG_DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                  void *pUserData) {

  SDL_Log("%s", pCallbackData->pMessage);
  return VK_FALSE;
}

void VkResultLog(VkResult result) {
  const char *str = string_VkResult(result);
  SDL_Log("Vulkan error: %s\n", str);
  abort();
}

#define VkResultCheck(x)                                                       \
  do {                                                                         \
    VkResult result = x;                                                       \
    if (result != VK_SUCCESS) {                                                \
      VkResultLog(result);                                                     \
      return SDL_APP_FAILURE;                                                  \
    }                                                                          \
  } while (0)

#define SDL_ResultCheck(x)                                                     \
  if (!x) {                                                                    \
    SDL_Log("%s", SDL_GetError());                                             \
    return SDL_APP_FAILURE;                                                    \
  }


SDL_AppResult APP_SwapchainCreate();
void APP_SwapchainDestroy();
SDL_AppResult APP_ShadersCreate();
void APP_ShadersDestroy();
SDL_AppResult APP_PipelineCreate(VkPipeline* pipeline);
void APP_PipelineDestroy(VkPipeline *pipeline);
SDL_AppResult APP_SyncResourcesCreate();
void APP_SyncResourcesDestroy();
SDL_AppResult APP_CommandBuffersCreate();
void APP_CommandBuffersDestroy();

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  SDL_ResultCheck(SDL_Init(SDL_INIT_VIDEO));
  Renderer.window.width = 800;
  Renderer.window.height = 600;
  Renderer.swapchain.format = APP_COLOR_FORMAT;
  Renderer.depth.format = APP_DEPTH_FORMAT;
  Renderer.vulkan.nextSignalValue = APP_MAX_FRAMES_IN_FLIGHT + 1;
  
  Renderer.window.sdl_window =
      SDL_CreateWindow("Vulkan + SDL3", Renderer.window.width,
                       Renderer.window.height, SDL_WINDOW_VULKAN);
  SDL_ResultCheck(Renderer.window.sdl_window);

  // Initializing vulkan
  VkResult res = volkInitialize();
  if (res != VK_SUCCESS) {
    SDL_Log("Failed to initialize volk with error %d\n", res);
    return SDL_APP_FAILURE;
  }

  vkEnumerateInstanceVersion(&Renderer.vulkan.version);
  SDL_Log(APP_OK "Vulkan API version: %d.%d.%d",
          VK_API_VERSION_MAJOR(Renderer.vulkan.version),
          VK_API_VERSION_MINOR(Renderer.vulkan.version),
          VK_API_VERSION_PATCH(Renderer.vulkan.version));

  //------------------------------------------------------------------------------
  // Creating VkInstance

  VkApplicationInfo app_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app_info.pNext = NULL;
  app_info.pApplicationName = "Vulkan + SDL3";
  app_info.apiVersion = Renderer.vulkan.version;
  app_info.pEngineName = "Vector";

  char *instance_extension_custom_names[] = {
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
  };

  uint32_t instance_extension_default_count;
  const char *const *instance_extension_default_names =
      SDL_Vulkan_GetInstanceExtensions(&instance_extension_default_count);
  uint32_t instance_extension_custom_count =
      ARRLEN(instance_extension_custom_names);
  Renderer.vulkan.instanceExtensionCount =
      instance_extension_default_count + instance_extension_custom_count;

  Renderer.vulkan.instanceExtensionNames =
      malloc(Renderer.vulkan.instanceExtensionCount * sizeof(char *));
  memcpy(&Renderer.vulkan.instanceExtensionNames[0],
         instance_extension_default_names,
         instance_extension_default_count * sizeof(char *));

  memcpy(
      &Renderer.vulkan.instanceExtensionNames[instance_extension_default_count],
      instance_extension_custom_names,
      instance_extension_custom_count * sizeof(char *));

  const char *InstanceRequestedLayers[] = {
      "VK_LAYER_KHRONOS_validation",
  };

  SDL_Log("%d extensions:", Renderer.vulkan.instanceExtensionCount);
  for (uint32_t i = 0; i < Renderer.vulkan.instanceExtensionCount; i++) {
    SDL_Log("%s", Renderer.vulkan.instanceExtensionNames[i]);
  }

  VkDebugUtilsMessengerCreateInfoEXT debug_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  debug_info.pNext = NULL;
  debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
  debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug_info.pfnUserCallback = LOG_DebugCallback;

  VkInstanceCreateInfo instance_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};

  instance_info.pNext = &debug_info;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledExtensionCount = Renderer.vulkan.instanceExtensionCount;
  instance_info.ppEnabledExtensionNames =
      Renderer.vulkan.instanceExtensionNames;
  instance_info.enabledLayerCount = ARRLEN(InstanceRequestedLayers);
  instance_info.ppEnabledLayerNames = InstanceRequestedLayers;

  SDL_Log(APP_WAIT "Creating a vulkan instance");
  VkResultCheck(
      vkCreateInstance(&instance_info, NULL, &Renderer.vulkan.instance));
  SDL_Log(APP_OK "Vulkan instance created");

  SDL_Log(APP_WAIT "Loading vulkan instance");
  volkLoadInstance(Renderer.vulkan.instance);
  SDL_Log(APP_OK "Vulkan instance loaded");

  // Create surface
  //------------------------------------------------------------------------------------------------------
  SDL_Log(APP_WAIT "Creating vulkan surface");
  SDL_ResultCheck(SDL_Vulkan_CreateSurface(Renderer.window.sdl_window,
                                           Renderer.vulkan.instance, NULL,
                                           &Renderer.vulkan.surface));
  SDL_Log(APP_OK "Vulkan surface created");

  // Find physical device
  //------------------------------------------------------------------------------------------------------
  SDL_Log(APP_WAIT "Looking for physical devices");

  uint32_t physical_device_count;
  vkEnumeratePhysicalDevices(Renderer.vulkan.instance, &physical_device_count,
                             NULL);
  if (!physical_device_count) {
    SDL_Log(APP_ERROR "no physical devices found");
    return SDL_APP_FAILURE;
  }

  SDL_Log(APP_OK "Found %d physical devices", physical_device_count);
  SDL_Log(APP_WAIT "Choosing the best one");
  VkPhysicalDevice *physical_devices =
      malloc(physical_device_count * sizeof(VkPhysicalDevice));
  vkEnumeratePhysicalDevices(Renderer.vulkan.instance, &physical_device_count,
                             physical_devices);
  Renderer.vulkan.physicalDevice = physical_devices[0];

  for (size_t i = 0; i < physical_device_count; i++) {
    VkPhysicalDeviceProperties props = {0};
    vkGetPhysicalDeviceProperties(physical_devices[i], &props);
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      Renderer.vulkan.physicalDevice = physical_devices[i];
      SDL_Log(APP_OK "Found discrete GPU");
      break;
    }
  }

  uint32_t physical_device_format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer.vulkan.physicalDevice,
                                       Renderer.vulkan.surface,
                                       &physical_device_format_count, NULL);
  VkSurfaceFormatKHR *physical_device_formats =
      malloc(physical_device_format_count * sizeof(VkFormat));

  vkGetPhysicalDeviceSurfaceFormatsKHR(
      Renderer.vulkan.physicalDevice, Renderer.vulkan.surface,
      &physical_device_format_count, physical_device_formats);

  bool format_supported = false;
  for (uint32_t i = 0; i < physical_device_format_count; i++) {
    if (physical_device_formats[i].format == Renderer.swapchain.format) {
      format_supported = true;
      break;
    }
  }

  if (!format_supported) {
    SDL_Log(APP_ERROR "Requested swapchain format not supported");
    return SDL_APP_FAILURE;
  }

  VkPhysicalDeviceProperties physical_device_properties = {0};
  vkGetPhysicalDeviceProperties(Renderer.vulkan.physicalDevice,
                                &physical_device_properties);
  SDL_Log(APP_OK "Physical device chosen: %s",
          physical_device_properties.deviceName);

  // Find graphics queue
  //------------------------------------------------------------------------------------------------------
  SDL_Log(APP_WAIT "Looking for graphics queue");

  uint32_t graphics_queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties2(Renderer.vulkan.physicalDevice,
                                            &graphics_queue_family_count, NULL);
  SDL_Log(APP_OK "Found %d graphics queue families",
          graphics_queue_family_count);
  VkQueueFamilyProperties2 *graphics_queue_family_properties_array;
  graphics_queue_family_properties_array =
      calloc(graphics_queue_family_count, sizeof(VkQueueFamilyProperties2));
  for (size_t i = 0; i < graphics_queue_family_count; i++) {
    graphics_queue_family_properties_array[i].sType =
        VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
  }
  vkGetPhysicalDeviceQueueFamilyProperties2(
      Renderer.vulkan.physicalDevice, &graphics_queue_family_count,
      graphics_queue_family_properties_array);

  bool found_queue_family_with_present_support = false;
  SDL_Log(APP_WAIT "Choosing one with Present support");
  for (size_t i = 0; i < graphics_queue_family_count; i++) {
    VkQueueFamilyProperties2 *props =
        &graphics_queue_family_properties_array[i];

    VkBool32 has_present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(Renderer.vulkan.physicalDevice, i,
                                         Renderer.vulkan.surface,
                                         &has_present_support);

    if (props->queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT &&
        has_present_support) {
      Renderer.graphicsQueueFamilyId = i;
      found_queue_family_with_present_support = true;
      break;
    }
  }

  if (!found_queue_family_with_present_support) {
    SDL_Log(APP_ERROR "Queue family with Present support not found");
    return SDL_APP_FAILURE;
  }
  SDL_Log(APP_OK "Found graphics queue with Present support");

  // Creating device
  //------------------------------------------------------------------------------------------------------
  VkPhysicalDeviceVulkan14Features supported_features_14 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
      .pNext = NULL};
  VkPhysicalDeviceVulkan13Features supported_features_13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &supported_features_14};
  VkPhysicalDeviceVulkan12Features supported_features_12 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &supported_features_13};
  VkPhysicalDeviceFeatures2 supported_features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &supported_features_12};
  vkGetPhysicalDeviceFeatures2(Renderer.vulkan.physicalDevice,
                               &supported_features);

  SDL_Log(APP_WAIT "Checking for available physical device features");
  if (!supported_features_13.dynamicRendering ||
      !supported_features_13.synchronization2 ||
      !supported_features_12.timelineSemaphore) {
    SDL_Log(APP_ERROR "Physical device doesn't meet the feature requirements");
    return SDL_APP_FAILURE;
  }
  SDL_Log(APP_OK "Physical device meets the feature requirements");

  VkPhysicalDeviceVulkan14Features requested_features_14 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
      .pNext = NULL};
  VkPhysicalDeviceVulkan13Features requested_features_13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &requested_features_14,
      .synchronization2 = VK_TRUE,  // modernized synchronization
      .dynamicRendering = VK_TRUE}; // no renderpass/subpasses

  VkPhysicalDeviceVulkan12Features requested_features_12 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext =
          &requested_features_13, // streamlined multi-frame-in-flight handling
      .timelineSemaphore = VK_TRUE};

  VkPhysicalDeviceFeatures2 requested_features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .pNext = &requested_features_12};

  float queue_priorities[] = {1.0f};
  VkDeviceQueueCreateInfo queue_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = Renderer.graphicsQueueFamilyId,
      .queueCount = ARRLEN(queue_priorities),
      .pQueuePriorities = queue_priorities};

  const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkDeviceCreateInfo device_create_info = {0};
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = &requested_features;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledExtensionCount = ARRLEN(device_extensions);
  device_create_info.ppEnabledExtensionNames = device_extensions;
  device_create_info.pEnabledFeatures = NULL;

  SDL_Log(APP_WAIT "Creating device");
  VkResultCheck(vkCreateDevice(Renderer.vulkan.physicalDevice,
                               &device_create_info, NULL,
                               &Renderer.vulkan.device));
  SDL_Log(APP_OK "Device created");

  SDL_Log(APP_WAIT "Getting device queue");
  vkGetDeviceQueue(Renderer.vulkan.device, Renderer.graphicsQueueFamilyId, 0,
                   &Renderer.graphicsQueue);
  SDL_Log(APP_OK "Device queue ready");

  // Initializing VMA
  //------------------------------------------------------------------------------------------------------
  VmaVulkanFunctions vma_func_info = {0};
  VmaAllocatorCreateInfo vma_allocator_info = {0};
  vma_allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vma_allocator_info.physicalDevice = Renderer.vulkan.physicalDevice;
  vma_allocator_info.device = Renderer.vulkan.device;
  vma_allocator_info.pVulkanFunctions = &vma_func_info;
  vma_allocator_info.instance = Renderer.vulkan.instance;
  vma_allocator_info.vulkanApiVersion = Renderer.vulkan.version;

  SDL_Log(APP_WAIT "Creating VMA allocator");
  vmaImportVulkanFunctionsFromVolk(&vma_allocator_info, &vma_func_info);
  if (vmaCreateAllocator(&vma_allocator_info, &Renderer.allocator) !=
      VK_SUCCESS) {
    SDL_Log(APP_ERROR "Failed to create VMA allocator");
    return SDL_APP_FAILURE;
  }
  SDL_Log(APP_OK "VMA allocator created");

  // Creating swapchain
  //------------------------------------------------------------------------------------------------------
  if (APP_SwapchainCreate() == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }

  // Creating shaders
  //------------------------------------------------------------------------------------------------------
  if (APP_ShadersCreate() == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }

  // Creating pipeline
  //------------------------------------------------------------------------------------------------------
  
  if (APP_PipelineCreate(&Renderer.vulkan.pipeline) == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }

  if (APP_SyncResourcesCreate() == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }

  if (APP_CommandBuffersCreate() == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }    
  
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
  SDL_Log("Frame %d", Renderer.frameIndex); 
  
  if (Renderer.swapchain.requireRecreate) {
    vkDeviceWaitIdle(Renderer.vulkan.device);
    APP_SwapchainDestroy();
    APP_SwapchainCreate();
  }

  const uint32_t frame_resource_index =
      Renderer.frameIndex++ % APP_MAX_FRAMES_IN_FLIGHT;
  const uint64_t signal_value = Renderer.vulkan.nextSignalValue;
  const uint64_t wait_value = signal_value - APP_MAX_FRAMES_IN_FLIGHT;

  VkSemaphoreWaitInfo wait_info = {0};
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  wait_info.semaphoreCount = 1;
  wait_info.pSemaphores = &Renderer.vulkan.timelineSemapthore;
  wait_info.pValues = &wait_value;

  vkWaitSemaphores(Renderer.vulkan.device, &wait_info, UINT64_MAX);


  // After we waited for semaphores, it's safe to record commands
  struct FrameResources *resources =
      &Renderer.vulkan.framesInFlight[frame_resource_index];
  vkResetCommandPool(Renderer.vulkan.device, resources->commandPool, 0);


  VkSemaphore image_acquire_semaphore =
    resources->imageAcquiredSemaphore;

  // Ask for next image in swapchain
  uint32_t image_index = 0;
  VkResult acquire_result = vkAcquireNextImageKHR(
      Renderer.vulkan.device, Renderer.swapchain.vk_swapchain, UINT64_MAX,
      image_acquire_semaphore, VK_NULL_HANDLE, &image_index);

  if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
    Renderer.swapchain.requireRecreate = true;
    return SDL_APP_CONTINUE;
  } else if (acquire_result == VK_SUBOPTIMAL_KHR) {
    Renderer.swapchain.requireRecreate = true;
  }


  // we can begin recording commands
  VkCommandBufferBeginInfo command_begin_info = {0};
  command_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(
      resources->commandBuffer,
      &command_begin_info);

  VkImageMemoryBarrier2 layout_barriers[] = {
    // Barrier that stops collor going to fragment shader before it is all set
    (VkImageMemoryBarrier2){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = Renderer.swapchain.images[image_index],
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }},
    // Barrier that stops depth fragment shader until depth buffer info is set
   (VkImageMemoryBarrier2){
       .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
       .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
       .srcAccessMask = 0,
       .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                       VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
       .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
       .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
       .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
       .image = Renderer.depth.image,
       .subresourceRange = {
           .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
           .baseMipLevel = 0,
           .levelCount = 1,
           .baseArrayLayer = 0,
	   .layerCount = 1,
       }           
    }        
  };


  // finalize layout barriers
  VkDependencyInfo dep_info = {0};
  dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep_info.imageMemoryBarrierCount = ARRLEN(layout_barriers);
  dep_info.pImageMemoryBarriers = layout_barriers;
  
  vkCmdPipelineBarrier2(resources->commandBuffer, &dep_info);

  // setting up the color and debug attachments, begin dynamic rendering
  VkRenderingAttachmentInfo color_attach_info = {0};
  color_attach_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attach_info.imageView = Renderer.swapchain.image_views[image_index];
  color_attach_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attach_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attach_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attach_info.clearValue = (VkClearValue){0.1f, 0.1f, 0.1f, 1};
  color_attach_info.pNext = NULL;

  VkRenderingAttachmentInfo depth_attach_info = {0};
  depth_attach_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depth_attach_info.imageView = Renderer.depth.image_view;
  depth_attach_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depth_attach_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attach_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attach_info.clearValue = (VkClearValue){1.0f, 0};
  depth_attach_info.pNext = NULL;

  VkRenderingInfo rendering_info = {0};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = (VkRect2D){
      .offset = {.x = 0, .y = 0},
      .extent = {.width = Renderer.swapchain.width,
                 .height = Renderer.swapchain.height},
  };
  rendering_info.layerCount = 1;
  rendering_info.colorAttachmentCount = 1;
  rendering_info.pColorAttachments = &color_attach_info;
  rendering_info.pDepthAttachment = &depth_attach_info;

  // Begin dynamic rendering
  vkCmdBeginRendering(resources->commandBuffer, &rendering_info);
  {
    VkViewport viewport = {0};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)Renderer.swapchain.width;
    viewport.height = (float)Renderer.swapchain.height;
    vkCmdSetViewport(resources->commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = Renderer.swapchain.width;
    scissor.extent.height = Renderer.swapchain.height;
    vkCmdSetScissor(resources->commandBuffer, 0, 1, &scissor);
    
    // drawing the triangle YOOOHOOOOO
    vkCmdBindPipeline(resources->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      Renderer.vulkan.pipeline);
    vkCmdDraw(resources->commandBuffer, 3, 1, 0, 0);
  }
  // End dynamic rendering
  vkCmdEndRendering(resources->commandBuffer);




  // dont present until everything has been written to
  VkImageMemoryBarrier2 present_layout_barrier = {0};
  present_layout_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  present_layout_barrier.srcStageMask =
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  present_layout_barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  present_layout_barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  present_layout_barrier.dstAccessMask = 0;
  present_layout_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  present_layout_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  present_layout_barrier.image = Renderer.swapchain.images[image_index];
  present_layout_barrier.subresourceRange =
      (VkImageSubresourceRange){.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .baseMipLevel = 0,
                                .levelCount = 1,
                                .baseArrayLayer = 0,
                                .layerCount = 1};
  VkDependencyInfo present_dep_info = {0};
  present_dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  present_dep_info.imageMemoryBarrierCount = 1;
  present_dep_info.pImageMemoryBarriers = &present_layout_barrier;
  vkCmdPipelineBarrier2(resources->commandBuffer, &present_dep_info);

  
  vkEndCommandBuffer(resources->commandBuffer);

  VkSemaphoreSubmitInfo image_acquire_wait_info = {0};
  image_acquire_wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  image_acquire_wait_info.semaphore = image_acquire_semaphore;
  image_acquire_wait_info.stageMask =
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;


  // Binary semaphores for each flight and timeline for gpu
  VkSemaphoreSubmitInfo semaphore_signals[] = {
      (VkSemaphoreSubmitInfo){
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
          .semaphore =
              Renderer.swapchain.semaphores[image_index], //????????????
          .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
      },
      (VkSemaphoreSubmitInfo){.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                              .semaphore = Renderer.vulkan.timelineSemapthore,
                              .value = signal_value,
                              .stageMask =
                                  VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT}};

  VkCommandBufferSubmitInfo command_submit_info = {0};
  command_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  command_submit_info.commandBuffer = resources->commandBuffer;
  
  
  VkSubmitInfo2 submit_info = {0};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit_info.waitSemaphoreInfoCount = 1;
  submit_info.pWaitSemaphoreInfos = &image_acquire_wait_info;
  submit_info.commandBufferInfoCount = 1;
  submit_info.pCommandBufferInfos = &command_submit_info;
  submit_info.signalSemaphoreInfoCount = ARRLEN(semaphore_signals);
  submit_info.pSignalSemaphoreInfos = semaphore_signals;

  vkQueueSubmit2(Renderer.graphicsQueue, 1, &submit_info, VK_NULL_HANDLE);

  VkPresentInfoKHR present_info = {0};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &Renderer.swapchain.semaphores[image_index]; // ?
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &Renderer.swapchain.vk_swapchain;
  present_info.pImageIndices = &image_index;
  present_info.pResults = NULL;
  
  vkQueuePresentKHR(Renderer.graphicsQueue, &present_info);
  
  
  return SDL_APP_CONTINUE;
}



SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  switch (event->type) {
  case SDL_EVENT_KEY_DOWN:
    switch (event->key.scancode) {
    case SDL_SCANCODE_ESCAPE:
      return SDL_APP_SUCCESS;
    default:
      return SDL_APP_CONTINUE;
    }
  }
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  vkDeviceWaitIdle(Renderer.vulkan.device);
  APP_CommandBuffersDestroy();
  APP_SyncResourcesDestroy();
  APP_PipelineDestroy(&Renderer.vulkan.pipeline);
  APP_ShadersDestroy();
  APP_SwapchainDestroy();

  if (Renderer.allocator) {
    vmaDestroyAllocator(Renderer.allocator);
  }    

  if (Renderer.vulkan.device) {
    vkDestroyDevice(Renderer.vulkan.device, NULL);
  }
  if (Renderer.vulkan.surface) {
    vkDestroySurfaceKHR(Renderer.vulkan.instance, Renderer.vulkan.surface,
                        NULL);
  }
  if (Renderer.vulkan.instance) {
    vkDestroyInstance(Renderer.vulkan.instance, NULL);
  }
  
  volkFinalize();
}


SDL_AppResult APP_SwapchainCreate() {
  // Creating swapchain
  //------------------------------------------------------------------------------------------------------
  Renderer.swapchain.width = Renderer.window.width;
  Renderer.swapchain.height = Renderer.window.height;
  Renderer.swapchain.requireRecreate = false;
  
  VkSurfaceCapabilitiesKHR surface_capabilities = {0};
  SDL_Log(APP_WAIT "Getting surface capabilities");
  VkResultCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      Renderer.vulkan.physicalDevice, Renderer.vulkan.surface,
      &surface_capabilities));
  SDL_Log(APP_OK "Surface capabilities ready");

  uint32_t requested_image_count =
      SDL_max(2, surface_capabilities.minImageCount);
  if (surface_capabilities.maxImageCount > 0) {
    requested_image_count =
        SDL_min(requested_image_count, surface_capabilities.maxImageCount);
  }

  
  VkSwapchainCreateInfoKHR swapchain_info = {0};
  swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_info.surface = Renderer.vulkan.surface;
  swapchain_info.minImageCount = requested_image_count;
  swapchain_info.imageFormat = Renderer.swapchain.format;
  swapchain_info.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  swapchain_info.imageExtent = (VkExtent2D){
      .width = Renderer.swapchain.width, .height = Renderer.swapchain.height};
  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_info.preTransform = surface_capabilities.currentTransform;
  swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;

  SDL_Log(APP_WAIT "Creating swapchain");
  VkResultCheck(vkCreateSwapchainKHR(Renderer.vulkan.device, &swapchain_info,
                                     NULL, &Renderer.swapchain.vk_swapchain));
  SDL_Log(APP_OK "Swapchain created");

  SDL_Log(APP_WAIT "Loading swapchain images");
  vkGetSwapchainImagesKHR(Renderer.vulkan.device,
                          Renderer.swapchain.vk_swapchain,
                          &Renderer.swapchain.image_count, NULL);
  Renderer.swapchain.images =
      malloc(Renderer.swapchain.image_count * sizeof(VkImage));

  vkGetSwapchainImagesKHR(
      Renderer.vulkan.device, Renderer.swapchain.vk_swapchain,
      &Renderer.swapchain.image_count, Renderer.swapchain.images);
  SDL_Log(APP_OK "Swapchain images loaded");
  
  SDL_Log(APP_WAIT "Creating swapchain imageviews");
  Renderer.swapchain.image_view_count = Renderer.swapchain.image_count;
  Renderer.swapchain.image_views = malloc(Renderer.swapchain.image_view_count*sizeof(VkImageView));
  
  for (uint32_t i = 0; i < Renderer.swapchain.image_count; i++) {
    VkImageViewCreateInfo image_view_info = {0};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = Renderer.swapchain.images[i];
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = Renderer.swapchain.format;
    image_view_info.subresourceRange =
        (VkImageSubresourceRange){.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel = 0,
                                  .levelCount = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount = 1};
    VkResultCheck(vkCreateImageView(Renderer.vulkan.device, &image_view_info, NULL, &Renderer.swapchain.image_views[i]));
  }
  SDL_Log(APP_OK"Swapchain imageviews created");

  SDL_Log(APP_WAIT "Creating swapchain semaphores");
  Renderer.swapchain.semaphore_count = Renderer.swapchain.image_count;
  Renderer.swapchain.semaphores =
      malloc(Renderer.swapchain.semaphore_count * sizeof(VkSemaphore));
  
  for (uint32_t i = 0; i < Renderer.swapchain.semaphore_count; i++) {
    VkSemaphoreCreateInfo semaphore_info = {0};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkResultCheck(vkCreateSemaphore(Renderer.vulkan.device, &semaphore_info,
                                    NULL, &Renderer.swapchain.semaphores[i]));
  }
  SDL_Log(APP_OK"Swapchain semaphores created");

  // Creating depth buffer
  //------------------------------------------------------------------------------------------------------
  SDL_Log(APP_WAIT "Creating depth buffer");
  VkImageCreateInfo depth_create_info = {0};
  depth_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  depth_create_info.imageType = VK_IMAGE_TYPE_2D;
  depth_create_info.format = Renderer.depth.format;
  depth_create_info.extent = (VkExtent3D){
    .width = Renderer.swapchain.width, .height = Renderer.swapchain.height,
    .depth = 1};
  depth_create_info.mipLevels = 1;
  depth_create_info.arrayLayers = 1;
  depth_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  depth_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  depth_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo alloc_info = {
      .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO};

  VkResultCheck(vmaCreateImage(Renderer.allocator, &depth_create_info,
                               &alloc_info, &Renderer.depth.image, &Renderer.depth.image_allocation, NULL));
  SDL_Log(APP_OK "Depth buffer created");

  SDL_Log(APP_WAIT "Creating depth buffer image view");
  VkImageViewCreateInfo depth_image_view_info = {0};
  depth_image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  depth_image_view_info.image = Renderer.depth.image;
  depth_image_view_info.format = Renderer.depth.format;
  depth_image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depth_image_view_info.subresourceRange =
      (VkImageSubresourceRange){.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                .levelCount = 1,
                                .layerCount = 1};
  VkResultCheck(vkCreateImageView(Renderer.vulkan.device,
                                  &depth_image_view_info, NULL,
                                  &Renderer.depth.image_view));
  SDL_Log(APP_OK"Depth buffer image view created");
  return SDL_APP_CONTINUE;
}

void APP_SwapchainDestroy() {
  SDL_Log(APP_WAIT "Destroying swapchain");
  for (uint32_t i = 0; i < Renderer.swapchain.image_view_count; i++) {
    vkDestroyImageView(Renderer.vulkan.device, Renderer.swapchain.image_views[i], NULL);
  }

  for (uint32_t i = 0; i < Renderer.swapchain.semaphore_count; i++) {
    vkDestroySemaphore(Renderer.vulkan.device, Renderer.swapchain.semaphores[i], NULL);
  }

  if (Renderer.swapchain.vk_swapchain) {
    vkDestroySwapchainKHR(Renderer.vulkan.device,
                          Renderer.swapchain.vk_swapchain, NULL);
    Renderer.swapchain.vk_swapchain = NULL;
  }

  if (Renderer.depth.image_view) {
    vkDestroyImageView(Renderer.vulkan.device, Renderer.depth.image_view, NULL);
    vmaDestroyImage(Renderer.allocator, Renderer.depth.image,
                    Renderer.depth.image_allocation);
    Renderer.depth.image_view = NULL;
  }
  SDL_Log(APP_OK"Swapchain destroyed");
}



// SHADER CREATION AND COMPILATION

SDL_AppResult APP_ShaderModuleCreate(const char *file_name,
                                     shaderc_shader_kind kind,
                                     const char *entry_point,
                                     VkShaderModule *shader_module) {
  *shader_module = NULL;
  
  size_t file_path_size = strlen(APP_SHADER_PATH) + strlen(file_name) + 1;
  char *file_path = malloc(sizeof(char) * file_path_size);
  strcpy(file_path, APP_SHADER_PATH);
  strcpy(&file_path[strlen(APP_SHADER_PATH)], file_name);

  size_t src_size = 0;
  char *src = SDL_LoadFile(file_path, &src_size);

  if (!src) {
    SDL_Log("%s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_Log(APP_WAIT"Compiling shader %s", file_path);
  shaderc_compiler_t compiler = shaderc_compiler_initialize();
  shaderc_compile_options_t options = shaderc_compile_options_initialize();
  shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
  shaderc_compile_options_set_target_spirv(options, shaderc_spirv_version_1_6);
  shaderc_compile_options_set_optimization_level(
      options, shaderc_optimization_level_performance);
  shaderc_compilation_result_t result = shaderc_compile_into_spv(
      compiler, src, src_size, kind, file_name, entry_point, options);

  if (shaderc_result_get_compilation_status(result) !=
      shaderc_compilation_status_success) {
    SDL_Log(APP_ERROR "Shader compilation Error: %s",
            shaderc_result_get_error_message(result));
    return SDL_APP_FAILURE;
  }    
  SDL_Log(APP_OK"Shader compiled");
  SDL_Log(APP_WAIT"Creating shader module");

  const size_t shader_size = shaderc_result_get_length(result);

  VkShaderModuleCreateInfo module_create_info = {0};
  module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  module_create_info.codeSize = shader_size;
  module_create_info.pCode = (uint32_t *)shaderc_result_get_bytes(result);
  
  VkResultCheck(vkCreateShaderModule(Renderer.vulkan.device, &module_create_info, NULL, shader_module));

  SDL_Log(APP_OK"Shader module created");
  SDL_free(src);
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_ShadersCreate() {
  if (APP_ShaderModuleCreate("shader.vs", shaderc_vertex_shader, "main",
                             &Renderer.vulkan.shader.vertex) == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }
  
  if (APP_ShaderModuleCreate("shader.fs", shaderc_fragment_shader, "main",
                             &Renderer.vulkan.shader.fragment) == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }
  
  return SDL_APP_CONTINUE;
}

void APP_ShadersDestroy() {
  SDL_Log(APP_WAIT"Destroying shaders");
  vkDestroyShaderModule(Renderer.vulkan.device, Renderer.vulkan.shader.vertex,
                        NULL);
  vkDestroyShaderModule(Renderer.vulkan.device, Renderer.vulkan.shader.fragment,
                        NULL);
  SDL_Log(APP_OK"Shaders destroyed");  
}


SDL_AppResult APP_PipelineCreate(VkPipeline *pipeline) {
  VkPipelineLayoutCreateInfo layout_info = {0};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_info.setLayoutCount = 0;
  layout_info.pushConstantRangeCount = 0;

  SDL_Log(APP_WAIT "Creating pipeline layout");
  VkResultCheck(vkCreatePipelineLayout(Renderer.vulkan.device, &layout_info,
                                       NULL, &Renderer.vulkan.pipelineLayout));
  SDL_Log(APP_OK "Pipeline layout created");
  SDL_Log(APP_WAIT "Creating pipeline");

  const char *entrypoint = "main";
  VkPipelineShaderStageCreateInfo shader_stages[] = {
      (VkPipelineShaderStageCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = Renderer.vulkan.shader.vertex,
          .pName = entrypoint},
      (VkPipelineShaderStageCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = Renderer.vulkan.shader.fragment,
          .pName = entrypoint}};

  // Primitive topology triangles
  VkPipelineVertexInputStateCreateInfo vert_input_info = {0};
  vert_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {0};
  input_assembly_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // Depth testing
  VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {0};
  depth_stencil_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_info.depthTestEnable = VK_TRUE;
  depth_stencil_info.depthWriteEnable = VK_TRUE;
  depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil_info.stencilTestEnable = VK_FALSE;

  // Viewport
  VkPipelineViewportStateCreateInfo viewport_info = {0};
  viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_info.viewportCount = 1;
  viewport_info.pViewports = NULL;
  viewport_info.scissorCount = 1;
  viewport_info.pScissors = NULL;

  // Rasterization
  VkPipelineRasterizationStateCreateInfo raster_info = {0};
  raster_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster_info.polygonMode = VK_POLYGON_MODE_FILL;
  raster_info.cullMode = VK_CULL_MODE_BACK_BIT;
  raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  raster_info.lineWidth = 1.0f;

  // Multisampling
  VkPipelineMultisampleStateCreateInfo multisampling_info = {0};
  multisampling_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // Alpha blending
  VkPipelineColorBlendAttachmentState attachment_state = {0};
  attachment_state.blendEnable = VK_FALSE;
  attachment_state.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo blend_info = {0};
  blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend_info.attachmentCount = 1;
  blend_info.pAttachments = &attachment_state;


  // Enable dynamic state
  VkDynamicState dynamic_state[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo dynamic_state_info = {0};
  dynamic_state_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state_info.dynamicStateCount = ARRLEN(dynamic_state);
  dynamic_state_info.pDynamicStates = dynamic_state;


  // Dynamic rendering
  VkPipelineRenderingCreateInfo render_info = {0};
  render_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  render_info.colorAttachmentCount = 1;
  render_info.pColorAttachmentFormats = &Renderer.swapchain.format;
  render_info.depthAttachmentFormat = Renderer.depth.format;

  // Graphics pipeline
  VkGraphicsPipelineCreateInfo pipeline_info = {0};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.pNext = &render_info; // for dynamic rendering
  pipeline_info.stageCount = ARRLEN(shader_stages);
  pipeline_info.pStages = shader_stages; // Shader stages with VkShader's
  pipeline_info.pVertexInputState = &vert_input_info; // blank vertex input state
  pipeline_info.pInputAssemblyState = &input_assembly_info; // triangle list topology
  pipeline_info.pViewportState = &viewport_info; // single viewport and scissor test, but dynamicaly
  pipeline_info.pRasterizationState = &raster_info; // culling backfaces, front direction CCW
  pipeline_info.pMultisampleState = &multisampling_info; // no multisampling
  pipeline_info.pDepthStencilState = &depth_stencil_info; // depth testing and writing on
  pipeline_info.pColorBlendState = &blend_info; // one color attachment
  pipeline_info.pDynamicState = &dynamic_state_info; // viewport and scissor states are dynamic
  pipeline_info.layout = Renderer.vulkan.pipelineLayout; // pipeline doesn't expect any descriptors
  pipeline_info.renderPass = VK_NULL_HANDLE; // not using render passes, dynamic rendering

  *pipeline = (VkPipeline){0};
  VkResultCheck(vkCreateGraphicsPipelines(Renderer.vulkan.device, NULL, 1,
                                          &pipeline_info, NULL, pipeline));

  SDL_Log(APP_OK "Pipeline created");  
  return SDL_APP_CONTINUE;
}

void APP_PipelineDestroy(VkPipeline *pipeline) {
  vkDestroyPipeline(Renderer.vulkan.device, *pipeline, NULL);
  vkDestroyPipelineLayout(Renderer.vulkan.device, Renderer.vulkan.pipelineLayout, NULL);
}

SDL_AppResult APP_SyncResourcesCreate() {

  // Timeline semaphore needed for sync between cpu and gpu
  VkSemaphoreTypeCreateInfo semaphore_type_info = {0};
  semaphore_type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  semaphore_type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  semaphore_type_info.initialValue = APP_MAX_FRAMES_IN_FLIGHT;

  VkSemaphoreCreateInfo semaphore_info = {0};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_info.pNext = &semaphore_type_info;

  SDL_Log(APP_WAIT"Creating timeline semaphore");
  VkResultCheck(vkCreateSemaphore(Renderer.vulkan.device, &semaphore_info, NULL,
                                  &Renderer.vulkan.timelineSemapthore));
  SDL_Log(APP_OK"Timeline semaphore created");


  SDL_Log(APP_WAIT"Creating frame semaphores");
  for (uint32_t i = 0; i < APP_MAX_FRAMES_IN_FLIGHT; i++) {
    VkSemaphoreCreateInfo semaphore_info = {0};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkResultCheck(vkCreateSemaphore(
        Renderer.vulkan.device, &semaphore_info, NULL,
        &Renderer.vulkan.framesInFlight[i].imageAcquiredSemaphore));
  }
  SDL_Log(APP_OK"Frame semaphores created");
  
  return SDL_APP_CONTINUE;
}

void APP_SyncResourcesDestroy() {
  SDL_Log(APP_WAIT"Destroying frame semaphores");
  for (uint32_t i = 0; i < APP_MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(Renderer.vulkan.device,
                       Renderer.vulkan.framesInFlight[i].imageAcquiredSemaphore,
                       NULL);
  }
  SDL_Log(APP_OK "Frame semaphores destroyed");
  SDL_Log(APP_WAIT"Destroying timeline semaphore");  
  vkDestroySemaphore(Renderer.vulkan.device, Renderer.vulkan.timelineSemapthore, NULL);
  SDL_Log(APP_OK"Timeline semaphore destroyed");
}


SDL_AppResult APP_CommandBuffersCreate() {
  SDL_Log(APP_WAIT"Creating frames in flight command buffers");
  for (uint32_t i = 0; i < APP_MAX_FRAMES_IN_FLIGHT; i++) {
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = Renderer.graphicsQueueFamilyId;
    VkResultCheck(
        vkCreateCommandPool(Renderer.vulkan.device, &pool_info, NULL,
                            &Renderer.vulkan.framesInFlight[i].commandPool));
    

    VkCommandBufferAllocateInfo command_alloc_info = {0};
    command_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_alloc_info.commandPool =
        Renderer.vulkan.framesInFlight[i].commandPool;
    command_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_alloc_info.commandBufferCount = 1;

    VkResultCheck(vkAllocateCommandBuffers(
        Renderer.vulkan.device, &command_alloc_info,
        &Renderer.vulkan.framesInFlight[i].commandBuffer));
  }
  SDL_Log(APP_OK "Frames in flight command buffer created");

  return SDL_APP_CONTINUE;
}


void APP_CommandBuffersDestroy() {
  for (uint32_t i = 0; i < APP_MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroyCommandPool(Renderer.vulkan.device, Renderer.vulkan.framesInFlight[i].commandPool, NULL);
  }    
}  
