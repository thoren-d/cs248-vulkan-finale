#ifndef STRUCTURES_H_
#define STRUCTURES_H_

#include <array>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

struct Vertex {
  Vertex(glm::vec3 pos, glm::vec3 norm, glm::vec3 tan, glm::vec2 uv)
      : position(pos), normal(norm), tangent(tan), texcoord(uv) {}
  Vertex(glm::vec3 pos)
      : position(pos), normal(0.0f), tangent(0.0f), texcoord(0.0f) {}

  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 tangent;
  glm::vec2 texcoord;
};

struct InstanceData {
    glm::mat4 obj2world;
    glm::mat3 obj2world_normal;
};

std::array<vk::VertexInputBindingDescription, 2> GetVertexInputBindingDescriptions();
std::array<vk::VertexInputAttributeDescription, 11> GetVertexInputAttributeDescriptions();

#define NUM_LIGHTS 3

struct SceneUniforms {
    alignas(16) glm::vec3 camera_position;
    alignas(16) glm::vec4 light_positions[NUM_LIGHTS];
    alignas(16) glm::vec4 light_directions_angles[NUM_LIGHTS];
    alignas(16) glm::vec4 light_intensities[NUM_LIGHTS];
};

struct PushConstants {
    glm::mat4 view_proj;
};

#endif  // STRUCTURES_H_
