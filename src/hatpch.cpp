#include "hatpch.h"

#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>

namespace hatgpu
{
std::unique_ptr<spdlog::logger> Logger::mInstance = nullptr;

spdlog::logger &Logger::GetOrCreateInstance()
{
    if (mInstance == nullptr)
    {
        // FIXME: I somehow broke the color output. Fix this.
        auto errorSink = std::make_shared<spdlog::sinks::stderr_color_sink_st>();
        errorSink->set_level(spdlog::level::err);
        errorSink->set_pattern("[%n] [%l] %v");

        auto debugSink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
        debugSink->set_level(spdlog::level::debug);
        debugSink->set_pattern("[%n] [%l] [%v]");

        mInstance =
            std::unique_ptr<spdlog::logger>(new spdlog::logger("HatGpu", {errorSink, debugSink}));

        mInstance->enable_backtrace(128);
    }
    return *mInstance;
}
}  // namespace hatgpu
