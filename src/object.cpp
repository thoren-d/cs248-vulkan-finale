#include "object.h"

#include <glm/gtc/matrix_transform.hpp>

InstanceData Object::GetInstanceData() {
    glm::mat4 obj2world = glm::translate(glm::mat4_cast(rotation_) * glm::scale(glm::identity<glm::mat4>(), scale_), position_);
    glm::mat4 obj2world_normal = glm::transpose(glm::inverse(obj2world));

    return InstanceData {
        obj2world,
        obj2world_normal
    };
}
