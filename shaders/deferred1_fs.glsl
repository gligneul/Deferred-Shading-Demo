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

uniform vec3 diffuse;
uniform vec3 ambient;
uniform vec3 specular;
uniform float shininess;

uniform int material_id;
uniform vec4 light_pos;
uniform vec3 eye_pos;

in vec3 frag_position;
in vec3 frag_normal;

out vec3 frag_color;

vec3 compute_diffuse(vec3 normal, vec3 light_dir) {
    return diffuse * max(dot(normal, light_dir), 0);
}

vec3 compute_specular(vec3 normal, vec3 light_dir, vec3 half_vector) {
    if (dot(normal, light_dir) > 0)
        return specular * pow(max(dot(normal, half_vector), 0), shininess);
    else
        return vec3(0, 0, 0);
}

void main() {
    vec3 normal = normalize(frag_normal);
    vec3 eye_dir = normalize(eye_pos - frag_position);
    vec3 light_dir = normalize(light_pos.xyz / light_pos.w - frag_position);
    vec3 half_vector = normalize(light_dir + eye_dir);

    vec3 diffuse = compute_diffuse(normal, light_dir);
    vec3 specular = compute_specular(normal, light_dir, half_vector);

    float gray = float(material_id + 1) / 5.0;
    vec3 color = vec3(gray, gray, gray);

    frag_color = (diffuse + ambient) * color + specular;
}

