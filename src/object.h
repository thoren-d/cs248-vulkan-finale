#ifndef OBJECT_H_
#define OBJECT_H_

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "material.h"
#include "mesh.h"
#include "structures.h"

class Object {
public:
    Object(Material* material, Mesh* mesh): material_(material), mesh_(mesh) {}
    ~Object() {};

    InstanceData GetInstanceData();

    Material* material() {
        return material_;
    }

    Mesh* mesh() {
        return mesh_;
    }

    glm::vec3& scale() {
        return scale_;
    }

    glm::vec3& position() {
        return position_;
    }

    glm::quat& rotation() {
        return rotation_;
    }

private:
    Material* material_;
    Mesh* mesh_;

    glm::vec3 scale_ = glm::vec3(1.0f);
    glm::quat rotation_ = glm::quat(glm::vec3(0.0f));
    glm::vec3 position_ = glm::vec3(0.0f);
};

#endif // OBJECT_H_
