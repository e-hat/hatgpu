#include "application/Application.h"
#include "hatpch.h"

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>

#include <memory>

ABSL_FLAG(std::string, scene_path, "../scenes/sponza.json", "The path to the scene json file.");

int main(int argc, char **argv)
{
    absl::ParseCommandLine(argc, argv);
    auto app = std::make_unique<hatgpu::Application>("HatGPU", absl::GetFlag(FLAGS_scene_path));
    app->Init();
    app->Run();

    return 0;
}
