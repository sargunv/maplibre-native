#include "glfw_skia_backend.hpp"

#include <mbgl/gfx/backend.hpp>

#include <GLFW/glfw3.h>

namespace {

mbgl::Size getFramebufferSize(GLFWwindow* window) {
    int width = 1;
    int height = 1;
    glfwGetFramebufferSize(window, &width, &height);
    return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

} // namespace

GLFWSkiaBackend::GLFWSkiaBackend(GLFWwindow* window, bool)
    : size(getFramebufferSize(window)),
      rendererBackend(size) {}

mbgl::Size GLFWSkiaBackend::getSize() const {
    return size;
}

void GLFWSkiaBackend::setSize(mbgl::Size size_) {
    size = size_;
    rendererBackend.setSize(size_);
}

namespace mbgl {
namespace gfx {

template <>
std::unique_ptr<GLFWBackend> Backend::Create<mbgl::gfx::Backend::Type::Skia>(GLFWwindow* window, bool capFrameRate) {
    return std::make_unique<GLFWSkiaBackend>(window, capFrameRate);
}

} // namespace gfx
} // namespace mbgl
