//
// Created by Steven Winston on 3/22/20.
//

#ifndef LIGHTFIELDFORWARDRENDERER_CAMERA_HPP
#define LIGHTFIELDFORWARDRENDERER_CAMERA_HPP


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class Camera
{
private:
    float fov;
    float znear, zfar;

    void updateViewMatrix();
public:
    Camera();
    enum CameraType { lookat, firstperson };
    CameraType type;

    glm::vec3 rotation;
    glm::vec4 position = {0.0f, 0.0f, 0.0f, 1.0f};

    float rotationSpeed = 1.0f;
    float movementSpeed = 1.0f;

    bool updated = false;

    struct
    {
        glm::mat4x4 perspective;
        glm::mat4x4 view;
    } matrices;

    struct
    {
        bool left = false;
        bool right = false;
        bool up = false;
        bool down = false;
    } keys;

    bool moving();

    float getNearClip() { return znear; }
    float getFarClip() { return zfar; }
    void setPerspective(float fov_, float aspect, float znear_, float zfar_);
    void updateAspectRatio(float aspect);
    void setPosition(glm::vec3 position_);
    void setRotation(glm::vec3 rotation_);
    void rotate(glm::vec3 delta);
    void setTranslation(glm::vec3 translation);
    void translate(glm::vec3 delta);

    void update(float deltaTime);
};

#endif //LIGHTFIELDFORWARDRENDERER_CAMERA_HPP
