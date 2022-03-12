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


const vec2 kVertexPositions[6] = {
    vec2(-1,-1),
    vec2(1,-1),
    vec2(-1,1),
    vec2(-1,1),
    vec2(1,-1),
    vec2(1,1),
};


layout(location = 0) out vec3 out_direction;

void main() {
    vec4 pos = vec4(kVertexPositions[gl_VertexIndex], 1.0, 1.0);
    out_direction = (view.view_proj * pos).xyz; 
    gl_Position = pos;
}
