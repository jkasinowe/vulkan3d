#define VLOG_LOG
#include <vlog.h>
#include <vmath.h>
#include <vmesh.h>
#include <vsys.h>
#define VK_NO_PROTOTYPES
#include <shaderc/shaderc.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "app_game.h"

#define ARRLEN(x) (sizeof(x) / sizeof(*x))

#define APP_COLOR_FORMAT VK_FORMAT_B8G8R8A8_SRGB
#define APP_DEPTH_FORMAT VK_FORMAT_D32_SFLOAT
#define APP_SHADER_PATH "src/shaders/"
#define APP_MAX_FRAMES_IN_FLIGHT 3
#define APP_VSYNC_MODE 0
#define APP_MOUSE_HIDE 1
#define APP_PLAYER_SPEED 20.0


struct Window {
  struct VSYS_Window *vsysWindow;
  uint32_t width; // main size
  uint32_t height;
};

struct Swapchain {
  uint32_t width;
  uint32_t height;
  VkFormat format;

  VkSwapchainKHR vkSwapchain;
  uint32_t imageIndex;

  uint32_t imageCount;
  VkImage *images;
  VkImage *imageCurrent;

  uint32_t imageViewCount;
  VkImageView *imageViews;
  VkImageView *imageViewCurrent;

  uint32_t semaphoreCount;
  VkSemaphore *semaphores;
  VkSemaphore *semaphoreCurrent;

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

struct UniformBufferObject {
  float model[16];
  float view[16];
  float projection[16];  
  float screenSize[2];
  float time;
  float light_color[3];
  float light_position[3];
};


struct Buffers {
  VkBuffer *vkBuffers;
  VmaAllocation *allocation;
  VmaAllocationInfo *allocationInfo;
};

struct Uniform {
  struct UniformBufferObject bufferObject;
  struct Buffers buffers;
  VkDescriptorSetLayout descriptorLayout;
  VkDescriptorPool descriptorPool;
  VkDescriptorSet descriptorSets[APP_MAX_FRAMES_IN_FLIGHT];  
};

#define MODEL_NAME_LENGTH 50
struct Model {
  char name[MODEL_NAME_LENGTH+1];
  u32 vertexCount;
  u32 indexCount;
  u32 vertexOffset;
  u32 indexOffset;
};

struct Vertices {
  struct VMESH_Vertex *vertices;
  uint32_t count;
};

struct Indices {
  uint32_t *indices;
  uint32_t count;
};

struct InstanceData {
  float modelMatrix[16];
};

struct Instances {
  struct InstanceData* instances;
  uint32_t count;
};  

struct VertexBuffer {
  VkBuffer vkBuffer;
  struct Vertices vertices;
  VmaAllocation allocation;
  VkDeviceAddress address;
  VmaAllocationInfo allocationInfo;
};

struct IndexBuffer {
  VkBuffer vkBuffer;
  struct Indices indices;
  VmaAllocation allocation;
  VkDeviceAddress address;
  VmaAllocationInfo allocationInfo;
};

struct InstanceBuffer {
  VkBuffer vkBuffer;
  struct Instances instances;
  VmaAllocation allocation;
  VkDeviceAddress address;
  VmaAllocationInfo allocationInfo;
};

struct Models {
  struct Model *models;
  u32 count;
};  

struct ModelData {
  struct VertexBuffer vertexBuffer;
  struct IndexBuffer indexBuffer;
  struct InstanceBuffer instanceBuffer;
  struct Models models;
};


struct PushConstantData {
  VkDeviceAddress indexAddress;
  VkDeviceAddress vertexAddress;
  VkDeviceAddress instanceAddress;
};  

struct Pipeline {
  VkPipelineLayout layout;
  VkPipeline vkPipeline;
  struct Uniform uniform;
  struct Shader shader;
  struct PushConstantData pushConstants;
};

struct Vulkan {
  uint32_t version;
  uint32_t instanceExtensionCount;
  const char **instanceExtensionNames;
  VkInstance instance;
  VkSurfaceKHR surface;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  struct Pipeline pipeline;
  VkSemaphore timelineSemaphore;
  struct FrameResources framesInFlight[APP_MAX_FRAMES_IN_FLIGHT];
  uint64_t nextSignalValue;
  struct FrameResources *frameCurrent;
  uint32_t frameIndex;
  struct ModelData modelData;
};

struct Depth {
  VkFormat format;
  VkImage image;
  VmaAllocation imageAllocation;
  VkImageView imageView;
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
extern struct Player Player;
extern struct Sphere Sphere;
extern struct Light Light;


VKAPI_ATTR VkBool32 VKAPI_CALL LOG_DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

#define VkResultLog(result)                                                    \
  {                                                                            \
    const char *str = string_VkResult(result);                                 \
    VLOG_Error("Vulkan error: %s\n", str);                                        \
  }

#define VkResultCheck(x)                                                       \
  do {                                                                         \
    VkResult result = x;                                                       \
    if (result != VK_SUCCESS) {                                                \
      VLOG_Error("Vulkan error:");                                             \
      VkResultLog(result);                                                     \
      return true;                                                  \
    }                                                                          \
  } while (0)

#define VSYS_ResultCheck(x)                                                    \
  if (!x) {                                                                    \
    VLOG_Error("%s", VSYS_ErrorGet());                                         \
    return true;                                                               \
  }

bool APP_GraphicsInit();
void APP_GraphicsFinalize();
bool APP_RendererInit();
bool APP_VSYSInit();
bool APP_VulkanVolkBind();
void APP_VulkanVolkUnbind();
bool APP_VulkanInstanceCreate();
void APP_VulkanInstanceDestroy();
bool APP_VulkanSurfaceCreate();
void APP_VulkanSurfaceDestroy();
bool APP_VulkanPhysicalDeviceFind();
bool APP_VulkanGraphicsQueueFind();
bool APP_VulkanDeviceCreate();
void APP_VulkanDeviceDestroy();
bool APP_VulkanVMACreate();
void APP_VulkanVMADestroy();
bool APP_VulkanSwapchainCreate();
void APP_VulkanSwapchainDestroy();
bool APP_VulkanShadersCreate();
void APP_VulkanShadersDestroy();
bool APP_VulkanUniformBuffersCreate(struct Uniform *uniform);
bool APP_VulkanVerticesInitialize();
bool APP_VulkanDescriptorSetLayoutCreate();
bool APP_VulkanDescriptorPoolCreate();
bool APP_VulkanDescriptorSetsCreate();
bool APP_VulkanPipelineCreate(struct Pipeline *pipeline);
void APP_VulkanPipelineDestroy(struct Pipeline *pipeline);
bool APP_VulkanSyncResourcesCreate();
void APP_VulkanSyncResourcesDestroy();
bool APP_VulkanCommandBuffersCreate();
void APP_VulkanCommandBuffersDestroy();
bool APP_RendererWindowSizeUpdate();
bool APP_VulkanUniformBufferObjectUpdate();
bool APP_VulkanBDACreate(u32 size, VkBuffer *buffer,
                                  VmaAllocation *allocation,
                                  VmaAllocationInfo *allocation_info,
                                  VkDeviceAddress *address);
bool APP_VulkanBDAsCreate();
bool APP_VulkanBDAsUpdate();
bool APP_VulkanModelDraw(struct Model *model);
