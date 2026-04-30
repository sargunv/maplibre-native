#include <mbgl/map/camera.hpp>
#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/renderer/renderer.hpp>
#include <mbgl/skia/renderer_backend.hpp>
#include <mbgl/style/layer.hpp>
#include <mbgl/style/layers/background_layer.hpp>
#include <mbgl/style/light.hpp>
#include <mbgl/renderer/update_parameters.hpp>
#include <mbgl/util/chrono.hpp>
#include <mbgl/util/color.hpp>
#include <mbgl/util/image.hpp>

#include <mbgl/map/transform_state.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

bool writePPM(const std::string& path, const mbgl::PremultipliedImage& image) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }

    stream << "P6\n" << image.size.width << " " << image.size.height << "\n255\n";
    for (uint32_t y = 0; y < image.size.height; ++y) {
        const auto* row = image.data.get() + y * image.stride();
        for (uint32_t x = 0; x < image.size.width; ++x) {
            const auto* pixel = row + x * 4;
            const char rgb[] = {static_cast<char>(pixel[0]), static_cast<char>(pixel[1]), static_cast<char>(pixel[2])};
            stream.write(rgb, sizeof(rgb));
        }
    }
    return stream.good();
}

} // namespace

int main(int argc, char** argv) {
    const std::string outputPath = argc > 1 ? argv[1] : "skia-map-smoke.ppm";
    constexpr mbgl::Size size{256, 256};
    constexpr float pixelRatio = 1.0f;

    mbgl::skia::RendererBackend backend(size);
    mbgl::gfx::BackendScope backendScope{backend};
    mbgl::Renderer renderer(backend, pixelRatio);

    mbgl::TransformState transformState;
    transformState.setSize(size);
    transformState.setLatLngZoom(mbgl::LatLng{0.0, 0.0}, 0.0);

    mbgl::style::BackgroundLayer background("background");
    background.setBackgroundColor(mbgl::Color{0.09f, 0.53f, 0.84f, 1.0f});

    auto layers = mbgl::makeMutable<std::vector<mbgl::Immutable<mbgl::style::Layer::Impl>>>();
    layers->emplace_back(background.baseImpl);

    mbgl::style::Light light;
    auto updateParameters = std::make_shared<mbgl::UpdateParameters>(mbgl::UpdateParameters{
        true,
        mbgl::MapMode::Static,
        pixelRatio,
        mbgl::MapDebugOptions{},
        mbgl::TimePoint{},
        transformState,
        {},
        nullptr,
        true,
        mbgl::style::TransitionOptions{},
        light.impl,
        mbgl::makeMutable<std::vector<mbgl::Immutable<mbgl::style::Image::Impl>>>(),
        mbgl::makeMutable<std::vector<mbgl::Immutable<mbgl::style::Source::Impl>>>(),
        std::move(layers),
        {},
        nullptr,
        0,
        true,
        true,
    });

    renderer.render(updateParameters);

    const auto image = backend.readStillImage();
    if (!image.valid()) {
        std::cerr << "MapLibre Skia render did not produce an image\n";
        return EXIT_FAILURE;
    }

    if (!writePPM(outputPath, image)) {
        std::cerr << "Failed to write " << outputPath << "\n";
        return EXIT_FAILURE;
    }

    std::cout << outputPath << "\n";
    return EXIT_SUCCESS;
}
