#version 450

#define NUM_LIGHTS 3

struct Light {
    mat4 world2light;
    vec4 direction_angle;
    vec3 intensity;
    vec3 position;
};

layout (set=0, binding=0) uniform Scene {
    vec3 camera_position;
    Light lights[NUM_LIGHTS];
} scene;

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

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_texcoord;
layout(location = 3) out mat3 out_tan2world;

void main() {
    vec4 world_position = in_obj2world * vec4(in_position, 1.0);
    
    gl_Position = view.view_proj * world_position;
    out_position = world_position.xyz;
    out_normal = in_obj2world_normal * in_normal;
    out_texcoord = in_texcoord;

    vec3 tangent = normalize(in_obj2world_normal * in_tangent);
    vec3 bitangent = normalize(cross(out_normal, tangent));
    vec3 normal = normalize(cross(tangent, bitangent));
    out_tan2world = mat3(tangent, bitangent, normal);
}
