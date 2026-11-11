#include "app_internall.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>

uint64_t time_last = 0;
double delta_time = 0;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  time_last = SDL_GetPerformanceCounter();
  Player.position[2] = -2;
  if (APP_GraphicsInit() != SDL_APP_CONTINUE) {
    return SDL_APP_FAILURE;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_RendererWindowSizeUpdate() {
  SDL_GetWindowSize(Renderer.window.sdlWindow, &Renderer.window.width,
                    &Renderer.window.height);
  Renderer.swapchain.requireRecreate = true;
  return SDL_APP_CONTINUE;
}

void APP_RendererUpdateFrameCurrent() {
  Renderer.vulkan.frameIndex =
      Renderer.frameIndex++ % APP_MAX_FRAMES_IN_FLIGHT;
  Renderer.vulkan.frameCurrent =
      &Renderer.vulkan.framesInFlight[Renderer.vulkan.frameIndex];
}

void APP_RendererSwapchainUpdateCurrent() {
  Renderer.swapchain.imageCurrent =
      &Renderer.swapchain.images[Renderer.swapchain.imageIndex];
  Renderer.swapchain.imageViewCurrent =
      &Renderer.swapchain.imageViews[Renderer.swapchain.imageIndex];
  Renderer.swapchain.semaphoreCurrent =
      &Renderer.swapchain.semaphores[Renderer.swapchain.imageIndex];
}

void APP_RendererCommandPoolReset() {
  vkResetCommandPool(Renderer.vulkan.device,
                     Renderer.vulkan.frameCurrent->commandPool, 0);
}

void APP_RendererSwapchainNext(VkResult *acquire_image_vulkan_result) {
  const uint64_t wait_value =
      Renderer.vulkan.nextSignalValue++ - APP_MAX_FRAMES_IN_FLIGHT;

  // Wait before getting the next swapchain
  VkSemaphoreWaitInfo wait_info = {0};
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  wait_info.semaphoreCount = 1;
  wait_info.pSemaphores = &Renderer.vulkan.timelineSemaphore;
  wait_info.pValues = &wait_value;

  vkWaitSemaphores(Renderer.vulkan.device, &wait_info, UINT64_MAX);

  APP_RendererUpdateFrameCurrent();

  APP_RendererCommandPoolReset();

  *acquire_image_vulkan_result = vkAcquireNextImageKHR(
      Renderer.vulkan.device, Renderer.swapchain.vkSwapchain, UINT64_MAX,
      Renderer.vulkan.frameCurrent->imageAcquiredSemaphore, VK_NULL_HANDLE,
      &Renderer.swapchain.imageIndex);

  APP_RendererSwapchainUpdateCurrent();
}

void APP_RendererCommandsBegin() {
  VkCommandBufferBeginInfo command_begin_info = {0};
  command_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(Renderer.vulkan.frameCurrent->commandBuffer,
                       &command_begin_info);
}

void APP_RendererBarrierBeforeFragmentShaderCreate() {
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
          .image = *Renderer.swapchain.imageCurrent,
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
          }}};

  // finalize layout barriers
  VkDependencyInfo dep_info = {0};
  dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep_info.imageMemoryBarrierCount = ARRLEN(layout_barriers);
  dep_info.pImageMemoryBarriers = layout_barriers;

  vkCmdPipelineBarrier2(Renderer.vulkan.frameCurrent->commandBuffer, &dep_info);
}

void APP_RendererDynamicRenderingBegin() {
  VkRenderingAttachmentInfo color_attach_info = {0};
  color_attach_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attach_info.imageView = *Renderer.swapchain.imageViewCurrent;
  color_attach_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attach_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attach_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attach_info.clearValue = (VkClearValue){0.1f, 0.1f, 0.1f, 1};
  color_attach_info.pNext = NULL;

  VkRenderingAttachmentInfo depth_attach_info = {0};
  depth_attach_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depth_attach_info.imageView = Renderer.depth.imageView;
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
  vkCmdBeginRendering(Renderer.vulkan.frameCurrent->commandBuffer,
                      &rendering_info);
}

void APP_RendererDynamicRenderingEnd() {
  vkCmdEndRendering(Renderer.vulkan.frameCurrent->commandBuffer);
}

void APP_RendererBarrierAfterFragmentShaderCreate() {
  VkImageMemoryBarrier2 present_layout_barrier = {0};
  present_layout_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  present_layout_barrier.srcStageMask =
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  present_layout_barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  present_layout_barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  present_layout_barrier.dstAccessMask = 0;
  present_layout_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  present_layout_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  present_layout_barrier.image = *Renderer.swapchain.imageCurrent;
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
  vkCmdPipelineBarrier2(Renderer.vulkan.frameCurrent->commandBuffer,
                        &present_dep_info);
}

void APP_RendererCommandsEnd() {
  vkEndCommandBuffer(Renderer.vulkan.frameCurrent->commandBuffer);
}

void APP_RendererCommandsSubmit() {
  VkSemaphoreSubmitInfo image_acquire_wait_info = {0};
  image_acquire_wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  image_acquire_wait_info.semaphore =
      Renderer.vulkan.frameCurrent->imageAcquiredSemaphore;
  image_acquire_wait_info.stageMask =
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

  // Binary semaphores for each flight and timeline for gpu
  VkSemaphoreSubmitInfo semaphore_signals[] = {
      (VkSemaphoreSubmitInfo){
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
          .semaphore = *Renderer.swapchain.semaphoreCurrent,
          .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
      },
      (VkSemaphoreSubmitInfo){.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                              .semaphore = Renderer.vulkan.timelineSemaphore,
                              .value = Renderer.vulkan.nextSignalValue - 1,
                              .stageMask =
                                  VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT}};

  VkCommandBufferSubmitInfo command_submit_info = {0};
  command_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  command_submit_info.commandBuffer =
      Renderer.vulkan.frameCurrent->commandBuffer;

  VkSubmitInfo2 submit_info = {0};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit_info.waitSemaphoreInfoCount = 1;
  submit_info.pWaitSemaphoreInfos = &image_acquire_wait_info;
  submit_info.commandBufferInfoCount = 1;
  submit_info.pCommandBufferInfos = &command_submit_info;
  submit_info.signalSemaphoreInfoCount = ARRLEN(semaphore_signals);
  submit_info.pSignalSemaphoreInfos = semaphore_signals;

  vkQueueSubmit2(Renderer.graphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
}

void APP_RendererPresent() {
  VkPresentInfoKHR present_info = {0};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = Renderer.swapchain.semaphoreCurrent; // ?
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &Renderer.swapchain.vkSwapchain;
  present_info.pImageIndices = &Renderer.swapchain.imageIndex;
  present_info.pResults = NULL;

  vkQueuePresentKHR(Renderer.graphicsQueue, &present_info);
}

SDL_AppResult SDL_AppIterate(void *appstate) {

  uint64_t time_now = SDL_GetPerformanceCounter();
  delta_time = (time_now - time_last)/(double)SDL_GetPerformanceFrequency();
  time_last = time_now;
  
  SDL_Log("Frame %d, dt = %lf", Renderer.frameIndex, delta_time);
  APP_GameInputHandle();
  
  // Recreate swapchain if necessary
  if (Renderer.swapchain.requireRecreate) {
    vkDeviceWaitIdle(Renderer.vulkan.device);
    APP_VulkanSwapchainDestroy();
    APP_VulkanSwapchainCreate();
  }
  // Ask for next image in swapchain
  VkResult acquire_image_vulkan_result = 0;

  APP_RendererSwapchainNext(&acquire_image_vulkan_result);

  if (acquire_image_vulkan_result == VK_ERROR_OUT_OF_DATE_KHR) {
    Renderer.swapchain.requireRecreate = true;
    return SDL_APP_CONTINUE;
  } else if (acquire_image_vulkan_result == VK_SUBOPTIMAL_KHR) {
    Renderer.swapchain.requireRecreate = true;
  }

  // Update uniforms
  SDL_Log("Image index: %d", Renderer.swapchain.imageIndex);
  APP_VulkanUniformBufferObjectUpdate();
  
  // After we waited for semaphores, it's safe to record commands
  APP_RendererCommandsBegin();

  // Make layout barriers to stop image from fragment shader before everythin is
  // loaded
  APP_RendererBarrierBeforeFragmentShaderCreate();

  // setting up the color and debug attachments, begin dynamic rendering
  APP_RendererDynamicRenderingBegin();

  {
    VkViewport viewport = {0};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)Renderer.swapchain.width;
    viewport.height = (float)Renderer.swapchain.height;
    vkCmdSetViewport(Renderer.vulkan.frameCurrent->commandBuffer, 0, 1,
                     &viewport);

    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = Renderer.swapchain.width;
    scissor.extent.height = Renderer.swapchain.height;
    vkCmdSetScissor(Renderer.vulkan.frameCurrent->commandBuffer, 0, 1,
                    &scissor);

    // drawing the triangle YOOOHOOOOO
    vkCmdBindPipeline(Renderer.vulkan.frameCurrent->commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      Renderer.vulkan.pipeline.vkPipeline);
    // binding ubo
    vkCmdBindDescriptorSets(Renderer.vulkan.frameCurrent->commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            Renderer.vulkan.pipeline.layout, 0, 1,
                            &Renderer.vulkan.pipeline.uniform
                                 .descriptorSets[Renderer.vulkan.frameIndex],
                            0, NULL);

    memcpy(Renderer.vulkan.pipeline.vertexBuffer.allocationInfo.pMappedData,
           Renderer.vulkan.pipeline.vertexBuffer.vertices.vertices,
           sizeof(struct Vertex) *
               Renderer.vulkan.pipeline.vertexBuffer.vertices.count);
    
    
    vkCmdPushConstants(Renderer.vulkan.frameCurrent->commandBuffer,
                     Renderer.vulkan.pipeline.layout,
		       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress),
                     &Renderer.vulkan.pipeline.vertexBuffer.address);

    vkCmdDraw(Renderer.vulkan.frameCurrent->commandBuffer,
              Renderer.vulkan.pipeline.vertexBuffer.vertices.count, 1, 0, 0);
  }

  // End dynamic rendering
  APP_RendererDynamicRenderingEnd();

  // Make layout barriers to stop image from presenting before everything was
  // processed
  APP_RendererBarrierAfterFragmentShaderCreate();

  // End saving renderer commands
  APP_RendererCommandsEnd();

  // We can submit the frame to gpu
  APP_RendererCommandsSubmit();

  // Present the image to the screen
  APP_RendererPresent();

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
    break;
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;
  case SDL_EVENT_WINDOW_RESIZED:
    APP_RendererWindowSizeUpdate();
    break;
  }
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  APP_GraphicsFinalize();
}
