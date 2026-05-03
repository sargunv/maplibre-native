#include "glfw_skia_backend.hpp"

#include <mbgl/gfx/backend.hpp>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

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
      rendererBackend(size) {
    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (!nsWindow) {
        return;
    }

    id<MTLDevice> device = (__bridge id<MTLDevice>)rendererBackend.getMetalDevice();
    if (!device) {
        // Skia backend fell back to raster; nothing to attach to.
        return;
    }

    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    layer.contentsGravity = kCAGravityTopLeft;
    layer.drawableSize = CGSizeMake(static_cast<CGFloat>(std::max<uint32_t>(1, size.width)),
                                    static_cast<CGFloat>(std::max<uint32_t>(1, size.height)));

    nsWindow.contentView.wantsLayer = YES;
    nsWindow.contentView.layer = layer;

    metalLayer = (__bridge_retained void*)layer;
    rendererBackend.attachMetalLayer(metalLayer);
}

GLFWSkiaBackend::~GLFWSkiaBackend() {
    if (metalLayer) {
        rendererBackend.attachMetalLayer(nullptr);
        CAMetalLayer* layer = (__bridge_transfer CAMetalLayer*)metalLayer;
        (void)layer; // Releases via ARC when this scope ends.
        metalLayer = nullptr;
    }
}

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
