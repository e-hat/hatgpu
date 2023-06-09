#ifndef _INCLUDE_TIME_H
#define _INCLUDE_TIME_H

#include "hatpch.h"

namespace hatgpu
{
class Time
{
  public:
    Time() = default;

    float GetDeltaTime();

  private:
    float deltaTime;
    float lastFrame;
};
}  // namespace hatgpu
#endif