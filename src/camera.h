#ifndef CAMERA_H_
#define CAMERA_H_

#include <glm/glm.hpp>

class Camera {
public:
    Camera();
    ~Camera();

    glm::vec3 look_at;
    glm::vec3 position;

    glm::mat4 GetViewProj();

    void RotateBy(float phi, float theta);

private:
    float phi_ = 1.0f;
    float theta_ = 0.0f;
};

#endif  // CAMERA_H_
