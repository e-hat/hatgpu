#ifndef _INCLUDE_AABBLAYER_H
#define _INCLUDE_AABBLAYER_H
#include "hatpch.h"

#include "application/Layer.h"

namespace hatgpu
{

class AabbLayer : public Layer
{
  public:
    AabbLayer() : Layer("AabbLayer"), mToggled(false) {}

    void OnAttach() override;
    void OnDetach() override;
    void OnRender() override;
    void OnImGuiRender() override;

  private:
    bool mToggled;
};

}  // namespace hatgpu

#endif  //_INCLUDE_AABBLAYER_H
