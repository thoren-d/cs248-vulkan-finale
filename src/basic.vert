#version 450

#define NUM_LIGHTS 3

layout (set=0, binding=0) uniform Scene {
    vec3 camera_position;
    vec3 light_positions[NUM_LIGHTS];
    vec4 light_directions_angles[NUM_LIGHTS];
    vec3 light_intensities[NUM_LIGHTS];
} scene;

vec3 color[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

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
