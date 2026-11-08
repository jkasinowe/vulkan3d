#include "app_internall.h"

VKAPI_ATTR VkBool32 VKAPI_CALL LOG_DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
							void *pUserData) {
  SDL_Log("%s", pCallbackData->pMessage);
  return VK_FALSE;
}
