cmake_minimum_required(VERSION 3.7)
project(VkPathtracer VERSION 1.0.0)

add_compile_definitions(SHADER_PATH="${PROJECT_SOURCE_DIR}/shader" ASSET_PATH="${PROJECT_SOURCE_DIR}/assets" _CRT_SECURE_NO_WARNINGS)
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
set(SDL_SHARED OFF CACHE INTERNAL "" FORCE)
set(SDL_STATIC ON CACHE INTERNAL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/sdl)

#add glm
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/glm)

#add glslang
set(ENABLE_OPT OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/glslang)

#add vma
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/vma)

#add tinygltf
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/tinygltf)

#add imgui
file(GLOB IMGUI_SOURCES
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_demo.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_vulkan.cpp
    third_party/imgui/backends/imgui_impl_sdl3.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_link_libraries(imgui PUBLIC ${Vulkan_LIBRARIES} SDL3::SDL3)
target_include_directories(imgui PUBLIC ${PROJECT_SOURCE_DIR}/third_party/imgui)

target_link_libraries(VkPathtracer glslang SPIRV glm SDL3::SDL3 GPUOpen::VulkanMemoryAllocator imgui)


#add vma
include_directories(${PROJECT_SOURCE_DIR}/third_party/vma-hpp)
include_directories(${PROJECT_SOURCE_DIR}/third_party/tinygltf)
include_directories(${PROJECT_SOURCE_DIR}/src)

