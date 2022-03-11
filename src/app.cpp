#include "app.h"

#include <chrono>
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
}

App::~App() {
  glfwDestroyWindow(window_);
  glfwTerminate();
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
