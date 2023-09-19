#include "hatpch.h"

#include "AabbLayer.h"

#include "imgui.h"

namespace hatgpu
{
void AabbLayer::OnAttach() {}

void AabbLayer::OnDetach() {}

void AabbLayer::OnImGuiRender()
{
    ImGui::Checkbox("Enable AABB visualization layer", &mToggled);
}

void AabbLayer::OnRender() {}

}  // namespace hatgpu
