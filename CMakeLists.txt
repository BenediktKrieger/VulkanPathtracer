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

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

#add vulkan
find_package(Vulkan)
IF (Vulkan_FOUND)
    target_link_libraries(${CMAKE_PROJECT_NAME} ${Vulkan_LIBRARIES})
    target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIR})
ELSE()
    message(ERROR "Vulkan SDK has to be installed")
ENDIF()


#add sdl
set(SDL_SHARED OFF CACHE INTERNAL "" FORCE)
set(SDL_STATIC ON CACHE INTERNAL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/sdl)

#add glm
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/glm)

#add vma
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/vma)

#add vma hpp
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/vma-hpp/include)

#add tinygltf
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/tinygltf)

#add imgui
#add imgui
set(USE_BACKEND_VULKAN ON)
set(USE_BACKEND_SDL3 ON)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/imgui)

#add shaderc
set(SHADERC_SKIP_TESTS ON)
set(SHADERC_SKIP_EXAMPLES ON)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/shaderc)

target_link_libraries(${CMAKE_PROJECT_NAME} shaderc glm SDL3::SDL3 GPUOpen::VulkanMemoryAllocator imgui)


#add vma
include_directories(${CMAKE_PROJECT_NAME} PRIVATE 
    ${PROJECT_SOURCE_DIR}/third_party/vma-hpp
    ${PROJECT_SOURCE_DIR}/third_party/tinygltf
    ${PROJECT_SOURCE_DIR}/third_party/imgui
    ${PROJECT_SOURCE_DIR}/src
)

