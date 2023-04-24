#ifndef _INCLUDED_DELETER_H
#define _INCLUDED_DELETER_H
#include "hatpch.h"

#include <deque>
#include <functional>

namespace hatgpu
{
namespace vk
{

class DeletionQueue
{
  public:
    using Deleter = std::function<void()>;

    DeletionQueue() = default;

    void enqueue(Deleter &&func) noexcept { mQueue.emplace_back(func); }

    void flush() noexcept
    {
        while (!mQueue.empty())
        {
            Deleter nextDeleter = mQueue.back();
            nextDeleter();
            mQueue.pop_back();
        }
    }

  private:
    std::deque<Deleter> mQueue;
};
}  // namespace vk
}  // namespace hatgpu

#endif
