cmake_minimum_required(VERSION 3.2)

project(efvk VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(DEPS ${PROJECT_SOURCE_DIR}/deps)

set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
file(GLOB_RECURSE SOURCES ${SOURCE_DIR}/**/*.cpp)
file(GLOB TOPLEVEL_SOURCES ${SOURCE_DIR}/*.cpp)
set(SOURCES ${SOURCES} ${TOPLEVEL_SOURCES})
add_executable(efvk ${SOURCES})

set(INCLUDES ${SOURCE_DIR})
set(INCLUDES ${INCLUDES} 
  ${DEPS}/assimp/include
  ${DEPS}/glfw/include
  ${DEPS}/glm
  ${DEPS}/imgui
  ${DEPS}/stb)
target_include_directories(efvk PRIVATE ${INCLUDES})

add_subdirectory(deps/assimp)
target_link_libraries(efvk PRIVATE assimp)

add_subdirectory(deps/glfw)
target_link_libraries(efvk PRIVATE glfw)

add_subdirectory(deps/glm)
target_link_libraries(efvk PRIVATE glm)

add_subdirectory(deps/imgui)
target_link_libraries(efvk PRIVATE imgui)

find_package(Vulkan REQUIRED)
target_link_libraries(efvk PRIVATE ${Vulkan_LIBRARIES})
target_include_directories(efvk PRIVATE ${Vulkan_INCLUDE_DIRS})
