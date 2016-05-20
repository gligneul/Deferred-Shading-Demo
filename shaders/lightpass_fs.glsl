/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2016 Gabriel de Quadros Ligneul
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#version 450

// Geometry pass inputs
uniform sampler2D position_sampler;
uniform sampler2D normal_sampler;
uniform sampler2D material_sampler;

// Lights information
struct Light {
    vec4 position;
    vec3 diffuse;
    vec3 specular;
    bool is_spot;
    vec3 spot_direction;
    float spot_cutoff;
    float spot_exponent;
};

layout (std140) uniform LightsBlock {
    vec3 global_ambient;
    int n_lights;
    Light lights[100];
};

// Materials information
struct Material {
    vec3 diffuse;
    vec3 ambient;
    vec3 specular;
    float shininess;
};

layout (std140) uniform MaterialsBlock {
    Material materials[8];
};

// Background color
const vec3 background = vec3(0.1, 0.1, 0.1);

// Screen texture coordinates
in vec2 frag_textcoord;

// Output color
out vec3 color;

vec3 compute_diffuse(Light L, Material M, vec3 normal, vec3 light_dir) {
    vec3 diffuse = M.diffuse * L.diffuse;
    return diffuse * max(dot(normal, light_dir), 0);
}

vec3 compute_specular(Light L, Material M, vec3 normal, vec3 light_dir,
                      vec3 half_vector) {
    if (dot(normal, light_dir) > 0) {
        vec3 specular = M.specular * L.specular;
        float shininess = M.shininess;
        return specular * pow(max(dot(normal, half_vector), 0), shininess);
    } else {
        return vec3(0, 0, 0);
    }
}

float compute_spot(Light L, vec3 light_dir) {
    if (L.is_spot) {
        float kspot = max(dot(-light_dir, L.spot_direction), 0);
        if (kspot > L.spot_cutoff) {
            return pow(kspot, L.spot_exponent);
        } else {
            return 0;
        }
    } else {
        return 1;
    }
}

vec3 compute_shading(Light L, Material M, vec3 normal, vec3 position) {
    vec3 eye_dir = normalize(-position);
    vec3 light_pos = L.position.xyz / L.position.w;
    vec3 light_dir = normalize(light_pos - position);
    vec3 half_vector = normalize(light_dir + eye_dir);
    vec3 diffuse = compute_diffuse(L, M, normal, light_dir);
    vec3 specular = compute_specular(L, M, normal, light_dir, half_vector);
    float spot_intensity = compute_spot(L, light_dir);
    return spot_intensity * (diffuse + specular);
}

vec3 compute_ambient(Material M) {
    return M.ambient * global_ambient;
}

void main() {
    int material = int(texture(material_sampler, frag_textcoord).x) - 1;
    if (material == -1) {
        color = background;
        return;
    }
    vec3 position = texture(position_sampler, frag_textcoord).xyz;
    vec3 normal = texture(normal_sampler, frag_textcoord).xyz;
    Material M = materials[material];
    vec3 acc_color = vec3(0, 0, 0);
    for (int i = 0; i < n_lights; ++i) {
        Light L = lights[i];
        acc_color += compute_shading(L, M, normal, position);
    }
    vec3 ambient = compute_ambient(M);
    color = acc_color + ambient;
}

