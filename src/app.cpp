#include "app.h"

#include <chrono>
#include <cstdlib>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {
static App *g_App = nullptr;
}

constexpr uint32_t kWidth = 1920;
constexpr uint32_t kHeight = 1080;
constexpr double kPi = 3.14159;

App::App() {
  g_App = this;

  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW." << std::endl;
    throw "Failed to initialize GLFW";
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  window_ =
      glfwCreateWindow(kWidth, kHeight, "Vulkan Renderer", nullptr, nullptr);
  if (!window_) {
    std::cerr << "Failed to create window." << std::endl;
    throw "Failed to create window.";
  }

  glfwSetInputMode(window_, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);

  glfwSetScrollCallback(window_, scroll_callback);

  device_ = std::make_unique<Device>(window_);
  renderer_ = std::make_unique<Renderer>();

  LoadScene();
}

App::~App() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

void App::LoadScene() {
  auto tile = renderer_->AddMaterial(std::make_unique<OpaqueMaterial>(
      "../../../assets/Stone_Tiles_003_COLOR.png",
      "../../../assets/Stone_Tiles_003_NORM.png", 1.5f, 0.11f, 0.0f));
  auto blue_marble = renderer_->AddMaterial(std::make_unique<OpaqueMaterial>(
      "../../../assets/Blue_Marble_002_COLOR.png",
      "../../../assets/Blue_Marble_002_NORM.png", 2.7f, 0.04f, 0.0f));
  auto brick = renderer_->AddMaterial(std::make_unique<OpaqueMaterial>(
      "../../../assets/brick_color_map.png",
      "../../../assets/brick_normal_map.png", 1.2f, 0.25f, 0.0f));

  auto plane = renderer_->AddMesh("../../../assets/plane.obj");
  auto teapot = renderer_->AddMesh("../../../assets/teapot_low.obj");
  auto sphere = renderer_->AddMesh("../../../assets/sphere.obj");
  auto pedestal = renderer_->AddMesh("../../../assets/pedestal.obj");

  auto floor = renderer_->AddObject(plane, tile);
  floor->position() = glm::vec3(0.0f, -1.0f, 0.0f);

  auto planet = renderer_->AddObject(sphere, blue_marble);
  planet->scale() = glm::vec3(0.25f);

  auto pedestal_object = renderer_->AddObject(pedestal, brick);
  pedestal_object->scale() = glm::vec3(0.25f);
  pedestal_object->position() = glm::vec3(0.0f, -0.5f, 0.0f);

  for (size_t i = 0; i < 9001; i++) {
    auto satellite =
        renderer_->AddObject(teapot, i % 2 == 0 ? tile : brick);
    float r = (std::rand() & 8191) / 8191.0f;
    r = 0.3f + (6.0f * r);
    float x = glm::cos(i * 0.95f);
    float z = glm::sin(i * 0.95f);
    satellite->scale() = glm::vec3(0.04f);
    satellite->position() = glm::vec3(x, 0.0f, z) * r;
    satellite->rotation() = glm::vec3(0.0f, r + i, 0.0f);

    satellites_.push_back(satellite);
  }
}

void App::Run() {
  auto prev = std::chrono::high_resolution_clock::now();
  while (!glfwWindowShouldClose(window_)) {
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dt = now - prev;

    Update(dt.count());
    Render();

    glfwPollEvents();
    prev = now;
  }
}

void App::Update(double dt) {
  elapsed_ += dt;
  if (elapsed_ >= 1.0) {
    std::cout << "FPS: " << 1.0 / dt << std::endl;
    std::cout << "Camera Position: x=" << renderer_->camera().position.x
              << ", y=" << renderer_->camera().position.y
              << ", z=" << renderer_->camera().position.z << std::endl;
    elapsed_ = 0.0;
  }

  float dt_phys = glm::min((float)dt, 1.0f / 60);

  // Update satellites
  for (int i = 0; i < satellites_.size(); i++) {
    auto satellite = satellites_[i];

    auto old_pos = satellite->position();
    glm::vec3 velocity = glm::vec3(old_pos.z, 0.0f, -old_pos.x);
    velocity = 0.5f * velocity / (1.0f + glm::length(velocity) * glm::length(velocity));
    auto new_pos = old_pos + velocity * (float)dt_phys;
    satellite->position() = new_pos;
    satellite->rotation() *= glm::quat(glm::vec3(0.0f, dt_phys * (i % 9), 0.0f));
  }



  double cursor_x, cursor_y;
  glfwGetCursorPos(window_, &cursor_x, &cursor_y);
  glm::vec2 cursor_pos(cursor_x, cursor_y);

  if (glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
    glm::vec2 delta = cursor_pos - previous_cursor_pos_;
    renderer_->camera().RotateBy(delta.y * kPi / kHeight,
                                 -delta.x * kPi / kWidth);
  }

  renderer_->camera().position *= 1.0f + (scroll_offset_ * 0.1f);
  scroll_offset_ = 0.0;

  previous_cursor_pos_ = cursor_pos;
}

void App::Render() { renderer_->Render(); }

void App::scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  g_App->scroll_offset_ += yoffset;
}
