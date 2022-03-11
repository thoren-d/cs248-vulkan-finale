#version 450

#define NUM_LIGHTS 3

layout (set=0, binding=0) uniform Scene {
    vec3 camera_position;
    vec3 light_positions[NUM_LIGHTS];
    vec4 light_directions_angles[NUM_LIGHTS];
    vec3 light_intensities[NUM_LIGHTS];
} scene;
layout (set=0, binding=1) uniform sampler2D environment_map;

layout (set=1, binding=0) uniform Material {
    float ior;
    float roughness;
    float metalness;
} material;
layout (set=1, binding=1) uniform sampler2D diffuse_map;
layout (set=1, binding=2) uniform sampler2D normal_map;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec4 outColor;

const float kSmoothing = 0.1;

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

// Schlick's Approximation for fresnel factor
// https://en.wikipedia.org/wiki/Schlick%27s_approximation
float Fresnel(vec3 V, vec3 N) {
    float cos_theta = clamp(dot(V, N), 0, 1);
    float n1 = 1.0; // Air IOR
    float n2 = material.ior; // Material IOR
    float r0 = (n1 - n2) / (n1 + n2);
    r0 *= r0;
    return r0 + (1 - r0) * pow(1 - cos_theta, 5);
}

// https://en.wikipedia.org/wiki/Oren%E2%80%93Nayar_reflectance_model
float OrenNayar(vec3 L, vec3 V, vec3 N) {
    float sigma = material.roughness / 3.0;
    float A = 1 - 0.5 * (sigma*sigma) / (sigma*sigma + 0.33);
    float B = 0.45 * (sigma*sigma) / (sigma*sigma + 0.09);
    float theta_i = acos(abs(dot(L, N)));
    float theta_r = acos(abs(dot(V, N)));
    float cos_delta_phi = dot(L - N * dot(N,L), V - N * dot(N,V));
    float alpha = max(theta_i, theta_r);
    float beta = min(theta_i, theta_r);

    return (A + B * max(0, cos_delta_phi) * sin(alpha) * tan(beta));
}

float Beckmann(vec3 N, vec3 H) {
    float m = max(material.roughness, 0.0001);
    float cos_alpha = max(dot(N, H), 0.0001);
    float tan_stuff = (1.0 - cos_alpha*cos_alpha) / (cos_alpha*cos_alpha * m*m);
    float denom = clamp(PI * m*m * pow(cos_alpha, 4.0), 0.0001, 1.0 / 0.0001);
    return exp(-tan_stuff) / denom;
}

// https://en.wikipedia.org/wiki/Specular_highlight#Cook%E2%80%93Torrance_model
float CookTorrance(vec3 N, vec3 H, vec3 V, vec3 L) {
    float D = Beckmann(N, H);
    float F = Fresnel(V, N);
    float h_dot_n = dot(H, N);
    float v_dot_n = dot(V, N);
    float l_dot_n = dot(L, N);
    float v_dot_h = dot(V, H);
    float G = min(1, min(2 * h_dot_n * v_dot_n / v_dot_h, 2 * h_dot_n * l_dot_n / v_dot_h));
    return (D * F * G) / (PI * v_dot_n * l_dot_n);
}

vec3 BRDF(vec3 L, vec3 V, vec3 N, vec3 R, vec3 diffuse_color, vec3 specular_color) {
    vec3 H = normalize(V + L);

    float lambert = max(dot(N, L), 0.0);
    float diffuse_intensity = (1 - material.metalness) * OrenNayar(L, V, N);
    float specular_intensity = CookTorrance(N, H, V, L);

    return lambert * diffuse_intensity * diffuse_color + lambert * specular_intensity * specular_color;
}

void main() {
    vec3 radiance = vec3(0);

    // vec3 diffuse_color = vec3(255.0, 216.0, 0.0) / 255.0;
    vec3 diffuse_color = texture(diffuse_map, in_texcoord).rgb;
    vec3 specular_color = material.metalness * diffuse_color + (1.0 - material.metalness) * vec3(1.0);

    vec3 V = normalize(scene.camera_position - in_position);
    vec3 N = normalize(in_normal);
    vec3 R = normalize(reflect(-V, N));

    {
        // Environment Lighting
        vec3 intensity = texture(environment_map, VectorToSpherical(R)).rgb;
        float lambert = max(dot(R, N), 0.0);
        float fresnel = Fresnel(V, N);
        radiance +=  lambert * fresnel * specular_color * intensity;
    }

    for (int i = 0; i < NUM_LIGHTS; i++) {
        // Spot light calculations
        vec3 intensity = scene.light_intensities[i];
        vec3 light_pos = scene.light_positions[i];
        float cone_angle = scene.light_directions_angles[i].w;
        
        vec3 dir_to_surface = in_position - light_pos;
        float angle = acos(dot(normalize(dir_to_surface), scene.light_directions_angles[i].xyz));

        float min_angle = cone_angle * (1.0 - kSmoothing);
        float max_angle = cone_angle * (1.0 + kSmoothing);
        float spot_attenuation = 1.0 - smoothstep(min_angle, max_angle, angle);

        float r_squared = dot(dir_to_surface, dir_to_surface);
        float distance_attenuation = 1.0 / (1.0 + r_squared);

        intensity *= spot_attenuation * distance_attenuation;

        vec3 L = normalize(scene.light_positions[i] - in_position);
        radiance += intensity * BRDF(L, V, N, R, diffuse_color, specular_color);
    }

    outColor = vec4(radiance, 1.0);
}
