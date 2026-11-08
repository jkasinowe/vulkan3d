#define SDL_MAIN_USE_CALLBACKS 1
#define VK_NO_PROTOTYPES
#include <volk.h>
#include <shaderc/shaderc.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

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

extern struct Renderer Renderer;

VKAPI_ATTR VkBool32 VKAPI_CALL LOG_DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

#define VkResultLog(result)                                                    \
  {                                                                            \
    const char *str = string_VkResult(result);                                 \
    SDL_Log("Vulkan error: %s\n", str);                                        \
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


SDL_AppResult APP_GraphicsInit();
void APP_GraphicsFinalize();
SDL_AppResult APP_RendererInit();
SDL_AppResult APP_SDLInit();
SDL_AppResult APP_VulkanVolkBind();
void APP_VulkanVolkUnbind();
SDL_AppResult APP_VulkanInstanceCreate();
void APP_VulkanInstanceDestroy();
SDL_AppResult APP_VulkanSurfaceCreate();
void APP_VulkanSurfaceDestroy();
SDL_AppResult APP_VulkanPhysicalDeviceFind();
SDL_AppResult APP_VulkanGraphicsQueueFind();
SDL_AppResult APP_VulkanDeviceCreate();
void APP_VulkanDeviceDestroy();
SDL_AppResult APP_VulkanVMACreate();
void APP_VulkanVMADestroy();
SDL_AppResult APP_VulkanSwapchainCreate();
void APP_VulkanSwapchainDestroy();
SDL_AppResult APP_VulkanShadersCreate();
void APP_VulkanShadersDestroy();
SDL_AppResult APP_VulkanPipelineCreate(VkPipeline* pipeline);
void APP_VulkanPipelineDestroy(VkPipeline *pipeline);
SDL_AppResult APP_VulkanSyncResourcesCreate();
void APP_VulkanSyncResourcesDestroy();
SDL_AppResult APP_VulkanCommandBuffersCreate();
void APP_VulkanCommandBuffersDestroy();
