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
layout (set=0, binding=1) uniform sampler2D environment_map;
layout (set=0, binding=2) uniform sampler2D shadow_maps[NUM_LIGHTS];

layout(location = 0) in vec3 in_direction;

#define PI 3.14159265358979323846
vec2 VectorToSpherical(vec3 D) {
    float theta = acos(dot(D, vec3(0, 1, 0)));  // 0 to PI, with 0 meaning up and PI meaining down
    float y = dot(D, vec3(1, 0, 0));
    float x = dot(D, vec3(0, 0, 1));
    float phi = atan(y, x);
    if (phi < 0.0) {
        phi = (2.0*PI) + phi;
    }

    float u = phi / (2.0*PI);
    float v = theta / PI;

    return vec2(u, v);
}

layout(location = 0) out vec4 out_color;

void main() {
    out_color = texture(environment_map, VectorToSpherical(normalize(in_direction)));
}

