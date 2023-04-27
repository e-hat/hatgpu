#include "hatpch.h"

#if defined(DEBUG) && defined(linux)
#    define SEGFAULT_HANDLER
#    include <execinfo.h>
#    include <signal.h>
#endif

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>

#ifdef SEGFAULT_HANDLER
namespace
{
static void SignalHandler(int sig)
{
    // Dump the logs when we get a signal
    LOGGER.error("[SEGFAULT HANDLER] Signal {}", sig);
    LOGGER.dump_backtrace();
    BREAK;
}

static void SetSegfaultHandler()
{
    signal(SIGSEGV, SignalHandler);
}
}  // namespace
#endif

namespace hatgpu
{
std::unique_ptr<spdlog::logger> Logger::mInstance = nullptr;

spdlog::logger &Logger::GetOrCreateInstance()
{
    if (mInstance == nullptr)
    {
        auto errorSink = std::make_shared<spdlog::sinks::stderr_color_sink_st>();
        errorSink->set_level(spdlog::level::err);
        // We need "%^" and "%$" here to specify the range to use color
        errorSink->set_pattern("[%n] [%^%l%$] %v");

        auto debugSink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
        debugSink->set_level(spdlog::level::debug);
        debugSink->set_pattern("[%n] [%^%l%$] %v");

        mInstance =
            std::unique_ptr<spdlog::logger>(new spdlog::logger("HatGpu", {errorSink, debugSink}));

        mInstance->enable_backtrace(128);

#ifdef SEGFAULT_HANDLER
        SetSegfaultHandler();
#endif
    }
    return *mInstance;
}
}  // namespace hatgpu
