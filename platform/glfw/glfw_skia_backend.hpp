#pragma once

#include "glfw_backend.hpp"

#include <mbgl/skia/renderer_backend.hpp>

struct GLFWwindow;

class GLFWSkiaBackend final : public GLFWBackend {
public:
    GLFWSkiaBackend(GLFWwindow* window_, bool capFrameRate);

    mbgl::gfx::RendererBackend& getRendererBackend() override { return rendererBackend; }
    mbgl::Size getSize() const override;
    void setSize(mbgl::Size size) override;

private:
    mbgl::Size size;
    mbgl::skia::RendererBackend rendererBackend;
};
