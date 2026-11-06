#include <volk.h>
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <stdlib.h>

#include <vulkan/vk_enum_string_helper.h>

#define ARRLEN(x) (sizeof(x) / sizeof(*x))

SDL_Window *window;
uint32_t VulkanVersion = VK_API_VERSION_1_4;

uint32_t InstanceExtensionCount = 0;
const char **InstanceExtensionNames;
uint32_t InstanceExtensionDefaultCount = 0;
const char *const *InstanceExtensionDefaultNames;
uint32_t InstanceExtensionCustomCount = 0;
char *InstanceExtensionCustomNames[] = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
};

VkInstance VulkanInstance;

VkBool32
LOG_DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
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
  window = SDL_CreateWindow("Vulkan + SDL3", 800, 600, SDL_WINDOW_VULKAN);
  SDL_ResultCheck(window);

  // Initializing vulkan
  VkResult res = volkInitialize();
  if (res != VK_SUCCESS) {
    SDL_Log("Failed to initialize volk with error %d\n", res);
    return SDL_APP_FAILURE;
  }

  VkApplicationInfo app_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app_info.pApplicationName = "Vulkan + SDL3";
  app_info.apiVersion = VulkanVersion;

  InstanceExtensionDefaultNames =
      SDL_Vulkan_GetInstanceExtensions(&InstanceExtensionDefaultCount);
  InstanceExtensionCustomCount = ARRLEN(InstanceExtensionCustomNames);
  InstanceExtensionCount =
      InstanceExtensionDefaultCount + InstanceExtensionCustomCount;

  
  InstanceExtensionNames = malloc(InstanceExtensionCount * sizeof(char *));
  memcpy(InstanceExtensionNames, InstanceExtensionDefaultNames,
         InstanceExtensionDefaultCount);
  memcpy(InstanceExtensionNames + InstanceExtensionDefaultCount,
         InstanceExtensionCustomNames, InstanceExtensionCustomCount);

  const char *InstanceRequestedLayers[] = {
      "VK_LAYER_KHRONOS_validation",
  };

  VkDebugUtilsMessengerCreateInfoEXT debug_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
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
  instance_info.enabledExtensionCount = InstanceExtensionCount;
  instance_info.ppEnabledExtensionNames = InstanceExtensionNames;
  instance_info.enabledLayerCount = ARRLEN(InstanceRequestedLayers);
  instance_info.ppEnabledLayerNames = InstanceRequestedLayers;

  VkResultCheck(vkCreateInstance(&instance_info, NULL, &VulkanInstance));

  volkLoadInstance(VulkanInstance);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {}


