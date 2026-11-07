set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

cmake_minimum_required(VERSION 3.16)
project(VulkanProject LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Vulkan App
add_executable(vulkan_app main.c vk_mem_alloc.cpp)

find_package(Vulkan REQUIRED COMPONENTS volk)
find_package(SDL3 REQUIRED)


# Link the imported targets
target_link_libraries(vulkan_app PRIVATE Vulkan::volk SDL3::SDL3)


