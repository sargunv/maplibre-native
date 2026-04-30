#include "skia_impl.hpp"

#include <include/core/SkBlender.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkData.h>
#include <include/core/SkMesh.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkString.h>
#include <include/gpu/ganesh/SkMeshGanesh.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace {

struct Vertex {
    float position[2];
};

sk_sp<SkMeshSpecification> makeSpecification() {
    using Attribute = SkMeshSpecification::Attribute;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, 0, SkString("a_pos")}};

    const SkString vertexShader(R"(
        Varyings main(const Attributes attrs) {
            Varyings varyings;
            varyings.position = attrs.a_pos;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        layout(color) uniform half4 u_color;

        float2 main(const Varyings varyings, out half4 color) {
            color = u_color;
            return varyings.position;
        }
    )");

    auto [specification, error] = SkMeshSpecification::Make(attributes,
                                                            sizeof(Vertex),
                                                            SkSpan<const SkMeshSpecification::Varying>(),
                                                            vertexShader,
                                                            fragmentShader);
    if (!specification) {
        std::cerr << "SkMeshSpecification failed: " << error.c_str() << "\n";
    }
    return specification;
}

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
    const std::string outputPath = argc > 1 ? argv[1] : "skia-smoke.ppm";
    constexpr mbgl::Size size{256, 256};

    auto directContext = mbgl::skia::makeDefaultGaneshContext();
    if (!directContext) {
        std::cerr << "Failed to create Skia GPU context\n";
        return EXIT_FAILURE;
    }

    mbgl::skia::RenderableResource resource(size, directContext.get());
    auto* canvas = resource.getCanvas();
    if (!canvas) {
        std::cerr << "Failed to create Skia GPU canvas\n";
        return EXIT_FAILURE;
    }

    canvas->clear(SkColor4f{0.05f, 0.06f, 0.08f, 1.0f});

    const auto specification = makeSpecification();
    if (!specification) {
        return EXIT_FAILURE;
    }

    const Vertex vertices[] = {{{32.0f, 224.0f}}, {{128.0f, 32.0f}}, {{224.0f, 224.0f}}};
    const std::uint16_t indexes[] = {0, 1, 2};

    const auto vertexBuffer = SkMeshes::MakeVertexBuffer(directContext.get(), vertices, sizeof(vertices));
    const auto indexBuffer = SkMeshes::MakeIndexBuffer(directContext.get(), indexes, sizeof(indexes));
    if (!vertexBuffer || !indexBuffer) {
        std::cerr << "Failed to create GPU-backed SkMesh buffers\n";
        return EXIT_FAILURE;
    }

    auto uniforms = SkData::MakeUninitialized(specification->uniformSize());
    const float color[] = {0.0f, 0.65f, 1.0f, 1.0f};
    const auto* colorUniform = specification->findUniform("u_color");
    if (!colorUniform || colorUniform->offset + sizeof(color) > uniforms->size()) {
        std::cerr << "Missing u_color uniform\n";
        return EXIT_FAILURE;
    }
    std::memcpy(static_cast<std::uint8_t*>(uniforms->writable_data()) + colorUniform->offset, color, sizeof(color));

    const auto mesh = SkMesh::MakeIndexed(specification,
                                          SkMesh::Mode::kTriangles,
                                          vertexBuffer,
                                          std::size(vertices),
                                          /*vertexOffset=*/0,
                                          indexBuffer,
                                          std::size(indexes),
                                          /*indexOffset=*/0,
                                          uniforms,
                                          SkSpan<SkMesh::ChildPtr>(),
                                          SkRect::MakeWH(size.width, size.height));
    if (!mesh.mesh.isValid()) {
        std::cerr << "SkMesh creation failed: " << mesh.error.c_str() << "\n";
        return EXIT_FAILURE;
    }

    SkPaint paint;
    paint.setAntiAlias(true);
    canvas->drawMesh(mesh.mesh, SkBlender::Mode(SkBlendMode::kSrcOver), paint);
    resource.flush();

    mbgl::PremultipliedImage image(size);
    const auto info = SkImageInfo::Make(size.width, size.height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    if (!resource.getSurface()->readPixels(info, image.data.get(), image.stride(), 0, 0)) {
        std::cerr << "Failed to read Skia GPU surface\n";
        return EXIT_FAILURE;
    }

    if (!writePPM(outputPath, image)) {
        std::cerr << "Failed to write " << outputPath << "\n";
        return EXIT_FAILURE;
    }

    std::cout << outputPath << "\n";
    return EXIT_SUCCESS;
}
