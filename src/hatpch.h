#ifndef _INCLUDE_EFPCH_H
#define _INCLUDE_EFPCH_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifdef DEBUG
#    define SPDLOG_ACTIVE_LEVEL SPD_LEVEL_DEBUG
#endif
#include <spdlog/spdlog.h>
#include <memory>
#define LOGGER ::hatgpu::Logger::GetOrCreateInstance()

#ifdef DEBUG
#    if defined(_WIN32)
#        define BREAK __debugbreak()
#    elif defined(__GNUC__)
#        define BREAK __builtin_trap()
#    endif

#    define H_CHECK(stmt, msg)                                                               \
        if ((stmt) != VK_SUCCESS)                                                            \
        {                                                                                    \
            LOGGER.error("[{}] [{}:{}] [{}]", msg, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
            LOGGER.dump_backtrace();                                                         \
            BREAK;                                                                           \
        }

#    define H_ASSERT(stmt, msg)                                                       \
        if (!(stmt))                                                                  \
        {                                                                             \
            LOGGER.error("[FAILED ASSERT: {}] [{}:{}] [{}]", msg, __FILE__, __LINE__, \
                         __PRETTY_FUNCTION__);                                        \
            LOGGER.dump_backtrace();                                                  \
            BREAK;                                                                    \
        }

#    define H_LOG(msg) LOGGER.debug("{}", msg);

#else
#    define H_CHECK(stmt, msg) static_cast<void>(stmt)
#    define H_ASSERT(stmt, msg) static_cast<void>(stmt)
#    define H_LOG(msg)
#endif

namespace hatgpu
{
class Logger
{
  public:
    static spdlog::logger &GetOrCreateInstance();

  private:
    static std::unique_ptr<spdlog::logger> mInstance;
};
}  // namespace hatgpu

#endif
