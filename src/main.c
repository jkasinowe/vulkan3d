#include "app_internall.h"
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  if (APP_GraphicsInit() != SDL_APP_CONTINUE) {
    return SDL_APP_FAILURE;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
  SDL_Log("Frame %d", Renderer.frameIndex); 
  
  if (Renderer.swapchain.requireRecreate) {
    vkDeviceWaitIdle(Renderer.vulkan.device);
    APP_VulkanSwapchainDestroy();
    APP_VulkanSwapchainCreate();
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
  APP_GraphicsFinalize();
}
