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

#include <cmath>
#include <cstdio>
#include <vector>
#include <iostream>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <lodepng.h>
#include <tiny_obj_loader.h>

#include "ShaderProgram.h"
#include "Texture2D.h"
#include "UniformBuffer.h"
#include "VertexArray.h"

// Materials
enum MaterialID {
    MATERIAL_METAL,
    MATERIAL_LAMP,
    MATERIAL_GROUND
};

// Scene configuration constants
static const int I_OFFSET = 10;
static const int J_OFFSET = 10;
static const int N_LAMPS_I = 10;
static const int N_LAMPS_J = 10;
static const int N_LAMPS = N_LAMPS_I * N_LAMPS_J;

// Window size
static int window_w = 1920;
static int window_h = 1080;

// Global Helpers
static ShaderProgram shader;
static UniformBuffer materials;
static UniformBuffer lights;

// Lamp mesh
static UniformBuffer lamp_matrices;
static VertexArray lamp_base;
static VertexArray lamp_body;
static VertexArray lamp_light;

// Ground quad
static UniformBuffer ground_matrices;
static VertexArray ground;

// Global matrices
static glm::mat4 rotation;
static glm::mat4 view;
static glm::mat4 projection;

// Random colors
static glm::vec3 random_colors[N_LAMPS];

// Camera config
#if 1
static glm::vec3 eye(0.0, 100.0, 0.0);
static glm::vec3 center(0.0, 0.0, 0.0);
static glm::vec3 up(0.0, 0.0, 1.0);
#else
static glm::vec3 eye(-10.0, 20.0, -10.0);
static glm::vec3 center(0, 0, 0);
static glm::vec3 up(0.0, 1.0, 0.0);
#endif

// Verifies the condition, if it fails, shows the error message and
// exits the program
#define Assert(condition, message) Assertf(condition, message, 0)
#define Assertf(condition, format, ...) { \
    if (!condition) { \
        auto finalformat = std::string("Error at function %s: ") \
                + format + "\n"; \
        fprintf(stderr, finalformat.c_str(), __func__, __VA_ARGS__); \
        exit(1); \
    } \
}

// Loads the shader
static void LoadShader() {
    try {
        shader.LoadVertexShader("shaders/deferred1_vs.glsl");
        shader.LoadFragmentShader("shaders/deferred1_fs.glsl");
        shader.LinkShader();
    } catch (std::exception& e) {
        Assertf(false, "%s", e.what());
    }
}

// Creates an random number between 0 an 1
static double Random() {
    return (double)rand() / RAND_MAX;
}

// Creates the random colors
static void CreateRandomColors() {
    for (int i = 0; i < N_LAMPS; ++i)
        random_colors[i] = glm::vec3(Random(), Random(), Random());
}

// Loads the materials
static void CreateMaterials() {
    // Buffer configuration
    // struct Material {
    //     vec3 diffuse;
    //     vec3 ambient;
    //     vec3 specular;
    //     float shininess;
    // };
    // 
    // layout (std140) uniform MaterialsBlock {
    //     Material materials[8];
    // };

    materials.Init();

    // MATERIAL_METAL
    materials.Add({0.2, 0.2, 0.2});
    materials.Add({0.1, 0.1, 0.1});
    materials.Add({0.7, 0.7, 0.7});
    materials.Add(64.0f);
    materials.FinishChunk();

    // MATERIAL_LAMP
    materials.Add({0.0, 0.0, 0.0});
    materials.Add({3.0, 3.0, 2.9});
    materials.Add({0.0, 0.0, 0.0});
    materials.Add(0.0f);
    materials.FinishChunk();

    // MATERIAL_GROUND
    materials.Add({0.70, 0.70, 0.70});
    materials.Add({0.20, 0.20, 0.20});
    materials.Add({0.10, 0.10, 0.10});
    materials.Add(16.0f);
    materials.FinishChunk();

    materials.SendToDevice();
}

// Compute the light translation given the i, j indices
static glm::mat4 ComputeTranslation(int i, int j) {
    auto x = (i - (N_LAMPS_I - 1) / 2.0) * I_OFFSET;
    auto z = (j - (N_LAMPS_J - 1) / 2.0) * J_OFFSET;
    return glm::translate(glm::vec3(x, 0, z));
}

// Updates the lights buffer
static void UpdateLightsBuffer() {
    // Buffer configuration
    // struct Light {
    //     vec4 position;
    //     vec3 diffuse;
    //     vec3 specular;
    //     bool is_spot;
    //     vec3 spot_direction;
    //     float spot_cutoff;
    //     float spot_exponent;
    // };
    // 
    // layout (std140) uniform LightsBlock {
    //     vec3 global_ambient;
    //     int n_lights;
    //     Light lights[100];
    // };

    auto position = glm::vec4(0.6, 10, 0.0, 1.0);
    auto specular = glm::vec3(0.5, 0.5, 0.5);
    auto is_spot = true;
    auto spot_direction = glm::vec3(0.0, -1.0, 0.0);
    auto spot_cutoff = glm::radians(45.0f);
    auto spot_exponent = 16.0f;

    if (!lights.GetId())
        lights.Init();
    else
        lights.Clear();

    lights.Add({0.2, 0.2, 0.2});
    lights.Add(N_LAMPS);
    lights.FinishChunk();

    for (int i = 0; i < N_LAMPS_I; ++i) {
        for (int j = 0; j < N_LAMPS_J; ++j) {
            auto model = rotation * ComputeTranslation(i, j);
            auto modelview = view * model;
            auto normalmatrix = glm::transpose(glm::inverse(modelview));
            auto spot_dir_ws = glm::vec4(spot_direction, 1);
            auto spot_dir_vs = glm::normalize(
                    glm::vec3(normalmatrix * spot_dir_ws));
            lights.Add(modelview * position);
            lights.Add(random_colors[i + N_LAMPS_I * j]);
            lights.Add(specular);
            lights.Add(is_spot);
            lights.Add(spot_dir_vs);
            lights.Add(spot_cutoff);
            lights.Add(spot_exponent);
            lights.FinishChunk();
        }
    }

    lights.SendToDevice();
}

// Loads the quad that represents the floor
static void LoadGround() {
    unsigned int indices[] = {0, 1, 2, 3};
    float h = -0.1;
    float v = 100;
    float vertices[] = {
        -v, h, v,
        -v, h, -v,
        v, h, -v,
        v, h, v
    };
    float normals[] = {
        0, 1, 0,
        0, 1, 0,
        0, 1, 0,
        0, 1, 0
    };
    ground.Init();
    ground.SetElementArray(indices, 4);
    ground.AddArray(0, vertices, 12, 3);
    ground.AddArray(1, normals, 12, 3);
}

// Loads a single mesh into the gpu
static void LoadMesh(VertexArray* vao, tinyobj::mesh_t *mesh) {
    vao->Init();
    vao->SetElementArray(mesh->indices.data(), mesh->indices.size());
    vao->AddArray(0, mesh->positions.data(), mesh->positions.size(), 3);
    vao->AddArray(1, mesh->normals.data(), mesh->positions.size(), 3);
}

// Loads the street lamp
static void LoadStreetLamp() {
    auto inputfile = "data/street_lamp.obj";
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string err;
    bool ret = tinyobj::LoadObj(shapes, materials, err, inputfile, "data/");
    Assertf(err.empty() && ret, "tinyobj error: %s", err.c_str());

    LoadMesh(&lamp_base, &shapes[0].mesh);
    LoadMesh(&lamp_light, &shapes[1].mesh);
    LoadMesh(&lamp_body, &shapes[2].mesh);
}

// Creates the lamps instances
void UpdateLampMatrices() {
    // Buffer configuration:
    // struct Matrices {
    //     mat4 mvp;
    //     mat4 modelview;
    //     mat4 normalmatrix;
    // };
    // 
    // layout (std140) uniform MatricesBlock {
    //     Matrices matrices[100];
    // };

    if (!lamp_matrices.GetId())
        lamp_matrices.Init();
    else
        lamp_matrices.Clear();

    for (int i = 0; i < N_LAMPS_I; ++i) {
        for (int j = 0; j < N_LAMPS_J; ++j) {
            auto model = ComputeTranslation(i, j);
            auto modelview = view * model;
            auto normalmatrix = glm::transpose(glm::inverse(modelview));
            auto mvp = projection * modelview;
            lamp_matrices.Add(mvp);
            lamp_matrices.Add(modelview);
            lamp_matrices.Add(normalmatrix);
        }
    }

    lamp_matrices.SendToDevice();
}

// Creates a single ground instance
void UpdateGroundMatrices() {
    // Buffer configuration:
    // struct Matrices {
    //     mat4 mvp;
    //     mat4 modelview;
    //     mat4 normalmatrix;
    // };
    // 
    // layout (std140) uniform MatricesBlock {
    //     Matrices matrices[100];
    // };

    if (!ground_matrices.GetId())
        ground_matrices.Init();
    else
        ground_matrices.Clear();

    auto model = glm::mat4();
    auto modelview = view * model;
    auto normalmatrix = glm::transpose(glm::inverse(modelview));
    auto mvp = projection * modelview;

    ground_matrices.Add(mvp);
    ground_matrices.Add(modelview);
    ground_matrices.Add(normalmatrix);

    ground_matrices.SendToDevice();
}

// Updates the variables that depend on the model, view and projection
static void UpdateMatrices() {
    view = glm::lookAt(eye, center, up);

    auto ratio = (float)window_w / (float)window_h;
    projection = glm::perspective(glm::radians(60.0f), ratio, 1.5f, 300.0f);

    UpdateLampMatrices();
    UpdateGroundMatrices();
}

// Loads the global opengl configuration
static void LoadGlobalConfiguration() {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02, 0.01, 0.10, 1);

    glfwWindowHint(GLFW_SAMPLES, 8);
    glEnable(GL_MULTISAMPLE);
}

// Display callback, renders the sphere
static void Display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.Enable();
    UpdateLightsBuffer();
    shader.SetUniformBuffer("MaterialsBlock", 0, materials.GetId());
    shader.SetUniformBuffer("LightsBlock", 1, lights.GetId());

    shader.SetUniformBuffer("MatricesBlock", 2, ground_matrices.GetId());
    shader.SetUniform("material_id", MATERIAL_GROUND);
    ground.DrawElements(GL_QUADS);

    shader.SetUniformBuffer("MatricesBlock", 2, lamp_matrices.GetId());
    shader.SetUniform("material_id", MATERIAL_METAL);
    lamp_base.DrawInstances(GL_TRIANGLES, N_LAMPS);
    lamp_body.DrawInstances(GL_TRIANGLES, N_LAMPS);
    shader.SetUniform("material_id", MATERIAL_LAMP);
    lamp_light.DrawInstances(GL_TRIANGLES, N_LAMPS);

    shader.Disable();
}

// Measures the frames per second (and prints in the terminal)
static void ComputeFPS() {
    static double last = glfwGetTime();
    static int frames = 0;
    double curr = glfwGetTime();
    if (curr - last > 1.0) {
        printf("fps: %d\n", frames);
        last += 1.0;
        frames = 0;
    } else {
        frames++;
    }
}

// Resize callback
static void Reshape(GLFWwindow *window) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if (width == window_w && height == window_h)
        return;

    window_w = width;
    window_h = height;
    glViewport(0, 0, width, height);
}

// Called each frame
static void Idle() {
    rotation = glm::rotate(rotation, glm::radians(0.1f), glm::vec3(0, 1, 0));
}

// Keyboard callback
static void Keyboard(GLFWwindow* window, int key, int scancode, int action,
                     int mods) {
    if (action != GLFW_PRESS)
        return;

    switch (key) {
        case GLFW_KEY_Q:
            exit(0);
            break;
        default:
            break;
    }
}

// Mouse Callback
static void Mouse(GLFWwindow *window, int button, int action, int mods) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    UpdateMatrices();
}

// Motion callback
static void Motion(GLFWwindow *window, double x, double y) {
    UpdateMatrices();
}

// Initialization
int main(int argc, char *argv[]) {
    // Init glfw
    Assert(glfwInit(), "glfw init failed");
    auto window = glfwCreateWindow(window_w, window_h, "OpenGL4 Application",
            nullptr, nullptr);
    Assert(window, "glfw window couldn't be created");
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, Keyboard);
    glfwSetMouseButtonCallback(window, Mouse);
    glfwSetCursorPosCallback(window, Motion);

    // Init glew
    auto glew_error = glewInit();
    Assertf(!glew_error, "GLEW error: %s", glewGetErrorString(glew_error));

    // Init application
    LoadGlobalConfiguration();
    CreateRandomColors();
    CreateMaterials();
    LoadGround();
    LoadStreetLamp();
    LoadShader();
    UpdateMatrices();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        Reshape(window);
        Idle();
        UpdateMatrices();
        Display();
        ComputeFPS();
        glfwSwapBuffers(window);
        glfwPollEvents();
    };

    glfwTerminate();
    return 0;
}

