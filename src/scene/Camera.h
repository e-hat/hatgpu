#ifndef _INCLUDE_CAMERA_H
#define _INCLUDE_CAMERA_H
#include "hatpch.h"

#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/gtx/quaternion.hpp>

#include <cmath>
#include <iostream>
#include <vector>

namespace hatgpu
{
enum class CameraMovement
{
    LEFT,
    RIGHT,
    UP,
    DOWN,
    FORWARD,
    BACKWARD,
    SPIN_UP,
    SPIN_DOWN,
    SPIN_LEFT,
    SPIN_RIGHT,
};

static constexpr float kSpeed             = 10.F;
static constexpr float kSensitivity       = 0.001f;
static constexpr float kMouseVelocity     = 45.f;
static constexpr float kMaxMouseVelocity  = 0.01f;
static constexpr float kMinMouseVelocity  = -kMaxMouseVelocity;
static constexpr float kZoomVelocity      = 1.0f;
static constexpr float kMinTargetDistance = 0.1f;
static constexpr float kMaxTargetDistance = 100.f;
static constexpr float kAngularVelocity   = 2.5f;
static constexpr float kMaxSpinDistance   = 0.1f;

class Camera
{
  public:
    glm::vec3 Position;
    glm::vec3 Target;
    glm::vec3 WorldUp;
    glm::vec3 Up;

    float Near;
    float Far;

    int ScreenWidth;
    int ScreenHeight;

    float MovementSpeed;
    float MouseSensitivity;
    float TargetDistance;

    Camera(int screenWidth    = 1366,
           int screenHeight   = 768,
           glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
           glm::vec3 up       = glm::vec3(0.0f, 1.0f, 0.0f),
           float near         = 0.1f,
           float far          = 200.f)
        : Position(position),
          Target(glm::vec3(0.f)),
          WorldUp(up),
          Up(WorldUp),
          Near(near),
          Far(far),
          ScreenWidth(screenWidth),
          ScreenHeight(screenHeight),
          MovementSpeed(kSpeed),
          MouseSensitivity(kSensitivity)
    {}

    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ)
        : Position(glm::vec3(posX, posY, posZ)),
          Target(glm::vec3(0.f)),
          WorldUp(glm::vec3(upX, upY, upZ)),
          Up(WorldUp),
          MovementSpeed(kSpeed),
          MouseSensitivity(kSensitivity)
    {}

    inline glm::mat4 GetViewMatrix() const { return glm::lookAt(Position, Target, Up); }

    inline glm::mat4 GetProjectionMatrix() const
    {
        auto result = glm::perspective(glm::radians(45.0f),
                                       (float)ScreenWidth / (float)ScreenHeight, Near, Far);
        result[1][1] *= -1;
        return result;
    }

    void ProcessKeyboard(CameraMovement direction, float dt)
    {
        float velocity = MovementSpeed * dt;
        switch (direction)
        {
            case CameraMovement::LEFT:
                pan(-velocity, 0.f, 0.f);
                break;
            case CameraMovement::RIGHT:
                pan(velocity, 0.f, 0.f);
                break;
            case CameraMovement::UP:
                pan(0.f, velocity, 0.f);
                break;
            case CameraMovement::DOWN:
                pan(0.f, -velocity, 0.f);
                break;
            case CameraMovement::FORWARD:
                pan(0.f, 0.f, -velocity);
                break;
            case CameraMovement::BACKWARD:
                pan(0.f, 0.f, velocity);
                break;
            case CameraMovement::SPIN_UP:
                spinAroundX(dt, 1);
                break;
            case CameraMovement::SPIN_DOWN:
                spinAroundX(dt, -1);
                break;
            case CameraMovement::SPIN_LEFT:
                spinAroundY(dt, -1);
                break;
            case CameraMovement::SPIN_RIGHT:
                spinAroundY(dt, 1);
                break;
        }
    }

    void ProcessMouseScroll(float yOffset)
    {
        if (yOffset > 0)
        {
            zoomIn();
        }
        else
        {
            zoomOut();
        }
    }

  private:
    inline glm::vec3 forward() const { return glm::normalize(Target - Position); }

    inline glm::vec3 right() const { return glm::normalize(glm::cross(forward(), Up)); }

    void zoomIn()
    {
        const glm::vec3 delta     = Target - Position;
        const glm::vec3 direction = glm::normalize(delta);

        glm::vec3 pos = Position + direction * kZoomVelocity;

        if (glm::distance(pos, Target) > kMinTargetDistance)
            Position = pos;
    }

    void zoomOut()
    {
        const glm::vec3 delta     = Target - Position;
        const glm::vec3 direction = glm::normalize(delta);

        glm::vec3 pos = Position - direction * kZoomVelocity;

        if (glm::distance(pos, Target) < kMaxTargetDistance)
            Position = pos;
    }

    void pan(float horizontal, float vertical, float zAmt)
    {
        const glm::mat4 view    = GetViewMatrix();
        const glm::mat4 viewInv = glm::inverse(view);
        const glm::mat4 translation =
            glm::translate(glm::mat4(1.f), glm::vec3(horizontal, vertical, zAmt));
        const glm::mat4 transform = viewInv * translation * view;

        glm::vec4 pos    = transform * glm::vec4(Position, 1.f);
        glm::vec4 target = transform * glm::vec4(Target, 1.f);

        Position = glm::vec3(pos);
        Target   = glm::vec3(target);
    }

    void spinAroundY(float dt, int direction)
    {
        float diskRadius =
            std::sqrt(glm::length2(Position - Target) - std::pow((Position - Target).z, 2));
        float dTheta = kAngularVelocity * dt;
        if (dTheta * diskRadius > kMaxSpinDistance)
        {
            dTheta = kMaxSpinDistance / diskRadius;
        }

        const glm::mat4 view    = glm::lookAt(Target, Position, Up);
        const glm::mat4 viewInv = glm::inverse(view);
        const glm::mat4 rotation =
            glm::rotate(glm::mat4(1.f), direction * dTheta, glm::vec3(0.f, 1.f, 0.f));
        const glm::mat4 transform = viewInv * rotation * view;

        glm::vec4 pos = transform * glm::vec4(Position, 1.f);

        Position = glm::vec3(pos);
    }

    void spinAroundX(float dt, int direction)
    {
        float diskRadius = glm::length(Position - Target);
        float dTheta     = kAngularVelocity * dt;
        if (dTheta * diskRadius > kMaxSpinDistance)
        {
            dTheta = kMaxSpinDistance / diskRadius;
        }

        const glm::mat4 view    = glm::lookAt(Target, Position, Up);
        const glm::mat4 viewInv = glm::inverse(view);
        const glm::mat4 rotation =
            glm::rotate(glm::mat4(1.f), direction * dTheta, glm::vec3(1.f, 0.f, 0.f));
        const glm::mat4 transform = viewInv * rotation * view;

        glm::vec4 pos = transform * glm::vec4(Position, 1.f);

        Position = glm::vec3(pos);
    }
};
}  // namespace hatgpu
#endif
