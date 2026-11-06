set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

cmake_minimum_required(VERSION 3.12)
project(OpenGLProject LANGUAGES C)

add_executable(vulkan_app main.c)

find_package(Vulkan REQUIRED COMPONENTS volk)
find_package(SDL3 REQUIRED)
find_package(cglm REQUIRED)

# Link the imported targets
target_link_libraries(vulkan_app PRIVATE Vulkan::volk SDL3::SDL3 cglm)
