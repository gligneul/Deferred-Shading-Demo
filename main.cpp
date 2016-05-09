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

#include "Manipulator.h"
#include "ShaderProgram.h"
#include "Texture2D.h"
#include "UniformBuffer.h"
#include "VertexArray.h"

// Constants
static const int kN = 64;
static const int kM = 64;

// Window size
static int window_w = 1920;
static int window_h = 1080;

// Helpers
static Manipulator manipulator;
static ShaderProgram shader;
static UniforBuffer materials;
static VertexArray lamp_base;
static VertexArray lamp_body;
static VertexArray lamp_light;
static VertexArray ground;

// Matrices
static glm::mat4 model;
static glm::mat4 model_invt;
static glm::mat4 view;
static glm::mat4 projection;
static glm::mat4 mvp;
static glm::vec3 eye_pos;

// Configurable variables
static glm::vec3 eye(0.0, 5.0, 15.0);
static glm::vec3 center(0.0, 5.0, 0.0);
static glm::vec3 up(0.0, 1.0, 0.0);
static glm::vec4 light(0.6, 9.5, 0.0, 1.0);
static glm::vec3 diffuse(0.7, 0.7, 0.7);
static glm::vec3 ambient(0.2, 0.2, 0.2);
static glm::vec3 specular(1.0, 1.0, 1.0);

enum MaterialID {
    MATERIAL_METAL,
    MATERIAL_LAMP,
    MATERIAL_GROUND
};

// Verifies the condition, if it fails, shows the error message and
// exits the program
#define Assert(condition, format, ...) { \
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
        Assert(false, "%s", e.what());
    }
}

// Loads the materials
static void LoadMaterials() {
    materials.Init();

    materials.Add({0.2, 0.2, 0.2});
    materials.Add({0.1, 0.1, 0.1});
    materials.Add({0.7, 0.7, 0.7});
    materials.Add(64.0f);
    materials.FinishChunk();

    materials.Add({0.0, 0.0, 0.0});
    materials.Add({10., 10., 10.});
    materials.Add({0.0, 0.0, 0.0});
    materials.Add(0.0f);
    materials.FinishChunk();

    materials.Add({0.46, 0.54, 0.23});
    materials.Add({0.23, 0.27, 0.12});
    materials.Add({0.1, 0.1, 0.1});
    materials.Add(16.0f);
    materials.FinishChunk();

    materials.SendToDevice();
}

// Loads the quad that represents the floor
static void LoadGround() {
    unsigned int indices[] = {0, 1, 2, 3};
    float h = -0.1;
    float v = 1e5;
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
    Assert(err.empty() && ret, "tinyobj error: %s", err.c_str());

    LoadMesh(&lamp_base, &shapes[0].mesh);
    LoadMesh(&lamp_light, &shapes[1].mesh);
    LoadMesh(&lamp_body, &shapes[2].mesh);
}

// Updates the variables that depend on the model, view and projection
static void UpdateMatrices() {
    manipulator.SetReferencePoint(center.x, center.y, center.z);
    model_invt = glm::transpose(glm::inverse(model));
    view = glm::lookAt(eye, center, up);
    view *= manipulator.GetMatrix(glm::normalize(center - eye));
    auto ratio = (float)window_w / (float)window_h;
    projection = glm::perspective(glm::radians(60.0f), ratio, 1.0f, 100.0f);
    mvp = projection * view * model;
    auto view_inv = glm::inverse(view);
    eye_pos = glm::vec3(view_inv * glm::vec4());
}

// Loads the global opengl configuration
static void LoadGlobalConfiguration() {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02, 0.01, 0.10, 1);
    glfwWindowHint(GLFW_SAMPLES, 8);
    glEnable(GL_MULTISAMPLE);
}

// Loads the shader's uniform variables
static void LoadShaderVariables() {
    shader.SetUniform("mvp", mvp);
    shader.SetUniform("model", model);
    shader.SetUniform("model_invt", model_invt);
    shader.SetUniform("light_pos", light);
    shader.SetUniform("eye_pos", eye_pos);
    shader.SetUniform("l_diffuse", diffuse);
    shader.SetUniform("l_ambient", ambient);
    shader.SetUniform("l_specular", specular);
    shader.SetUniformBuffer("MaterialsBlock", 0, materials.GetId());
}

// Display callback, renders the sphere
static void Display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader.Enable();
    LoadShaderVariables();

    shader.SetUniform("material_id", MATERIAL_GROUND);
    ground.DrawElements(GL_QUADS);
    shader.SetUniform("material_id", MATERIAL_METAL);
    lamp_base.DrawElements(GL_TRIANGLES);
    lamp_body.DrawElements(GL_TRIANGLES);
    shader.SetUniform("material_id", MATERIAL_LAMP);
    lamp_light.DrawElements(GL_TRIANGLES);

    shader.Disable();
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
    UpdateMatrices();
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
    auto manip_button = (button == GLFW_MOUSE_BUTTON_LEFT) ? 0 :
                        (button == GLFW_MOUSE_BUTTON_RIGHT) ? 1 : -1;
    auto pressed = action == GLFW_PRESS;
    manipulator.MouseClick(manip_button, pressed, (int)x, (int)y);
    UpdateMatrices();
}

// Motion callback
static void Motion(GLFWwindow *window, double x, double y) {
    manipulator.MouseMotion((int)x, (int)y);
    UpdateMatrices();
}

// Initialization
int main(int argc, char *argv[]) {
    // Init glfw
    Assert(glfwInit(), "glfw init failed", 0);
    auto window = glfwCreateWindow(window_w, window_h, "OpenGL4 Application",
            nullptr, nullptr);
    Assert(window, "glfw window couldn't be created", 0);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, Keyboard);
    glfwSetMouseButtonCallback(window, Mouse);
    glfwSetCursorPosCallback(window, Motion);

    // Init glew
    auto glew_error = glewInit();
    Assert(!glew_error, "GLEW error: %s", glewGetErrorString(glew_error));

    // Init application
    LoadGlobalConfiguration();
    LoadMaterials();
    LoadGround();
    LoadStreetLamp();
    LoadShader();
    UpdateMatrices();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        Reshape(window);
        Display();
        glfwSwapBuffers(window);
        glfwPollEvents();
    };

    glfwTerminate();
    return 0;
}

