#include "app_internall.h"

bool APP_GraphicsInit() {
  VSYS_Initialize();
  // Initializing rendering data
  if (APP_RendererInit() != false) {
    return true;
  }

  // Initializing SDL window
  if (APP_VSYSInit() != false) {    
    return true;
  }

  // Initializing vulkan via volk
  if (APP_VulkanVolkBind() != false) {
    return true;
  }

  //------------------------------------------------------------------------------
  // Creating VkInstance
  if (APP_VulkanInstanceCreate() != false) {
    return true;
  }

  // Create surface
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanSurfaceCreate() != false) {
    return true;
  }

  // Find physical device
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanPhysicalDeviceFind() != false) {
    return true;
  }

  // Find graphics queue
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanGraphicsQueueFind() != false) {
    return true;
  }

  // Creating device
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanDeviceCreate() != false) {
    return true;
  }

  // Initializing VMA
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanVMACreate() != false) {
    return true;
  }

  // Creating swapchain
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanSwapchainCreate() == true) {
    return true;
  }

  // Creating shaders
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanShadersCreate() == true) {
    return true;
  }

  // Creating uniform buffer object
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanUniformBuffersCreate(&Renderer.vulkan.pipeline.uniform)) {
    return true;
  }

  if (APP_VulkanVerticesInitialize()) {
    return true;
  }

  if (APP_VulkanBDAsCreate()) {
    return true;
  }

  if (APP_VulkanBDAsUpdate()) {
    return true;
  }

  // Creating descriptor set layout
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanDescriptorSetLayoutCreate()) {
    return true;
  }

  if (APP_VulkanDescriptorPoolCreate()) {
    return true;
  }

  if (APP_VulkanDescriptorSetsCreate()) {
    return true;
  }

  // Creating pipeline
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanPipelineCreate(&Renderer.vulkan.pipeline) == true) {
    return true;
  }

  // Creating semaphores
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanSyncResourcesCreate() == true) {
    return true;
  }

  // Creating frame in flight command buffers;
  //------------------------------------------------------------------------------------------------------
  if (APP_VulkanCommandBuffersCreate() == true) {
    return true;
  }

  VLOG_Wait("Loading mesh data");
  if (APP_VulkanBDAsUpdate()) {
    return true;
  }

  return false;
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

  VSYS_Finalize();
}
