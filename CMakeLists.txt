cmake_minimum_required(VERSION 3.7)
project(VkPathtracer VERSION 1.0.0)


add_definitions(-D_CRT_SECURE_NO_WARNINGS)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
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

#add glm
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/glm)

#add glslang
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/glslang)

target_link_libraries(VkPathtracer glslang SPIRV glm SDL3::SDL3)

#add imgui, stb
include_directories(${PROJECT_SOURCE_DIR}/third_party/imgui)
include_directories(${PROJECT_SOURCE_DIR}/third_party/stb)
include_directories(${PROJECT_SOURCE_DIR}/src)

