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
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
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
#include "UniformBuffer.h"
#include "VertexArray.h"
#include "FrameBuffer.h"

// Materials
enum MaterialID { BEAR_MATERIAL, GROUND_MATERIAL };

// Scene configuration constants
const int I_OFFSET = 15;
const int J_OFFSET = 15;
const int N_LIGHTS_I = 10;
const int N_LIGHTS_J = 10;
const int N_LIGHTS = N_LIGHTS_I * N_LIGHTS_J;

// Window size
int window_w = 1280;
int window_h = 720;

// Global Helpers
ShaderProgram geompass_shader;
ShaderProgram lightpass_shader;
UniformBuffer materials;
UniformBuffer lights;
FrameBuffer framebuffer;
VertexArray screen_quad;
UniformBuffer bear_matrices;
VertexArray bear_mesh;
UniformBuffer ground_matrices;
VertexArray ground_mesh;

// Global matrices
glm::mat4 view;
glm::mat4 projection;

// Lights rotation
glm::mat4 rotation;

// Random colors
glm::vec3 random_colors[N_LIGHTS];

// Camera config
int camera_config = 0;
const int N_CAMERA_CONFIGS = 3;
glm::vec3 eye;
glm::vec3 center;
glm::vec3 up;

// Verifies the condition, if it fails, shows the error message and
// exits the program
#define Assert(condition, message) Assertf(condition, message, 0)
#define Assertf(condition, format, ...)                                        \
  {                                                                            \
    if (!(condition)) {                                                        \
      auto finalformat =                                                       \
          std::string("Error at function ") + __func__ + ": " + format + "\n"; \
      fprintf(stderr, finalformat.c_str(), __VA_ARGS__);                       \
      exit(1);                                                                 \
    }                                                                          \
  }

// Creates the framebuffer used for deferred shading
void LoadFramebuffer() {
  // Creates the position, normal and material textures
  framebuffer.Init(window_w, window_h);
  framebuffer.AddColorTexture(GL_RGB32F, GL_RGB, GL_FLOAT);
  framebuffer.AddColorTexture(GL_RGB32F, GL_RGB, GL_FLOAT);
  framebuffer.AddColorTexture(GL_R8, GL_RED, GL_UNSIGNED_BYTE);
  try {
    framebuffer.Verify();
  } catch (std::exception &e) {
    Assertf(false, "%s", e.what());
  }
}

// Loads the geometry pass and lighting pass shaders
void LoadShaders() {
  try {
    geompass_shader.LoadVertexShader("shaders/geompass_vs.glsl");
    geompass_shader.LoadFragmentShader("shaders/geompass_fs.glsl");
    geompass_shader.LinkShader();
    lightpass_shader.LoadVertexShader("shaders/lightpass_vs.glsl");
    lightpass_shader.LoadFragmentShader("shaders/lightpass_fs.glsl");
    lightpass_shader.LinkShader();
  } catch (std::exception &e) {
    Assertf(false, "%s", e.what());
  }
}

// Creates an random number between 0 an 1
double Random() { return (double)rand() / RAND_MAX; }

// Creates the random colors
void CreateRandomColors() {
  for (int i = 0; i < N_LIGHTS; ++i)
    random_colors[i] = glm::vec3(Random(), Random(), Random());
}

// Loads the materials
void CreateMaterialsBuffer() {
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

  // BEAR_MATERIAL
  materials.Add({0.70, 0.70, 0.70});
  materials.Add({0.50, 0.50, 0.50});
  materials.Add({0.50, 0.50, 0.50});
  materials.Add(16.0f);
  materials.FinishChunk();

  // GROUND_MATERIAL
  materials.Add({0.50, 0.50, 0.50});
  materials.Add({0.50, 0.50, 0.50});
  materials.Add({0.20, 0.20, 0.20});
  materials.Add(16.0f);
  materials.FinishChunk();

  materials.SendToDevice();
}

// Compute the light translation given the i, j indices
glm::mat4 ComputeTranslation(int i, int j) {
  auto x = (i - (N_LIGHTS_I - 1) / 2.0) * I_OFFSET;
  auto z = (j - (N_LIGHTS_J - 1) / 2.0) * J_OFFSET;
  return glm::translate(glm::vec3(x, 0, z));
}

// Loads the screen quad
void LoadScreenQuad() {
  unsigned int indices[] = {0, 1, 2, 3};
  float vertices[] = {
      -1, -1, 0, -1, 1, 0, 1, 1, 0, 1, -1, 0,
  };
  float textcoords[] = {0, 0, 0, 1, 1, 1, 1, 0};
  screen_quad.Init();
  screen_quad.SetElementArray(indices, 4);
  screen_quad.AddArray(0, vertices, 12, 3);
  screen_quad.AddArray(1, textcoords, 8, 2);
}

// Loads the ground quad
void LoadGround() {
  unsigned int indices[] = {0, 1, 2, 3};
  float h = -0.1;
  float v = 100;
  float vertices[] = {-v, h, v, -v, h, -v, v, h, -v, v, h, v};
  float normals[] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
  ground_mesh.Init();
  ground_mesh.SetElementArray(indices, 4);
  ground_mesh.AddArray(0, vertices, 12, 3);
  ground_mesh.AddArray(1, normals, 12, 3);
}

// Loads a single mesh into the gpu
void LoadMesh(VertexArray *vao, tinyobj::mesh_t *mesh) {
  vao->Init();
  vao->SetElementArray(mesh->indices.data(), mesh->indices.size());
  vao->AddArray(0, mesh->positions.data(), mesh->positions.size(), 3);
  vao->AddArray(1, mesh->normals.data(), mesh->normals.size(), 3);
}

// Loads the bear mesh
void LoadBearMesh() {
  auto inputfile = "data/bear-obj.obj";
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string err;
  bool ret = tinyobj::LoadObj(shapes, materials, err, inputfile, "data/");
  Assertf(err.empty() && !ret, "tinyobj error: %s", err.c_str());

  LoadMesh(&bear_mesh, &shapes[0].mesh);
}

// Updates the lights buffer
void UpdateLightsBuffer() {
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

  if (!lights.GetId())
    lights.Init();
  else
    lights.Clear();

  lights.Add({0.2, 0.2, 0.2});
  lights.Add(N_LIGHTS);
  lights.FinishChunk();

  for (int i = 0; i < N_LIGHTS_I; ++i) {
    for (int j = 0; j < N_LIGHTS_J; ++j) {
      auto position = glm::vec4(0.0, 10, 0.0, 1.0);
      auto diffuse = random_colors[i + N_LIGHTS_I * j];
      auto specular = glm::vec3(0.5, 0.5, 0.5);
      auto is_spot = true;
      auto spot_direction = glm::vec3(0.0, -1.0, 0.0);
      auto spot_cutoff = glm::radians(45.0f);
      auto spot_exponent = 16.0f;

      auto model = rotation * ComputeTranslation(i, j);
      auto modelview = view * model;
      auto normalmatrix = glm::transpose(glm::inverse(modelview));
      auto spot_dir_ws = glm::vec4(spot_direction, 1);
      auto spot_dir_vs = glm::normalize(glm::vec3(normalmatrix * spot_dir_ws));

      lights.Add(modelview * position);
      lights.Add(diffuse);
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

// Creates the bear instances matrices
void UpdateBearMatrices() {
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

  if (!bear_matrices.GetId())
    bear_matrices.Init();
  else
    bear_matrices.Clear();

  for (int i = 0; i < N_LIGHTS_I; ++i) {
    for (int j = 0; j < N_LIGHTS_J; ++j) {
      float theta = random_colors[i + j * N_LIGHTS_I].x * 2.0 * M_PI;
      auto rotation = glm::rotate(theta, glm::vec3(0, 1, 0));
      auto model = ComputeTranslation(i, j) * rotation;
      auto modelview = view * model;
      auto normalmatrix = glm::transpose(glm::inverse(modelview));
      auto mvp = projection * modelview;
      bear_matrices.Add(mvp);
      bear_matrices.Add(modelview);
      bear_matrices.Add(normalmatrix);
    }
  }

  bear_matrices.SendToDevice();
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

// Updates the camera configuration
void UpdateCameraConfig() {
  switch (camera_config) {
    case 0:
      eye = glm::vec3(0.0, 5.0, 0.0);
      center = glm::vec3(1.0, 5.0, -1.0);
      up = glm::vec3(0.0, 1.0, 0.0);
      break;
    case 1:
      eye = glm::vec3(-20.0, 20.0, -20.0);
      center = glm::vec3(0.0, 0.0, 0.0);
      up = glm::vec3(0.0, 1.0, 0.0);
      break;
    case 2:
      eye = glm::vec3(0.0, 100.0, 0.0);
      center = glm::vec3(0.0, 0.0, 0.0);
      up = glm::vec3(0.0, 0.0, 1.0);
      break;
    default:
      break;
  }
}

// Updates the variables that depend on the model, view and projection
void UpdateMatrices() {
  UpdateCameraConfig();
  view = glm::lookAt(eye, center, up);
  auto ratio = (float)window_w / (float)window_h;
  projection = glm::perspective(glm::radians(60.0f), ratio, 1.5f, 300.0f);
  UpdateBearMatrices();
  UpdateGroundMatrices();
}

// Loads the global opengl configuration
void LoadGlobalConfiguration() {
  glEnable(GL_DEPTH_TEST);
  glfwWindowHint(GLFW_SAMPLES, 8);
  glEnable(GL_MULTISAMPLE);
}

// Renders the geometry pass
void RenderGeometry() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  geompass_shader.Enable();
  UpdateLightsBuffer();

  geompass_shader.SetUniformBuffer("MatricesBlock", 2, ground_matrices.GetId());
  geompass_shader.SetUniform("material_id", GROUND_MATERIAL);
  ground_mesh.DrawElements(GL_QUADS);

  geompass_shader.SetUniformBuffer("MatricesBlock", 2, bear_matrices.GetId());
  geompass_shader.SetUniform("material_id", BEAR_MATERIAL);
  bear_mesh.DrawInstances(GL_TRIANGLES, N_LIGHTS);

  geompass_shader.Disable();
}

// Renders the lighting pass
void RenderLighting() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  lightpass_shader.Enable();

  auto &texts = framebuffer.GetTextures();
  lightpass_shader.SetTexture2D("position_sampler", 0, texts[0]);
  lightpass_shader.SetTexture2D("normal_sampler", 1, texts[1]);
  lightpass_shader.SetTexture2D("material_sampler", 2, texts[2]);

  lightpass_shader.SetUniformBuffer("MaterialsBlock", 0, materials.GetId());
  lightpass_shader.SetUniformBuffer("LightsBlock", 1, lights.GetId());

  screen_quad.DrawElements(GL_QUADS);

  lightpass_shader.Disable();
}

// Display callback, renders the sphere
void Render() {
  framebuffer.Bind();
  RenderGeometry();
  framebuffer.Unbind();
  RenderLighting();
}

// Measures the frames per second (and prints in the terminal)
void ComputeFPS() {
  static double last = glfwGetTime();
  static int frames = 0;
  double curr = glfwGetTime();
  if (curr - last > 1.0) {
    printf("fps: %d\r", frames);
    fflush(stdout);
    last += 1.0;
    frames = 0;
  } else {
    frames++;
  }
}

// Updates the window size (w, h)
void Resize(GLFWwindow *window) {
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  if (width == window_w && height == window_h) return;

  window_w = width;
  window_h = height;
  glViewport(0, 0, width, height);
  framebuffer.Resize(width, height);
}

// Called each frame
void Idle() {
  static double last = glfwGetTime();
  double curr = glfwGetTime();
  float angle = glm::radians(10 * (curr - last));
  last = curr;
  rotation = glm::rotate(rotation, angle, glm::vec3(0, 1, 0));
}

// Keyboard callback
void Keyboard(GLFWwindow *window, int key, int scancode, int action, int mods) {
  if (action != GLFW_PRESS) return;

  switch (key) {
    case GLFW_KEY_Q:
      exit(0);
      break;
    case GLFW_KEY_SPACE:
      camera_config = (camera_config + 1) % N_CAMERA_CONFIGS;
      break;
    default:
      break;
  }
}

// Mouse Callback
void Mouse(GLFWwindow *window, int button, int action, int mods) {
  double x, y;
  glfwGetCursorPos(window, &x, &y);
}

// Motion callback
void Motion(GLFWwindow *window, double x, double y) {}

// Obtais the monitor if the fullscreen flag is active
GLFWmonitor *GetGLFWMonitor(int argc, char *argv[]) {
  bool fullscreen = false;
  int monitor_id = 0;
  for (int i = 1; i < argc; ++i) {
    if (sscanf(argv[i], "--fullscreen=%d", &monitor_id) == 1) {
      fullscreen = true;
      break;
    }
  }
  if (!fullscreen) {
    return nullptr;
  }
  int n_monitors;
  auto monitors = glfwGetMonitors(&n_monitors);
  Assertf(monitor_id < n_monitors, "monitor %d not found", monitor_id);
  return monitors[monitor_id];
}

// Initializes the GLFW
GLFWwindow *InitGLFW(int argc, char *argv[]) {
  Assert(glfwInit(), "glfw init failed");
  auto monitor = GetGLFWMonitor(argc, argv);
  auto window = glfwCreateWindow(window_w, window_h, "OpenGL4 Application",
                                 monitor, nullptr);
  Assert(window, "glfw window couldn't be created");
  glfwMakeContextCurrent(window);
  glfwSetKeyCallback(window, Keyboard);
  glfwSetMouseButtonCallback(window, Mouse);
  glfwSetCursorPosCallback(window, Motion);
  return window;
}

// Initializes the GLEW
void InitGLEW() {
  auto glew_error = glewInit();
  Assertf(!glew_error, "GLEW error: %s", glewGetErrorString(glew_error));
}

// Initializes the application
void InitApplication() {
  LoadGlobalConfiguration();
  LoadFramebuffer();
  LoadShaders();
  CreateMaterialsBuffer();
  CreateRandomColors();
  LoadScreenQuad();
  LoadGround();
  LoadBearMesh();
}

// Application main loop
void MainLoop(GLFWwindow *window) {
  while (!glfwWindowShouldClose(window)) {
    Idle();
    Resize(window);
    UpdateMatrices();
    Render();
    ComputeFPS();
    glfwSwapBuffers(window);
    glfwPollEvents();
  };
}

// Initialization
int main(int argc, char *argv[]) {
  auto window = InitGLFW(argc, argv);
  InitGLEW();
  InitApplication();
  MainLoop(window);
  glfwTerminate();
  return 0;
}

