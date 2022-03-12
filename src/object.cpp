#include "object.h"

#include <glm/gtc/matrix_transform.hpp>

InstanceData Object::GetInstanceData() {
    // glm::mat4_cast(rotation_)
    // glm::scale(glm::mat4(1.0f), scale_)
    // glm::translate(glm::mat4(1.0f))
    glm::mat4 obj2world = glm::translate(glm::mat4(1.0f), position_) * glm::mat4_cast(rotation_) * glm::scale(glm::mat4(1.0f), scale_);
    glm::mat3 obj2world_normal = glm::transpose(glm::inverse(glm::mat3(obj2world)));

    return InstanceData {
        obj2world,
        obj2world_normal,
    };
}
