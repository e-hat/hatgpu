#ifndef _INCLUDE_TIME_H
#define _INCLUDE_TIME_H
#include "efpch.h"

namespace efvk
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
}  // namespace efvk
#endif