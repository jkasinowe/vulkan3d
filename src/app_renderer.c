#include "app_internall.h"
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_video.h>
#include <shaderc/shaderc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

struct Renderer Renderer;

SDL_AppResult APP_VulkanSwapchainCreate() {
  // Creating swapchain
  //------------------------------------------------------------------------------------------------------
  APP_RendererWindowSizeUpdate();
  Renderer.swapchain.width = Renderer.window.width;
  Renderer.swapchain.height = Renderer.window.height;
  Renderer.swapchain.requireRecreate = false;

  VkSurfaceCapabilitiesKHR surface_capabilities = {0};
  SDL_Log(APP_WAIT "Getting surface capabilities");
  VkResultCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      Renderer.vulkan.physicalDevice, Renderer.vulkan.surface,
      &surface_capabilities));
  uint32_t present_mode_count = 0;
  VkResultCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(
      Renderer.vulkan.physicalDevice, Renderer.vulkan.surface,
      &present_mode_count, NULL));
  VkPresentModeKHR *present_modes =
      malloc(sizeof(VkPresentModeKHR) * present_mode_count);
  VkResultCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(
      Renderer.vulkan.physicalDevice, Renderer.vulkan.surface,
      &present_mode_count, present_modes));
  bool present_mode_needed_found = false;
  for (uint32_t i = 0; i < present_mode_count; i++) {
    if (present_modes[i] == VK_PRESENT_MODE_FIFO_KHR) {
      present_mode_needed_found = true;
      break;
    }
  }

  SDL_Log(APP_OK "Surface capabilities ready");
  if (!present_mode_needed_found) {
    SDL_Log(APP_ERROR "Needed present mode not supported");
    return SDL_APP_FAILURE;
  }

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
                                     NULL, &Renderer.swapchain.vkSwapchain));
  SDL_Log(APP_OK "Swapchain created");

  SDL_Log(APP_WAIT "Loading swapchain images");
  vkGetSwapchainImagesKHR(Renderer.vulkan.device,
                          Renderer.swapchain.vkSwapchain,
                          &Renderer.swapchain.imageCount, NULL);
  Renderer.swapchain.images =
      malloc(Renderer.swapchain.imageCount * sizeof(VkImage));

  vkGetSwapchainImagesKHR(
      Renderer.vulkan.device, Renderer.swapchain.vkSwapchain,
      &Renderer.swapchain.imageCount, Renderer.swapchain.images);
  SDL_Log(APP_OK "Swapchain images loaded");

  SDL_Log(APP_WAIT "Creating swapchain imageviews");
  Renderer.swapchain.imageViewCount = Renderer.swapchain.imageCount;
  Renderer.swapchain.imageViews =
      malloc(Renderer.swapchain.imageViewCount * sizeof(VkImageView));

  for (uint32_t i = 0; i < Renderer.swapchain.imageCount; i++) {
    VkImageViewCreateInfo image_view_info = {0};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = Renderer.swapchain.images[i];
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = Renderer.swapchain.format;
    image_view_info.subresourceRange =
        (VkImageSubresourceRange){.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel = 0,
                                  .levelCount = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount = 1};
    VkResultCheck(vkCreateImageView(Renderer.vulkan.device, &image_view_info,
                                    NULL, &Renderer.swapchain.imageViews[i]));
  }
  SDL_Log(APP_OK "Swapchain imageviews created");

  SDL_Log(APP_WAIT "Creating swapchain semaphores");
  Renderer.swapchain.semaphoreCount = Renderer.swapchain.imageCount;
  Renderer.swapchain.semaphores =
      malloc(Renderer.swapchain.semaphoreCount * sizeof(VkSemaphore));

  for (uint32_t i = 0; i < Renderer.swapchain.semaphoreCount; i++) {
    VkSemaphoreCreateInfo semaphore_info = {0};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkResultCheck(vkCreateSemaphore(Renderer.vulkan.device, &semaphore_info,
                                    NULL, &Renderer.swapchain.semaphores[i]));
  }
  SDL_Log(APP_OK "Swapchain semaphores created");

  // Creating depth buffer
  //------------------------------------------------------------------------------------------------------
  SDL_Log(APP_WAIT "Creating depth buffer");
  VkImageCreateInfo depth_create_info = {0};
  depth_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  depth_create_info.imageType = VK_IMAGE_TYPE_2D;
  depth_create_info.format = Renderer.depth.format;
  depth_create_info.extent = (VkExtent3D){.width = Renderer.swapchain.width,
                                          .height = Renderer.swapchain.height,
                                          .depth = 1};
  depth_create_info.mipLevels = 1;
  depth_create_info.arrayLayers = 1;
  depth_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  depth_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  depth_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo alloc_info = {
      .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO};

  VkResultCheck(vmaCreateImage(Renderer.allocator, &depth_create_info,
                               &alloc_info, &Renderer.depth.image,
                               &Renderer.depth.imageAllocation, NULL));
  SDL_Log(APP_OK "Depth buffer created");

  SDL_Log(APP_WAIT "Creating depth buffer image view");
  VkImageViewCreateInfo depth_image_view_info = {0};
  depth_image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  depth_image_view_info.image = Renderer.depth.image;
  depth_image_view_info.format = Renderer.depth.format;
  depth_image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depth_image_view_info.subresourceRange =
      (VkImageSubresourceRange){.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                .levelCount = 1,
                                .layerCount = 1};
  VkResultCheck(vkCreateImageView(Renderer.vulkan.device,
                                  &depth_image_view_info, NULL,
                                  &Renderer.depth.imageView));
  SDL_Log(APP_OK "Depth buffer image view created");
  return SDL_APP_CONTINUE;
}

void APP_VulkanSwapchainDestroy() {
  SDL_Log(APP_WAIT "Destroying swapchain");
  for (uint32_t i = 0; i < Renderer.swapchain.imageViewCount; i++) {
    vkDestroyImageView(Renderer.vulkan.device, Renderer.swapchain.imageViews[i],
                       NULL);
  }

  for (uint32_t i = 0; i < Renderer.swapchain.semaphoreCount; i++) {
    vkDestroySemaphore(Renderer.vulkan.device, Renderer.swapchain.semaphores[i],
                       NULL);
  }

  if (Renderer.swapchain.vkSwapchain) {
    vkDestroySwapchainKHR(Renderer.vulkan.device,
                          Renderer.swapchain.vkSwapchain, NULL);
    Renderer.swapchain.vkSwapchain = NULL;
  }

  if (Renderer.depth.imageView) {
    vkDestroyImageView(Renderer.vulkan.device, Renderer.depth.imageView, NULL);
    vmaDestroyImage(Renderer.allocator, Renderer.depth.image,
                    Renderer.depth.imageAllocation);
    Renderer.depth.imageView = NULL;
  }
  SDL_Log(APP_OK "Swapchain destroyed");
}

// SHADER CREATION AND COMPILATION

SDL_AppResult APP_VulkanShaderModuleCreate(const char *file_name,
                                           shaderc_shader_kind kind,
                                           const char *entry_point,
                                           VkShaderModule *shader_module) {
  *shader_module = NULL;

  size_t file_path_size = strlen(APP_SHADER_PATH) + strlen(file_name) + 1;
  char *file_path = malloc(sizeof(char) * file_path_size);
  strcpy(file_path, APP_SHADER_PATH);
  strcpy(&file_path[strlen(APP_SHADER_PATH)], file_name);

  size_t src_size = 0;
  char *src = SDL_LoadFile(file_path, &src_size);

  if (!src) {
    SDL_Log("%s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_Log(APP_WAIT "Compiling shader %s", file_path);
  shaderc_compiler_t compiler = shaderc_compiler_initialize();
  shaderc_compile_options_t options = shaderc_compile_options_initialize();
  shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan,
                                         shaderc_env_version_vulkan_1_4);
  shaderc_compile_options_set_target_spirv(options, shaderc_spirv_version_1_6);
  shaderc_compile_options_set_optimization_level(
      options, shaderc_optimization_level_performance);
  char *macro_name = NULL;
  char *macro_value;
  if (kind == shaderc_vertex_shader) {
    macro_name = "VS";
    macro_value = "void vertex(); void main(){vertex();}";
  } else if (kind == shaderc_fragment_shader) {
    macro_name = "FS";
    macro_value = "void fragment(); void main(){fragment();}";
  }

  if (macro_name) {
    shaderc_compile_options_add_macro_definition(
        options, macro_name, strlen(macro_name), macro_value,
        strlen(macro_value));
  }

  shaderc_compilation_result_t result = shaderc_compile_into_spv(
      compiler, src, src_size, kind, file_name, entry_point, options);

  if (shaderc_result_get_compilation_status(result) !=
      shaderc_compilation_status_success) {
    SDL_Log(APP_ERROR "Shader compilation Error: %s",
            shaderc_result_get_error_message(result));
    return SDL_APP_FAILURE;
  }
  SDL_Log(APP_OK "Shader compiled");
  SDL_Log(APP_WAIT "Creating shader module");

  const size_t shader_size = shaderc_result_get_length(result);

  VkShaderModuleCreateInfo module_create_info = {0};
  module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  module_create_info.codeSize = shader_size;
  module_create_info.pCode = (uint32_t *)shaderc_result_get_bytes(result);

  VkResultCheck(vkCreateShaderModule(Renderer.vulkan.device,
                                     &module_create_info, NULL, shader_module));

  SDL_Log(APP_OK "Shader module created");
  SDL_free(src);
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanShadersCreate() {
  if (APP_VulkanShaderModuleCreate("shader.glsl", shaderc_vertex_shader, "main",
                                   &Renderer.vulkan.pipeline.shader.vertex) ==
      SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }

  if (APP_VulkanShaderModuleCreate("shader.glsl", shaderc_fragment_shader,
                                   "main", &Renderer.vulkan.pipeline.shader.fragment) ==
      SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

void APP_VulkanShadersDestroy() {
  SDL_Log(APP_WAIT "Destroying shaders");
  vkDestroyShaderModule(Renderer.vulkan.device, Renderer.vulkan.pipeline.shader.vertex,
                        NULL);
  vkDestroyShaderModule(Renderer.vulkan.device, Renderer.vulkan.pipeline.shader.fragment,
                        NULL);
  SDL_Log(APP_OK "Shaders destroyed");
}

SDL_AppResult APP_VulkanFindMemoryType(uint32_t type_filter,
				       VkMemoryPropertyFlags properties, uint32_t *memory_type) {
  VkPhysicalDeviceMemoryProperties available_properties;
  vkGetPhysicalDeviceMemoryProperties(Renderer.vulkan.physicalDevice,
                                      &available_properties);
  for (uint32_t i = 0; i < available_properties.memoryTypeCount; i++) {
    // if this type is available and has the same properties that we need
    if ((type_filter & (1 << i) &&
         (available_properties.memoryTypes[i].propertyFlags & properties) ==
             properties)) {
      *memory_type = i;
      return SDL_APP_CONTINUE;
    }
  }
  SDL_Log(APP_ERROR "Couldn't find a suitable memory type!");
  return SDL_APP_FAILURE;
}

SDL_AppResult APP_VulkanBufferCreate(
    VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VmaMemoryUsage memory_usage,
    VmaAllocationCreateFlags memory_flags, VkBuffer *out_buffer,
    VmaAllocation *out_allocation, VmaAllocationInfo *out_alloc_info) {

  VkBufferCreateInfo buffer_info = {0};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_info = {0};
  alloc_info.usage = memory_usage;
  alloc_info.flags = memory_flags;
  
  VkResultCheck(vmaCreateBuffer(Renderer.allocator, &buffer_info, &alloc_info,
                                out_buffer, out_allocation, out_alloc_info));
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanUniformBufferObjectUpdate() {
  SDL_Log(APP_WAIT "Updating uniform buffer object");
  struct UniformBufferObject *ubo = &Renderer.vulkan.pipeline.uniform.bufferObject;
  ubo->windowSize[0] = Renderer.window.width;
  ubo->windowSize[1] = Renderer.window.height;
  ubo->time = SDL_GetTicks()/1000.0f;

  VmaAllocationInfo *alloc_info = &Renderer.vulkan.pipeline.uniform.buffers.allocationInfo[Renderer.swapchain.imageIndex];
  SDL_Log("OK?");  
  void *mapped_data = alloc_info->pMappedData;
  SDL_Log("OK??");
  
  memcpy(mapped_data, ubo,
         sizeof(Renderer.vulkan.pipeline.uniform.bufferObject));
  SDL_Log("OK???");
  SDL_Log(APP_OK "Uniform buffer object updated");  
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanUniformBuffersCreate(struct Uniform *uniform) {
  SDL_Log(APP_WAIT"Creating uniform buffers");
  VkDeviceSize buffer_size = sizeof(uniform->bufferObject);
  uniform->buffers.vkBuffers =
      malloc(sizeof(*uniform->buffers.vkBuffers) * APP_MAX_FRAMES_IN_FLIGHT);
  uniform->buffers.allocation =
      malloc(sizeof(*uniform->buffers.allocation) * APP_MAX_FRAMES_IN_FLIGHT);
  uniform->buffers.allocationInfo = malloc(
      sizeof(*uniform->buffers.allocationInfo) * APP_MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < APP_MAX_FRAMES_IN_FLIGHT; i++) {

    APP_VulkanBufferCreate(
        buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
	VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU, // needed for uniforms 
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
	VMA_ALLOCATION_CREATE_MAPPED_BIT, // needed for persistent mapping
        &uniform->buffers.vkBuffers[i], &uniform->buffers.allocation[i],
        &uniform->buffers.allocationInfo[i]);
  }
  
  SDL_Log(APP_OK"Uniform buffers created");
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanDescriptorSetLayoutCreate() {
  // Creating descriptor set layout binding
  SDL_Log(APP_WAIT "Creating descriptor set layout");

  VkDescriptorSetLayoutBinding ubo_layout_binding = {0};
  ubo_layout_binding.binding = 0;
  ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_layout_binding.descriptorCount = 1;
  ubo_layout_binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;

  // Connecting it to a descriptor set layout
  VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {0};
  descriptor_layout_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_layout_info.bindingCount = 1;
  descriptor_layout_info.pBindings = &ubo_layout_binding;

  VkResultCheck(vkCreateDescriptorSetLayout(
      Renderer.vulkan.device, &descriptor_layout_info, NULL,
      &Renderer.vulkan.pipeline.uniform.descriptorLayout));
  SDL_Log(APP_OK"Descriptor set layout created");
  
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanDescriptorPoolCreate() {
  SDL_Log(APP_WAIT"Creating descriptor set pools");
  VkDescriptorPoolSize pool_size = {0};
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_size.descriptorCount = APP_MAX_FRAMES_IN_FLIGHT;

  VkDescriptorPoolCreateInfo pool_info = {0};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.pPoolSizes = &pool_size;
  pool_info.poolSizeCount = 1;
  pool_info.maxSets = APP_MAX_FRAMES_IN_FLIGHT;

  VkResultCheck(
      vkCreateDescriptorPool(Renderer.vulkan.device, &pool_info, NULL,
                             &Renderer.vulkan.pipeline.uniform.descriptorPool));
  SDL_Log(APP_OK"Descriptor set pool created");
  return SDL_APP_CONTINUE;
}


SDL_AppResult APP_VulkanDescriptorSetsCreate() {
  SDL_Log(APP_WAIT"Creating descriptor sets");
  VkDescriptorSetLayout descriptor_set_layouts[APP_MAX_FRAMES_IN_FLIGHT];
  for (uint32_t i = 0; i < APP_MAX_FRAMES_IN_FLIGHT; i++) {
    descriptor_set_layouts[i] = Renderer.vulkan.pipeline.uniform.descriptorLayout;
  }
  VkDescriptorSetAllocateInfo alloc_info = {0};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = Renderer.vulkan.pipeline.uniform.descriptorPool;
  alloc_info.descriptorSetCount = ARRLEN(descriptor_set_layouts);
  alloc_info.pSetLayouts = descriptor_set_layouts;

  // allocate enough size
  VkResultCheck(vkAllocateDescriptorSets(
      Renderer.vulkan.device, &alloc_info,
      Renderer.vulkan.pipeline.uniform.descriptorSets));
  SDL_Log(APP_OK"Descriptor sets created");

  SDL_Log(APP_WAIT"Updating descriptor sets");
  for (uint32_t i = 0; i < ARRLEN(Renderer.vulkan.pipeline.uniform.descriptorSets); i++) {
    VkDescriptorBufferInfo buffer_info = {0};
    buffer_info.buffer = Renderer.vulkan.pipeline.uniform.buffers.vkBuffers[i];
    buffer_info.offset = 0;
    buffer_info.range = sizeof(Renderer.vulkan.pipeline.uniform.bufferObject);


    VkWriteDescriptorSet desc_write = {0};
    desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    desc_write.dstSet = Renderer.vulkan.pipeline.uniform.descriptorSets[i];
    desc_write.dstBinding = 0;
    desc_write.dstArrayElement = 0;
    desc_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_write.descriptorCount = 1;
    desc_write.pBufferInfo = &buffer_info;
    desc_write.pImageInfo = NULL;
    desc_write.pTexelBufferView = NULL;

    vkUpdateDescriptorSets(Renderer.vulkan.device, 1, &desc_write, 0, NULL);
  }
  SDL_Log(APP_OK"Descriptor sets updated");
  
  return SDL_APP_CONTINUE;
}  

SDL_AppResult APP_VulkanPipelineCreate(struct Pipeline *pipeline) {
  // Creating pipeline
  VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pushConstantRangeCount = 0;
  pipeline_layout_info.pSetLayouts = &Renderer.vulkan.pipeline.uniform.descriptorLayout;

  SDL_Log(APP_WAIT "Creating pipeline layout");
  VkResultCheck(vkCreatePipelineLayout(Renderer.vulkan.device,
                                       &pipeline_layout_info, NULL,
                                       &pipeline->layout));
  SDL_Log(APP_OK "Pipeline layout created");
  SDL_Log(APP_WAIT "Creating pipeline");

  const char *entrypoint_vertex = "main";
  const char *entrypoint_fragment = "main";
  VkPipelineShaderStageCreateInfo shader_stages[] = {
      (VkPipelineShaderStageCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = Renderer.vulkan.pipeline.shader.vertex,
          .pName = entrypoint_vertex},
      (VkPipelineShaderStageCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = Renderer.vulkan.pipeline.shader.fragment,
          .pName = entrypoint_fragment}};

  // Primitive topology triangles
  VkPipelineVertexInputStateCreateInfo vert_input_info = {0};
  vert_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {0};
  input_assembly_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // Depth testing
  VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {0};
  depth_stencil_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_info.depthTestEnable = VK_TRUE;
  depth_stencil_info.depthWriteEnable = VK_TRUE;
  depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil_info.stencilTestEnable = VK_FALSE;

  // Viewport
  VkPipelineViewportStateCreateInfo viewport_info = {0};
  viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_info.viewportCount = 1;
  viewport_info.pViewports = NULL;
  viewport_info.scissorCount = 1;
  viewport_info.pScissors = NULL;

  // Rasterization
  VkPipelineRasterizationStateCreateInfo raster_info = {0};
  raster_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster_info.polygonMode = VK_POLYGON_MODE_FILL;
  raster_info.cullMode = VK_CULL_MODE_BACK_BIT;
  raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  raster_info.lineWidth = 1.0f;

  // Multisampling
  VkPipelineMultisampleStateCreateInfo multisampling_info = {0};
  multisampling_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // Alpha blending
  VkPipelineColorBlendAttachmentState attachment_state = {0};
  attachment_state.blendEnable = VK_FALSE;
  attachment_state.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo blend_info = {0};
  blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend_info.attachmentCount = 1;
  blend_info.pAttachments = &attachment_state;

  // Enable dynamic state
  VkDynamicState dynamic_state[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo dynamic_state_info = {0};
  dynamic_state_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state_info.dynamicStateCount = ARRLEN(dynamic_state);
  dynamic_state_info.pDynamicStates = dynamic_state;

  // Dynamic rendering
  VkPipelineRenderingCreateInfo render_info = {0};
  render_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  render_info.colorAttachmentCount = 1;
  render_info.pColorAttachmentFormats = &Renderer.swapchain.format;
  render_info.depthAttachmentFormat = Renderer.depth.format;

  // Graphics pipeline
  VkGraphicsPipelineCreateInfo pipeline_info = {0};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.pNext = &render_info; // for dynamic rendering
  pipeline_info.stageCount = ARRLEN(shader_stages);
  pipeline_info.pStages = shader_stages; // Shader stages with VkShader's
  pipeline_info.pVertexInputState =
      &vert_input_info; // blank vertex input state
  pipeline_info.pInputAssemblyState =
      &input_assembly_info; // triangle list topology
  pipeline_info.pViewportState =
      &viewport_info; // single viewport and scissor test, but dynamicaly
  pipeline_info.pRasterizationState =
      &raster_info; // culling backfaces, front direction CCW
  pipeline_info.pMultisampleState = &multisampling_info; // no multisampling
  pipeline_info.pDepthStencilState =
      &depth_stencil_info;                      // depth testing and writing on
  pipeline_info.pColorBlendState = &blend_info; // one color attachment
  pipeline_info.pDynamicState =
      &dynamic_state_info; // viewport and scissor states are dynamic
  pipeline_info.layout =
      pipeline->layout; // pipeline doesn't expect any descriptors
  pipeline_info.renderPass =
      VK_NULL_HANDLE; // not using render passes, dynamic rendering

  pipeline->vkPipeline = (VkPipeline){0};
  VkResultCheck(vkCreateGraphicsPipelines(Renderer.vulkan.device, NULL, 1,
                                          &pipeline_info, NULL, &pipeline->vkPipeline));

  SDL_Log(APP_OK "Pipeline created");
  return SDL_APP_CONTINUE;
}

void APP_VulkanPipelineDestroy(struct Pipeline *pipeline) {
  vkDestroyPipeline(Renderer.vulkan.device, pipeline->vkPipeline, NULL);
  vkDestroyPipelineLayout(Renderer.vulkan.device,
                          Renderer.vulkan.pipeline.layout, NULL);
  vkDestroyDescriptorSetLayout(Renderer.vulkan.device, Renderer.vulkan.pipeline.uniform.descriptorLayout, NULL);
}

SDL_AppResult APP_VulkanSyncResourcesCreate() {

  // Timeline semaphore needed for sync between cpu and gpu
  VkSemaphoreTypeCreateInfo semaphore_type_info = {0};
  semaphore_type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  semaphore_type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  semaphore_type_info.initialValue = APP_MAX_FRAMES_IN_FLIGHT;

  VkSemaphoreCreateInfo semaphore_info = {0};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_info.pNext = &semaphore_type_info;

  SDL_Log(APP_WAIT "Creating timeline semaphore");
  VkResultCheck(vkCreateSemaphore(Renderer.vulkan.device, &semaphore_info, NULL,
                                  &Renderer.vulkan.timelineSemaphore));
  SDL_Log(APP_OK "Timeline semaphore created");

  SDL_Log(APP_WAIT "Creating frame semaphores");
  for (uint32_t i = 0; i < APP_MAX_FRAMES_IN_FLIGHT; i++) {
    VkSemaphoreCreateInfo semaphore_info = {0};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkResultCheck(vkCreateSemaphore(
        Renderer.vulkan.device, &semaphore_info, NULL,
        &Renderer.vulkan.framesInFlight[i].imageAcquiredSemaphore));
  }
  SDL_Log(APP_OK "Frame semaphores created");

  return SDL_APP_CONTINUE;
}

void APP_VulkanSyncResourcesDestroy() {
  SDL_Log(APP_WAIT "Destroying frame semaphores");
  for (uint32_t i = 0; i < APP_MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(Renderer.vulkan.device,
                       Renderer.vulkan.framesInFlight[i].imageAcquiredSemaphore,
                       NULL);
  }
  SDL_Log(APP_OK "Frame semaphores destroyed");
  SDL_Log(APP_WAIT "Destroying timeline semaphore");
  vkDestroySemaphore(Renderer.vulkan.device, Renderer.vulkan.timelineSemaphore,
                     NULL);
  SDL_Log(APP_OK "Timeline semaphore destroyed");
}

SDL_AppResult APP_VulkanCommandBuffersCreate() {
  SDL_Log(APP_WAIT "Creating frames in flight command buffers");
  for (uint32_t i = 0; i < APP_MAX_FRAMES_IN_FLIGHT; i++) {
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = Renderer.graphicsQueueFamilyId;
    VkResultCheck(
        vkCreateCommandPool(Renderer.vulkan.device, &pool_info, NULL,
                            &Renderer.vulkan.framesInFlight[i].commandPool));

    VkCommandBufferAllocateInfo command_alloc_info = {0};
    command_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_alloc_info.commandPool =
        Renderer.vulkan.framesInFlight[i].commandPool;
    command_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_alloc_info.commandBufferCount = 1;

    VkResultCheck(vkAllocateCommandBuffers(
        Renderer.vulkan.device, &command_alloc_info,
        &Renderer.vulkan.framesInFlight[i].commandBuffer));
  }
  SDL_Log(APP_OK "Frames in flight command buffer created");

  return SDL_APP_CONTINUE;
}

void APP_VulkanCommandBuffersDestroy() {
  for (uint32_t i = 0; i < APP_MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroyCommandPool(Renderer.vulkan.device,
                         Renderer.vulkan.framesInFlight[i].commandPool, NULL);
  }
}

SDL_AppResult APP_RendererInit() {
  Renderer.window.width = 800;
  Renderer.window.height = 600;
  Renderer.swapchain.format = APP_COLOR_FORMAT;
  Renderer.depth.format = APP_DEPTH_FORMAT;
  Renderer.vulkan.nextSignalValue = APP_MAX_FRAMES_IN_FLIGHT + 1;
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_SDLInit() {
  SDL_ResultCheck(SDL_Init(SDL_INIT_VIDEO));

  Renderer.window.sdlWindow = SDL_CreateWindow(
      "Vulkan + SDL3", Renderer.window.width, Renderer.window.height,
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  SDL_ResultCheck(Renderer.window.sdlWindow);
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanVolkBind() {
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
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanInstanceCreate() {
  SDL_Log(APP_WAIT "Creating and loading a vulkan instance");
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

  VkResultCheck(
      vkCreateInstance(&instance_info, NULL, &Renderer.vulkan.instance));
  volkLoadInstance(Renderer.vulkan.instance);
  SDL_Log(APP_OK "Vulkan instance loaded");

  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanSurfaceCreate() {
  SDL_Log(APP_WAIT "Creating vulkan surface");
  SDL_ResultCheck(SDL_Vulkan_CreateSurface(Renderer.window.sdlWindow,
                                           Renderer.vulkan.instance, NULL,
                                           &Renderer.vulkan.surface));
  SDL_Log(APP_OK "Vulkan surface created");
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanPhysicalDeviceFind() {
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
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanGraphicsQueueFind() {
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
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanDeviceCreate() {
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
      !supported_features_12.timelineSemaphore ||
      !supported_features_12.bufferDeviceAddress) {
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
      .bufferDeviceAddress = VK_TRUE,
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

  SDL_Log(APP_WAIT "Getting device graphics queue");
  vkGetDeviceQueue(Renderer.vulkan.device, Renderer.graphicsQueueFamilyId, 0,
                   &Renderer.graphicsQueue);
  SDL_Log(APP_OK "Device graphics queue ready");
  return SDL_APP_CONTINUE;
}

SDL_AppResult APP_VulkanVMACreate() {
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
  return SDL_APP_CONTINUE;
}

void APP_VulkanVolkUnbind() { volkFinalize(); }

void APP_VulkanInstanceDestroy() {
  if (Renderer.vulkan.instance) {
    vkDestroyInstance(Renderer.vulkan.instance, NULL);
  }
}

void APP_VulkanSurfaceDestroy() {
  if (Renderer.vulkan.surface) {
    vkDestroySurfaceKHR(Renderer.vulkan.instance, Renderer.vulkan.surface,
                        NULL);
  }
}

void APP_VulkanDeviceDestroy() {
  if (Renderer.vulkan.device) {
    vkDestroyDevice(Renderer.vulkan.device, NULL);
  }
}

void APP_VulkanVMADestroy() {
  if (Renderer.allocator) {
    vmaDestroyAllocator(Renderer.allocator);
  }
}
