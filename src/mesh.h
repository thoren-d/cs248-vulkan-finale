#ifndef MESH_H_
#define MESH_H_

#include <array>
#include <string>

#include <vulkan/vulkan.hpp>

#include "resource_manager.h"

class Mesh {
public:
    Mesh();
    Mesh(const std::string& filename);
    ~Mesh();

    vk::Buffer vertex_buffer() {
        return vertex_buffer_.buffer;
    }

    size_t vertex_count() {
        return vertex_count_;
    }

private:
    ResourceManager::Buffer vertex_buffer_;
    size_t vertex_count_;
};

#endif // MESH_H_
