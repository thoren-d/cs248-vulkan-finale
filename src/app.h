#ifndef APP_H_
#define APP_H_

#include <memory>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "device.h"
#include "renderer.h"

class App {
public:
    App();
    ~App();

    void Run();

private:
    void LoadScene();

    void Update(double dt);
    void Render();

    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

    std::unique_ptr<Device> device_;
    std::unique_ptr<Renderer> renderer_;

    std::vector<Object*> satellites_;

    double elapsed_ = 0.0;
    glm::vec2 previous_cursor_pos_ = glm::vec2(0.0f);
    double scroll_offset_ = 0.0;
    GLFWwindow* window_ = nullptr;
};

#endif // APP_H_
