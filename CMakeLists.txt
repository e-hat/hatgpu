cmake_minimum_required(VERSION 3.2)

project(hatgpu VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(DEPS ${PROJECT_SOURCE_DIR}/deps)

set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
file(GLOB_RECURSE SOURCES ${SOURCE_DIR}/**/*.cpp)
file(GLOB TOPLEVEL_SOURCES ${SOURCE_DIR}/*.cpp)
set(SOURCES ${SOURCES} 
        ${SOURCE_DIR}/main.cpp
        ${SOURCE_DIR}/hatpch.cpp
        ${SOURCE_DIR}/application/Application.h
        ${SOURCE_DIR}/application/Application.cpp
        ${SOURCE_DIR}/application/InputManager.h
        ${SOURCE_DIR}/application/InputManager.cpp
        ${SOURCE_DIR}/renderers/ForwardRenderer.h
        ${SOURCE_DIR}/renderers/ForwardRenderer.cpp
        ${SOURCE_DIR}/renderers/BdptRenderer.h
        ${SOURCE_DIR}/renderers/BdptRenderer.cpp
        ${SOURCE_DIR}/util/Time.h
        ${SOURCE_DIR}/util/Time.cpp
        ${SOURCE_DIR}/util/Random.h 
        ${SOURCE_DIR}/scene/Camera.h
        ${SOURCE_DIR}/scene/Scene.h 
        ${SOURCE_DIR}/scene/Scene.cpp
        ${SOURCE_DIR}/geometry/Mesh.cpp
        ${SOURCE_DIR}/geometry/Mesh.h
        ${SOURCE_DIR}/geometry/Model.cpp
        ${SOURCE_DIR}/geometry/Model.h
        ${SOURCE_DIR}/vk/types.h
        ${SOURCE_DIR}/vk/initialize_vma.cpp
        ${SOURCE_DIR}/vk/initializers.h
        ${SOURCE_DIR}/vk/initializers.cpp
        ${SOURCE_DIR}/vk/allocator.h
        ${SOURCE_DIR}/vk/allocator.cpp
        ${SOURCE_DIR}/vk/upload_context.h
        ${SOURCE_DIR}/vk/upload_context.cpp
        ${SOURCE_DIR}/vk/deleter.h
        ${SOURCE_DIR}/texture/Texture.h
        ${SOURCE_DIR}/texture/Texture.cpp
        ${SOURCE_DIR}/geometry/prim/Sphere.h
        ${SOURCE_DIR}/geometry/prim/Sphere.cpp
        ${SOURCE_DIR}/geometry/prim/Quad.cpp
        ${SOURCE_DIR}/geometry/prim/Quad.h
        ${SOURCE_DIR}/geometry/prim/Box.h
        ${SOURCE_DIR}/geometry/prim/Box.cpp)
add_executable(hatgpu ${SOURCES})

set(INCLUDES ${SOURCE_DIR})
set(INCLUDES ${INCLUDES}
        ${DEPS}/assimp/include
        ${DEPS}/glfw/include
        ${DEPS}/glm
        ${DEPS}/imgui
        ${DEPS}/stb
        ${DEPS}/VulkanMemoryAllocator/include
        ${DEPS}/tracy/public
        ${DEPS}/json/include
        ${DEPS}/spdlog/include)
target_include_directories(hatgpu PRIVATE ${INCLUDES})

# We need to configure these options, else Assimp with build 
# every single importer and exporter. We only need GLTF B)
set(ASSIMP_BUILD_GLTF_IMPORTER TRUE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT CACHE INTERNAL FALSE)
option(ASSIMP_WARNINGS_AS_ERRORS "" OFF)
option(ASSIMP_NO_EXPORT "" ON)
option(ASSIMP_BUILD_TESTS "" OFF)
add_subdirectory(deps/assimp)
target_link_libraries(hatgpu PRIVATE assimp)

add_subdirectory(deps/glfw)
target_link_libraries(hatgpu PRIVATE glfw)

add_subdirectory(deps/glm)
target_link_libraries(hatgpu PRIVATE glm)

add_subdirectory(deps/imgui)
target_link_libraries(hatgpu PRIVATE imgui)

add_subdirectory(deps/VulkanMemoryAllocator)
# Um I guess I have to define this macro to 0, otherwise it causes build errors. 
# Not sure why this randomly started failing (and fails on previously working commits).
# Tbh I don't really care. 
target_compile_definitions(VulkanMemoryAllocator PRIVATE VMA_STATS_STRING_ENABLED=0)
target_link_libraries(hatgpu PRIVATE VulkanMemoryAllocator)

add_subdirectory(deps/tracy)
target_link_libraries(hatgpu PUBLIC Tracy::TracyClient)

add_subdirectory(deps/spdlog)
target_link_libraries(hatgpu PRIVATE spdlog::spdlog_header_only)

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(deps/json)
target_link_libraries(hatgpu PRIVATE nlohmann_json::nlohmann_json)

find_package(Vulkan REQUIRED)
target_link_libraries(hatgpu PRIVATE ${Vulkan_LIBRARIES})
target_include_directories(hatgpu PRIVATE ${Vulkan_INCLUDE_DIRS})

target_precompile_headers(hatgpu PRIVATE ${SOURCE_DIR}/hatpch.h)

# target_compile_definitions(hatgpu PRIVATE DEBUG VALIDATION_DEBUG_BREAK)
if (CMAKE_BUILD_TYPE MATCHES "Debug")
  target_compile_definitions(hatgpu PRIVATE DEBUG_BUILD)
  target_compile_definitions(hatgpu PRIVATE DEBUG)
endif()
target_compile_definitions(hatgpu PUBLIC TRACY_ENABLE GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_compile_options(hatgpu PRIVATE -Werror -Wall -Wextra -march=native)
