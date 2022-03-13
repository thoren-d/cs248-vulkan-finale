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
layout (set=0, binding=3) uniform sampler2D irradiance_map;

layout (set=1, binding=0) uniform Material {
    float ior;
    float roughness;
    float metalness;
} material;
layout (set=1, binding=1) uniform sampler2D diffuse_map;
layout (set=1, binding=2) uniform sampler2D normal_map;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;
layout(location = 3) in mat3 in_tan2world;

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

vec3 SampleSpherical(sampler2D tex, vec3 D) {
    float dx = length(dFdx(D));
    float dy = length(dFdy(D));

    // angle between two points on a unit circle that are dx apart.
    float du = acos(0.5 * (2.0 - dx*dx));
    float dv = acos(0.5 * (2.0 - dy*dy));

    ivec2 tex_size = textureSize(tex, 0);
    float lod = log2(max(1.0, max(du * tex_size.x, dv * tex_size.y)));

    return textureLod(tex, VectorToSpherical(D), lod).rgb;
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

    vec3 normal_map_value = 2.0 * texture(normal_map, in_texcoord).xyz - vec3(1.0);
    vec3 N = normalize(in_tan2world * normal_map_value);
    vec3 V = normalize(scene.camera_position - in_position);
    vec3 R = normalize(reflect(-V, N));


    {
        // Environment Lighting
        // vec3 intensity = textureLod(environment_map, VectorToSpherical(R), 0.0).rgb;
        vec3 intensity = SampleSpherical(environment_map, R);
        float lambert = max(dot(R, N), 0.0);
        float fresnel = Fresnel(V, N) * max(0.0, 1 - 2 * material.roughness);

        radiance +=  lambert * fresnel * specular_color * intensity;

        radiance += diffuse_color * texture(irradiance_map, VectorToSpherical(N)).rgb * (1.0 / PI);
    }

    for (int i = 0; i < NUM_LIGHTS; i++) {
        // Spot light calculations
        vec3 intensity = scene.lights[i].intensity;
        vec3 light_pos = scene.lights[i].position;
        float cone_angle = scene.lights[i].direction_angle.w;
        
        vec3 dir_to_surface = in_position - light_pos;
        float angle = acos(dot(normalize(dir_to_surface), scene.lights[i].direction_angle.xyz));

        float min_angle = cone_angle * (1.0 - kSmoothing);
        float max_angle = cone_angle * (1.0 + kSmoothing);
        float spot_attenuation = 1.0 - smoothstep(min_angle, max_angle, angle);

        float r_squared = dot(dir_to_surface, dir_to_surface);
        float distance_attenuation = 1.0 / (1.0 + r_squared);

        intensity *= spot_attenuation * distance_attenuation;

        // Shadow visibility calculation
        const float pcf_step_size = 2048;
        float visibility = 0.0;
        vec4 light_space_ndc = scene.lights[i].world2light * vec4(in_position, 1.0);
        vec3 shadow_map_pos = light_space_ndc.xyz / light_space_ndc.w;
        vec2 shadow_map_uv = (shadow_map_pos.xy + 1.0) * 0.5;
        for (int j = -2; j <= 2; j++) {
            for (int k = -2; k <= 2; k++) {
                vec2 offset = vec2(j,k) / pcf_step_size;
                float depth = texture(shadow_maps[i], shadow_map_uv + offset).x + 0.0005;
                if (depth > shadow_map_pos.z) {
                    visibility += 1.0;
                }
            }
        }
        visibility = visibility / 25.0;

        vec3 L = normalize(light_pos - in_position);
        radiance += visibility * intensity * BRDF(L, V, N, R, diffuse_color, specular_color);
    }

    outColor = vec4(radiance, 1.0);
}
