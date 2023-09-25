#include "hatpch.h"

#include <imgui.h>

namespace hatgpu
{
namespace ui
{
class Toggle
{
  public:
    Toggle() = default;
    Toggle(bool toggled) : mToggled(toggled) {}

    std::optional<bool> Draw(const std::string &featureName)
    {
        if (mToggled && ImGui::Button(std::format("Disable '{}'", featureName).c_str()))
        {
            mToggled = false;
            return false;
        }
        else if (!mToggled && ImGui::Button(std::format("Enable '{}'", featureName).c_str()))
        {
            mToggled = true;
            return true;
        }
        else
        {
            return std::nullopt;
        }
    }

  private:
    bool mToggled{false};
};
}  // namespace ui
}  // namespace hatgpu
