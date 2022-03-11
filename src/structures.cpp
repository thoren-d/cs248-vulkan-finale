#include "structures.h"

std::array<vk::VertexInputBindingDescription, 2>
GetVertexInputBindingDescriptions() {
  vk::VertexInputBindingDescription vertex_description;
  vertex_description.setBinding(0)
      .setStride(sizeof(Vertex))
      .setInputRate(vk::VertexInputRate::eVertex);

  auto instance_description = vk::VertexInputBindingDescription()
    .setBinding(1)
    .setStride(sizeof(InstanceData))
    .setInputRate(vk::VertexInputRate::eInstance);

  return {vertex_description, instance_description};
}

std::array<vk::VertexInputAttributeDescription, 12>
GetVertexInputAttributeDescriptions() {
  std::array<vk::VertexInputAttributeDescription, 12> result = {};
  result[0]
      .setBinding(0)
      .setLocation(0)
      .setOffset(offsetof(Vertex, position))
      .setFormat(vk::Format::eR32G32B32Sfloat);
  result[1]
      .setBinding(0)
      .setLocation(1)
      .setOffset(offsetof(Vertex, normal))
      .setFormat(vk::Format::eR32G32B32Sfloat);
  result[2]
      .setBinding(0)
      .setLocation(2)
      .setOffset(offsetof(Vertex, tangent))
      .setFormat(vk::Format::eR32G32B32Sfloat);
  result[3]
      .setBinding(0)
      .setLocation(3)
      .setOffset(offsetof(Vertex, texcoord))
      .setFormat(vk::Format::eR32G32Sfloat);
  // Two mat4s
  for (uint32_t i = 0; i < 8; i++) {
      result[i+4]
        .setBinding(1)
        .setLocation(i+4)
        .setOffset(sizeof(glm::vec4) * i)
        .setFormat(vk::Format::eR32G32B32A32Sfloat);
  }
  
  return result;
}
