#ifndef _INCLUDE_CAMERA_H
#define _INCLUDE_CAMERA_H
#include "efpch.h"

#include <vector>

namespace hatgpu
{
enum class CameraMovement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    LOOKUP,
    LOOKDOWN,
    LOOKLEFT,
    LOOKRIGHT
};

const float YAW            = -90.0f;
const float PITCH          = 0.0f;
const float SPEED          = 10.F;
const float SENSITIVITY    = 0.1f;
const float MOUSE_VELOCITY = 45.f;
const float ZOOM           = 45.0f;

class Camera
{
  public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Near;
    float Far;

    int ScreenWidth;
    int ScreenHeight;

    float Yaw;
    float Pitch;

    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    Camera(int screenWidth    = 1366,
           int screenHeight   = 768,
           glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
           glm::vec3 up       = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw          = YAW,
           float pitch        = PITCH,
           float near         = 0.1f,
           float far          = 200.f)
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)),
          Near(near),
          Far(far),
          ScreenWidth(screenWidth),
          ScreenHeight(screenHeight),
          MovementSpeed(SPEED),
          MouseSensitivity(SENSITIVITY),
          Zoom(ZOOM)
    {
        Position = position;
        WorldUp  = up;
        Yaw      = yaw;
        Pitch    = pitch;
        updateCameraVectors();
    }

    Camera(float posX,
           float posY,
           float posZ,
           float upX,
           float upY,
           float upZ,
           float yaw,
           float pitch)
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)),
          MovementSpeed(SPEED),
          MouseSensitivity(SENSITIVITY),
          Zoom(ZOOM)
    {
        Position = glm::vec3(posX, posY, posZ);
        WorldUp  = glm::vec3(upX, upY, upZ);
        Yaw      = yaw;
        Pitch    = pitch;
        updateCameraVectors();
    }

    inline glm::mat4 GetViewMatrix() const { return glm::lookAt(Position, Position + Front, Up); }

    inline glm::mat4 GetProjectionMatrix() const
    {
        auto result = glm::perspective(glm::radians(Zoom), (float)ScreenWidth / (float)ScreenHeight,
                                       Near, Far);
        result[1][1] *= -1;
        return result;
    }

    void ProcessKeyboard(CameraMovement direction, float deltaTime)
    {
        float velocity  = MovementSpeed * deltaTime;
        float mouseDiff = MOUSE_VELOCITY * deltaTime;
        switch (direction)
        {
            case CameraMovement::FORWARD:
                Position += Front * velocity;
                break;
            case CameraMovement::BACKWARD:
                Position -= Front * velocity;
                break;
            case CameraMovement::LEFT:
                Position -= Right * velocity;
                break;
            case CameraMovement::RIGHT:
                Position += Right * velocity;
                break;
            case CameraMovement::LOOKUP:
                Pitch += mouseDiff;
                break;
            case CameraMovement::LOOKDOWN:
                Pitch -= mouseDiff;
                break;
            case CameraMovement::LOOKLEFT:
                Yaw -= mouseDiff;
                break;
            case CameraMovement::LOOKRIGHT:
                Yaw += mouseDiff;
                break;
        }

        updateCameraVectors();
    }

    void ProcessMouseMovement(float xOffset, float yOffset, bool constrainPitch = true)
    {
        xOffset *= MouseSensitivity;
        yOffset *= MouseSensitivity;

        Yaw += xOffset;
        Pitch += yOffset;

        if (constrainPitch)
        {
            Pitch = (Pitch > 89.0f) ? 89.0f : Pitch;
            Pitch = (Pitch < -89.0f) ? -89.0f : Pitch;
        }

        updateCameraVectors();
    }

    void ProcessMouseScroll(float yOffset)
    {
        Zoom -= (float)yOffset;
        Zoom = (Zoom < 1.0f) ? 1.0f : Zoom;
        Zoom = (Zoom > 45.0f) ? 45.0f : Zoom;
    }

  private:
    void updateCameraVectors()
    {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front   = glm::normalize(front);

        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up    = glm::normalize(glm::cross(Right, Front));
    }
};
}  // namespace hatgpu
#endif