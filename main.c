#define SDL_MAIN_USE_CALLBACKS 1
#define VK_NO_PROTOTYPES
#include <volk.h>

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

#define ARRLEN(x) (sizeof(x) / sizeof(*x))

#define APP_COLOR_FORMAT VK_FORMAT_B8G8R8A8_SRGB

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
  uint32_t semaphores_count;
  VkSemaphore *semaphores;
};

#define APP_WAIT "WAIT:  "
#define APP_OK "OK:    "
#define APP_ERROR "ERROR: "

struct Vulkan {
  uint32_t version;
  uint32_t instanceExtensionCount;
  const char **instanceExtensionNames;
  VkInstance instance;
  VkSurfaceKHR surface;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
};

struct Renderer {
  struct Window window;
  struct Vulkan vulkan;
  uint32_t graphicsQueueFamilyId;
  VkQueue graphicsQueue;
  VmaAllocator allocator;
  struct Swapchain swapchain;
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

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  SDL_ResultCheck(SDL_Init(SDL_INIT_VIDEO));
  Renderer.window.width = 800;
  Renderer.window.height = 600;
  Renderer.swapchain.format = APP_COLOR_FORMAT;

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
  Renderer.swapchain.width = Renderer.window.width;
  Renderer.swapchain.height = Renderer.window.height;

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

  Renderer.swapchain.image_view_count = Renderer.swapchain.image_count;
  

  
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) { return SDL_APP_CONTINUE; }

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
  if (Renderer.swapchain.vk_swapchain) {
    vkDestroySwapchainKHR(Renderer.vulkan.device,
                          Renderer.swapchain.vk_swapchain, NULL);
  }

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
