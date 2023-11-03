cmake_minimum_required(VERSION 3.7)
project(VkPathtracer VERSION 1.0.0)

add_compile_definitions(SHADER_PATH="${PROJECT_SOURCE_DIR}/shaders" ASSET_PATH="${PROJECT_SOURCE_DIR}/assets" _CRT_SECURE_NO_WARNINGS)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

IF(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
ENDIF()

file(GLOB SOURCES
    src/*.h
    src/*.cpp
)

add_executable(VkPathtracer ${SOURCES})

#add vulkan
find_package(Vulkan)
IF (Vulkan_FOUND)
    target_link_libraries(VkPathtracer ${Vulkan_LIBRARIES})
    target_include_directories(VkPathtracer PUBLIC ${Vulkan_INCLUDE_DIR})
ELSE()
    message(ERROR "Vulkan SDK has to be installed")
ENDIF()


#add sdl
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/sdl)

#add glm
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/glm)

#add glslang
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/glslang)

#add vma
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/vma)

#add tinygltf
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/tinygltf)

target_link_libraries(VkPathtracer glslang SPIRV glm SDL3::SDL3 GPUOpen::VulkanMemoryAllocator)


#add imgui, vma
include_directories(${PROJECT_SOURCE_DIR}/third_party/imgui)
include_directories(${PROJECT_SOURCE_DIR}/third_party/vma-hpp)
include_directories(${PROJECT_SOURCE_DIR}/third_party/tinygltf)
include_directories(${PROJECT_SOURCE_DIR}/src)

