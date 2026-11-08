#include "app_internall.h"

SDL_AppResult APP_GraphicsInit() {
// Initializing rendering data
  if (APP_RendererInit() != SDL_APP_CONTINUE) {
    return SDL_APP_FAILURE;
  }

  // Initializing SDL window
  if (APP_SDLInit() != SDL_APP_CONTINUE) {
    return SDL_APP_FAILURE;
  }
  
  // Initializing vulkan via volk
  if (APP_VulkanVolkBind() != SDL_APP_CONTINUE) {
    return SDL_APP_FAILURE;
  }

  //------------------------------------------------------------------------------
  // Creating VkInstance
  if (APP_VulkanInstanceCreate() != SDL_APP_CONTINUE) {
    return SDL_APP_FAILURE;
  }
  
  // Create surface
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanSurfaceCreate() != SDL_APP_CONTINUE) {
    return SDL_APP_FAILURE;
  }

  // Find physical device
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanPhysicalDeviceFind() != SDL_APP_CONTINUE) {
    return SDL_APP_FAILURE;
  }

  // Find graphics queue
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanGraphicsQueueFind() != SDL_APP_CONTINUE) {
    return SDL_APP_FAILURE;
  }

  // Creating device
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanDeviceCreate() != SDL_APP_CONTINUE) {
    return SDL_APP_FAILURE;
  }

  // Initializing VMA
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanVMACreate() != SDL_APP_CONTINUE) {
    return SDL_APP_FAILURE;
  }

  // Creating swapchain
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanSwapchainCreate() == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }

  // Creating shaders
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanShadersCreate() == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }

  // Creating pipeline
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanPipelineCreate(&Renderer.vulkan.pipeline) == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }

  // Creating semaphores
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanSyncResourcesCreate() == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }

  // Creating frame in flight command buffers;
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanCommandBuffersCreate() == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }    
  
  return SDL_APP_CONTINUE;
}

void APP_GraphicsFinalize() {
  vkDeviceWaitIdle(Renderer.vulkan.device);

  APP_VulkanCommandBuffersDestroy();

  APP_VulkanSyncResourcesDestroy();

  APP_VulkanPipelineDestroy(&Renderer.vulkan.pipeline);

  APP_VulkanShadersDestroy();

  APP_VulkanSwapchainDestroy();
  
  APP_VulkanVMADestroy();

  APP_VulkanDeviceDestroy();

  APP_VulkanSurfaceDestroy();
  
  APP_VulkanVolkUnbind();
}
