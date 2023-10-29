cmake_minimum_required(VERSION 3.7)
project(VkPathtracer VERSION 1.0.0)


add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
add_compile_definitions(SHADER_PATH="${PROJECT_SOURCE_DIR}/shaders" ASSET_PATH="${PROJECT_SOURCE_DIR}/src/textures")
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
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/sdl)
target_link_libraries(VkPathtracer sdl)

#add glm
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/glm)
target_link_libraries(VkPathtracer glm)

#add glslang
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/glslang)
target_link_libraries(VkPathtracer glslang SPIRV)

#add imgui, stb, tinyobjloader
include_directories(${PROJECT_SOURCE_DIR}/third_party/imgui)
include_directories(${PROJECT_SOURCE_DIR}/third_party/stb)

