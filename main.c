#define SDL_MAIN_USE_CALLBACKS 1
#define VK_NO_PROTOTYPES

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <volk.h>
#include <vulkan/vulkan.h>
#include <SDL3/SDL_init.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

#define ARRLEN(x) (sizeof(x) / sizeof(*x))

struct Window {
  SDL_Window *sdl_window;
  uint32_t width;
  uint32_t height;
};

#define APP_WAIT  "WAIT:  "
#define APP_OK    "OK:    "
#define APP_ERROR "ERROR: "

struct Renderer {
  struct Window window;
  uint32_t vulkanVersion;
  uint32_t instanceExtensionCount;
  const char **instanceExtensionNames;
  VkInstance vulkanInstance;
  VkSurfaceKHR vulkanSurface;
  VkPhysicalDevice physicalDevice;
  uint32_t graphicsQueueFamilyId;
  VkQueue graphicsQueue;
  VkDevice device;
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

  vkEnumerateInstanceVersion(&Renderer.vulkanVersion);
  SDL_Log(APP_OK"Vulkan API version: %d.%d.%d",
          VK_API_VERSION_MAJOR(Renderer.vulkanVersion),
          VK_API_VERSION_MINOR(Renderer.vulkanVersion),
          VK_API_VERSION_PATCH(Renderer.vulkanVersion));

  //------------------------------------------------------------------------------
  // Creating VkInstance
  
  VkApplicationInfo app_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app_info.pNext = NULL;
  app_info.pApplicationName = "Vulkan + SDL3";
  app_info.apiVersion = Renderer.vulkanVersion;
  app_info.pEngineName = "Vector";

  char *instance_extension_custom_names[] = {
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
  };

  uint32_t instance_extension_default_count;
  const char *const *instance_extension_default_names =
      SDL_Vulkan_GetInstanceExtensions(&instance_extension_default_count);
  uint32_t instance_extension_custom_count =
      ARRLEN(instance_extension_custom_names);
  Renderer.instanceExtensionCount =
      instance_extension_default_count + instance_extension_custom_count;

  Renderer.instanceExtensionNames =
      malloc(Renderer.instanceExtensionCount * sizeof(char *));
  memcpy(&Renderer.instanceExtensionNames[0], instance_extension_default_names,
         instance_extension_default_count * sizeof(char *));

  memcpy(&Renderer.instanceExtensionNames[instance_extension_default_count],
         instance_extension_custom_names,
         instance_extension_custom_count * sizeof(char *));

  const char *InstanceRequestedLayers[] = {
      "VK_LAYER_KHRONOS_validation",
  };

  SDL_Log("%d extensions:", Renderer.instanceExtensionCount);
  for (uint32_t i = 0; i < Renderer.instanceExtensionCount; i++) {
    SDL_Log("%s", Renderer.instanceExtensionNames[i]);
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
  instance_info.enabledExtensionCount = Renderer.instanceExtensionCount;
  instance_info.ppEnabledExtensionNames = Renderer.instanceExtensionNames;
  instance_info.enabledLayerCount = ARRLEN(InstanceRequestedLayers);
  instance_info.ppEnabledLayerNames = InstanceRequestedLayers;

  SDL_Log(APP_WAIT"Creating a vulkan instance");
  VkResultCheck(
      vkCreateInstance(&instance_info, NULL, &Renderer.vulkanInstance));
  SDL_Log(APP_OK"Vulkan instance created");

  SDL_Log(APP_WAIT"Loading vulkan instance");
  volkLoadInstance(Renderer.vulkanInstance);
  SDL_Log(APP_OK"Vulkan instance loaded");
  
  // Create surface
  //------------------------------------------------------------------------------------------------------
  SDL_Log(APP_WAIT"Creating vulkan surface");
  SDL_ResultCheck(SDL_Vulkan_CreateSurface(Renderer.window.sdl_window, Renderer.vulkanInstance, NULL, &Renderer.vulkanSurface));
  SDL_Log(APP_OK"Vulkan surface created");

  // Find physical device
  //------------------------------------------------------------------------------------------------------
  SDL_Log(APP_WAIT"Looking for physical devices");
  
  uint32_t physical_device_count;
  vkEnumeratePhysicalDevices(Renderer.vulkanInstance, &physical_device_count,
                             NULL);
  if (!physical_device_count) {
    SDL_Log(APP_ERROR"no physical devices found");
    return SDL_APP_FAILURE;
  }

  SDL_Log(APP_OK "Found %d physical devices", physical_device_count);
  SDL_Log(APP_WAIT "Choosing the best one");
  VkPhysicalDevice *physical_devices =
      malloc(physical_device_count * sizeof(VkPhysicalDevice));
  vkEnumeratePhysicalDevices(Renderer.vulkanInstance, &physical_device_count,
                             physical_devices);
  Renderer.physicalDevice = physical_devices[0];
  
  for (size_t i = 0; i < physical_device_count; i++) {
    VkPhysicalDeviceProperties props = {0};
    vkGetPhysicalDeviceProperties(physical_devices[i], &props);
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      Renderer.physicalDevice = physical_devices[i];
      SDL_Log(APP_OK"Found discrete GPU");
      break;
    }      
  }

  VkPhysicalDeviceProperties physical_device_properties = {0};
  vkGetPhysicalDeviceProperties(Renderer.physicalDevice, &physical_device_properties);
  SDL_Log(APP_OK"Physical device chosen: %s", physical_device_properties.deviceName);

  // Find graphics queue
  //------------------------------------------------------------------------------------------------------
  SDL_Log(APP_WAIT"Looking for graphics queue");

  uint32_t graphics_queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties2(Renderer.physicalDevice,
                                            &graphics_queue_family_count, NULL);
  SDL_Log(APP_OK"Found %d graphics queue families", graphics_queue_family_count);
  VkQueueFamilyProperties2 *graphics_queue_family_properties_array;
  graphics_queue_family_properties_array =
      calloc(graphics_queue_family_count, sizeof(VkQueueFamilyProperties2));
  for (size_t i = 0; i < graphics_queue_family_count; i++) {
        graphics_queue_family_properties_array[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
  }    
  vkGetPhysicalDeviceQueueFamilyProperties2(Renderer.physicalDevice, &graphics_queue_family_count, graphics_queue_family_properties_array);

  bool found_queue_family_with_present_support = false;
  SDL_Log(APP_WAIT "Choosing one with Present support");
  for (size_t i = 0; i < graphics_queue_family_count; i++) {
    VkQueueFamilyProperties2 *props = &graphics_queue_family_properties_array[i];

    VkBool32 has_present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(Renderer.physicalDevice, i,
                                         Renderer.vulkanSurface,
                                         &has_present_support);

    if (props->queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT &&
        has_present_support) {
      Renderer.graphicsQueueFamilyId = i;
      found_queue_family_with_present_support = true;
      break;
    }      
  }

  if (!found_queue_family_with_present_support) {
    SDL_Log(APP_ERROR"Queue family with Present support not found");
    return SDL_APP_FAILURE;
  }
  SDL_Log(APP_OK"Found graphics queue with Present support");

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
  vkGetPhysicalDeviceFeatures2(Renderer.physicalDevice, &supported_features);

  SDL_Log(APP_WAIT"Checking for available physical device features");
  if (!supported_features_13.dynamicRendering ||
      !supported_features_13.synchronization2 ||
      !supported_features_12.timelineSemaphore) {
    SDL_Log(APP_ERROR "Physical device doesn't meet the feature requirements");
    return SDL_APP_FAILURE;
  }
  SDL_Log(APP_OK"Physical device meets the feature requirements");
  
  VkPhysicalDeviceVulkan14Features requested_features_14 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
      .pNext = NULL};
  VkPhysicalDeviceVulkan13Features requested_features_13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &requested_features_14,
      .synchronization2 = VK_TRUE, // modernized synchronization
      .dynamicRendering = VK_TRUE}; // no renderpass/subpasses

  VkPhysicalDeviceVulkan12Features requested_features_12 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &requested_features_13, // streamlined multi-frame-in-flight handling
      .timelineSemaphore = VK_TRUE};
  
  VkPhysicalDeviceFeatures2 requested_features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .pNext = &requested_features_12};

  float queue_priorities[] = {1.0f};
  VkDeviceQueueCreateInfo queue_create_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = Renderer.graphicsQueueFamilyId,
    .queueCount = ARRLEN(queue_priorities),
    .pQueuePriorities = queue_priorities
  };

  const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkDeviceCreateInfo device_create_info = {0};
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = &requested_features;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledExtensionCount = ARRLEN(device_extensions);
  device_create_info.ppEnabledExtensionNames = device_extensions;
  device_create_info.pEnabledFeatures = NULL;

  SDL_Log(APP_WAIT"Creating device");
  VkResultCheck(vkCreateDevice(Renderer.physicalDevice, &device_create_info, NULL, &Renderer.device));
  SDL_Log(APP_OK "Device created");

  SDL_Log(APP_WAIT"Getting device queue");
  vkGetDeviceQueue(Renderer.device, Renderer.graphicsQueueFamilyId, 0, &Renderer.graphicsQueue);
  SDL_Log(APP_OK "Device queue ready");
  
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
  if (Renderer.device) {
    vkDestroyDevice(Renderer.device, NULL);
  }    
  if (Renderer.vulkanSurface) {
    vkDestroySurfaceKHR(Renderer.vulkanInstance, Renderer.vulkanSurface, NULL);
  }    
  if (Renderer.vulkanInstance) {
    vkDestroyInstance(Renderer.vulkanInstance, NULL);
  }
  volkFinalize();
}
