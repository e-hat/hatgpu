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
        ${SOURCE_DIR}/application/ForwardRenderer.h
        ${SOURCE_DIR}/application/ForwardRenderer.cpp
        ${SOURCE_DIR}/application/InputManager.h
        ${SOURCE_DIR}/application/InputManager.cpp
        ${SOURCE_DIR}/application/BdptRenderer.h
        ${SOURCE_DIR}/application/BdptRenderer.cpp
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
        ${SOURCE_DIR}/vk/allocator.cpp)
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
set_target_properties(assimp PROPERTIES BUILD_SHARED_LIBS TRUE)
target_link_libraries(hatgpu PRIVATE assimp)

add_subdirectory(deps/glfw)
target_link_libraries(hatgpu PRIVATE glfw)

add_subdirectory(deps/glm)
target_link_libraries(hatgpu PRIVATE glm)

add_subdirectory(deps/imgui)
target_link_libraries(hatgpu PRIVATE imgui)

add_subdirectory(deps/VulkanMemoryAllocator)
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
target_compile_definitions(hatgpu PRIVATE DEBUG)
target_compile_definitions(hatgpu PUBLIC TRACY_ENABLE GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_compile_options(hatgpu PRIVATE -Werror -Wall -Wextra)
