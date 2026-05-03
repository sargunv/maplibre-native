#include "skia_impl.hpp"

#include <include/gpu/ganesh/GrDirectContext.h>
#include <include/gpu/ganesh/mtl/GrMtlBackendContext.h>
#include <include/gpu/ganesh/mtl/GrMtlDirectContext.h>
#include <include/gpu/ganesh/mtl/SkSurfaceMetal.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace mbgl {
namespace skia {

MetalGpuHandles::MetalGpuHandles(void* device_, void* queue_) noexcept
    : device(device_), queue(queue_) {}

MetalGpuHandles::~MetalGpuHandles() {
    if (device) {
        CFRelease(device);
        device = nullptr;
    }
    if (queue) {
        CFRelease(queue);
        queue = nullptr;
    }
}

MetalGpuHandles::MetalGpuHandles(MetalGpuHandles&& other) noexcept
    : device(other.device), queue(other.queue) {
    other.device = nullptr;
    other.queue = nullptr;
}

MetalGpuHandles& MetalGpuHandles::operator=(MetalGpuHandles&& other) noexcept {
    if (this != &other) {
        if (device) CFRelease(device);
        if (queue) CFRelease(queue);
        device = other.device;
        queue = other.queue;
        other.device = nullptr;
        other.queue = nullptr;
    }
    return *this;
}

GaneshGpuContext makeDefaultGaneshContext() {
#if MLN_SKIA_ENABLE_GPU
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
        return {};
    }

    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (!queue) {
        return {};
    }

    GrMtlBackendContext backendContext;
    backendContext.fDevice.retain((__bridge GrMTLHandle)device);
    backendContext.fQueue.retain((__bridge GrMTLHandle)queue);
    auto context = GrDirectContexts::MakeMetal(backendContext);
    if (!context) {
        return {};
    }

    return GaneshGpuContext{
        std::move(context),
        MetalGpuHandles{(__bridge_retained void*)device, (__bridge_retained void*)queue}
    };
#else
    return {};
#endif
}

sk_sp<SkSurface> wrapMetalLayerSurface(GrDirectContext* context, void* metalLayer, void** outDrawable) {
#if MLN_SKIA_ENABLE_GPU
    if (!context || !metalLayer || !outDrawable) {
        return nullptr;
    }
    GrMTLHandle drawable = nullptr;
    auto surface = SkSurfaces::WrapCAMetalLayer(context,
                                                (GrMTLHandle)metalLayer,
                                                kTopLeft_GrSurfaceOrigin,
                                                /*sampleCnt=*/1,
                                                kBGRA_8888_SkColorType,
                                                /*colorSpace=*/nullptr,
                                                /*surfaceProps=*/nullptr,
                                                &drawable);
    *outDrawable = const_cast<void*>(drawable);
    return surface;
#else
    (void)context;
    (void)metalLayer;
    (void)outDrawable;
    return nullptr;
#endif
}

void presentMetalDrawable(void* metalQueue, void* drawable) {
#if MLN_SKIA_ENABLE_GPU
    if (!metalQueue || !drawable) {
        if (drawable) {
            CFRelease(drawable);
        }
        return;
    }
    id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)metalQueue;
    id<CAMetalDrawable> mtlDrawable = (__bridge_transfer id<CAMetalDrawable>)drawable;
    id<MTLCommandBuffer> buffer = [queue commandBuffer];
    [buffer presentDrawable:mtlDrawable];
    [buffer commit];
#else
    (void)metalQueue;
    if (drawable) {
        // Without Metal we cannot release; treat as a programming error.
    }
#endif
}

void releaseMetalDrawable(void* drawable) {
#if MLN_SKIA_ENABLE_GPU
    if (drawable) {
        CFRelease(drawable);
    }
#else
    (void)drawable;
#endif
}

void updateMetalLayerDrawableSize(void* metalLayer, Size size) {
#if MLN_SKIA_ENABLE_GPU
    if (!metalLayer) return;
    CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
    layer.drawableSize = CGSizeMake(static_cast<CGFloat>(std::max<uint32_t>(1, size.width)),
                                    static_cast<CGFloat>(std::max<uint32_t>(1, size.height)));
#else
    (void)metalLayer;
    (void)size;
#endif
}

} // namespace skia
} // namespace mbgl
