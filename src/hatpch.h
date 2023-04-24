#ifndef _INCLUDE_EFPCH_H
#define _INCLUDE_EFPCH_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>
#ifdef DEBUG
#    define H_CHECK(stmt, msg)                                                                    \
        if ((stmt) != VK_SUCCESS)                                                                 \
        {                                                                                         \
            spdlog::error("HATGPU: [FAILED VULKAN CHECK ({}, {})]: {}", __FILE__, __LINE__, msg); \
            std::exit(1);                                                                         \
        }

#    if defined(_WIN32)
#        define BREAK __debugbreak()
#    elif defined(__GNUC__)
#        define BREAK __builtin_trap()
#    endif

#    define H_ASSERT(stmt, msg)                                                             \
        if (!(stmt))                                                                        \
        {                                                                                   \
            spdlog::error("HATGPU: [FAILED ASSERT ({}, {})]: {}", __FILE__, __LINE__, msg); \
            std::exit(1);                                                                   \
        }

#    define H_LOG(msg) spdlog::info("HATGPU: {}", msg);

#else
#    define H_CHECK(stmt, msg)
#    define H_ASSERT(stmt, msg)
#    define H_LOG(msg)
#endif

#endif
