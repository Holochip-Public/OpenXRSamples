//
// Created by Steven Winston on 3/22/20.
//

#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

void Camera::updateViewMatrix()
{
    glm::mat4x4 rotM(1.0f);
    glm::mat4x4 transM;

    rotM = glm::rotate( rotM, glm::radians(rotation.x), {1.0f, 0.0f, 0.0f} );
    rotM = glm::rotate( rotM, glm::radians(rotation.y), {0.0f, 1.0f, 0.0f} );
    rotM = glm::rotate( rotM, glm::radians(rotation.z), {0.0f, 0.0f, 1.0f} );

    transM = glm::translate(glm::mat4(1.0f), {position.x, position.y, position.z });

    matrices.view = (CameraType::firstperson == type)?rotM * transM : transM * rotM;
    updated = true;
}

bool Camera::moving() {
    return keys.left || keys.right || keys.up || keys.down;
}

void Camera::setPerspective(float fov_, float aspect, float znear_, float zfar_) {
    fov = fov_;
    znear = znear_;
    zfar = zfar_;
    matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
}

void Camera::updateAspectRatio(float aspect) {
    setPerspective(fov, aspect, znear, zfar);
}

void Camera::setPosition(glm::vec3 position_) {
    position = { position_.x, position_.y, position_.z, 1.0f };
    updateViewMatrix();
}

void Camera::setRotation(glm::vec3 rotation_) {
    rotation = rotation_;
    updateViewMatrix();
}

void Camera::rotate(glm::vec3 delta) {
    rotation += delta;
    updateViewMatrix();
}

Camera::Camera()
: type(lookat)
, fov(0.0f)
, znear(0.0f)
, zfar(0.0f)
{

}

void Camera::setTranslation(glm::vec3 translation) {
    setPosition(translation);
}

void Camera::translate(glm::vec3 delta) {
    position.x = (position.x + delta.x);
    position.y = (position.y + delta.y);
    position.z = (position.z + delta.z);
    updateViewMatrix();
}

void Camera::update(float deltaTime) {
    updated = false;
    if (type == CameraType::firstperson)
    {
        if (moving())
        {
            glm::vec3 camFront;
            glm::vec3 rotLoc = {
                    glm::radians(rotation.x),
                    glm::radians(rotation.y),
                    glm::radians(rotation.z)
            };
            camFront.x = (-glm::cos(rotLoc.x) * glm::sin(rotLoc.y));
            camFront.y = (glm::sin(rotLoc.x));
            camFront.z = (glm::cos(rotLoc.x) * glm::cos(rotLoc.y));
            camFront = glm::normalize(camFront);

            float moveSpeed = deltaTime * movementSpeed;

            glm::vec3 tPos = { position.x, position.y, position.z };
            if (keys.up)
                tPos += camFront * moveSpeed;
            if (keys.down)
                tPos -= camFront * moveSpeed;
            if (keys.left)
                tPos -= glm::normalize(glm::cross(camFront, {0.0, 1.0, 0.0})) * moveSpeed;
            if (keys.right)
                tPos += glm::normalize(glm::cross(camFront, {0.0f, 1.0f, 0.0f})) * moveSpeed;
            position = { tPos.x, tPos.y, tPos.z, position.w };

            updateViewMatrix();
        }
    }
}
