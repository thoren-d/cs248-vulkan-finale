#version 450

// Push Constants (view data)
layout(push_constant) uniform View {
    mat4 view_proj;
} view;

// Vertex data
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_texcoord;

// Instance data
layout(location = 4) in mat4 in_obj2world;
layout(location = 8) in mat3 in_obj2world_normal;

void main() {
    gl_Position = view.view_proj * (in_obj2world * vec4(in_position, 1.0));
}
