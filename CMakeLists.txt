cmake_minimum_required(VERSION 3.2)

project(efvk VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(DEPS ${PROJECT_SOURCE_DIR}/deps)

set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
file(GLOB_RECURSE SOURCES ${SOURCE_DIR}/**/*.cpp)
file(GLOB TOPLEVEL_SOURCES ${SOURCE_DIR}/*.cpp)
set(SOURCES ${SOURCES} ${SOURCE_DIR}/main.cpp
        ${SOURCE_DIR}/application/Application.h
        ${SOURCE_DIR}/application/Application.cpp
        ${SOURCE_DIR}/application/EfvkRenderer.h
        ${SOURCE_DIR}/application/EfvkRenderer.cpp
        ${SOURCE_DIR}/application/InputManager.h
        ${SOURCE_DIR}/application/InputManager.cpp
        ${SOURCE_DIR}/util/Time.h
        ${SOURCE_DIR}/util/Time.cpp
        ${SOURCE_DIR}/scene/Camera.h
        ${SOURCE_DIR}/geometry/Mesh.cpp
        ${SOURCE_DIR}/geometry/Mesh.h
        ${SOURCE_DIR}/efvk_types.h
        ${SOURCE_DIR}/initialize_vma.cpp src/geometry/Model.cpp src/geometry/Model.h)
add_executable(efvk ${SOURCES})

set(INCLUDES ${SOURCE_DIR})
set(INCLUDES ${INCLUDES}
        ${DEPS}/assimp/include
        ${DEPS}/glfw/include
        ${DEPS}/glm
        ${DEPS}/imgui
        ${DEPS}/stb
        ${DEPS}/VulkanMemoryAllocator/include)
target_include_directories(efvk PRIVATE ${INCLUDES})

add_subdirectory(deps/assimp)
target_link_libraries(efvk PRIVATE assimp)

add_subdirectory(deps/glfw)
target_link_libraries(efvk PRIVATE glfw)

add_subdirectory(deps/glm)
target_link_libraries(efvk PRIVATE glm)

add_subdirectory(deps/imgui)
target_link_libraries(efvk PRIVATE imgui)

add_subdirectory(deps/VulkanMemoryAllocator)
target_link_libraries(efvk PRIVATE VulkanMemoryAllocator)

find_package(Vulkan REQUIRED)
target_link_libraries(efvk PRIVATE ${Vulkan_LIBRARIES})
target_include_directories(efvk PRIVATE ${Vulkan_INCLUDE_DIRS})

target_precompile_headers(efvk PRIVATE ${SOURCE_DIR}/efpch.h)

target_compile_definitions(efvk PRIVATE DEBUG)

target_compile_options(efvk PRIVATE -Werror -Wall -Wextra)
