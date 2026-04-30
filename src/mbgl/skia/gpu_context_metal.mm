#include "skia_impl.hpp"

#include <include/gpu/ganesh/GrDirectContext.h>
#include <include/gpu/ganesh/mtl/GrMtlBackendContext.h>
#include <include/gpu/ganesh/mtl/GrMtlDirectContext.h>

#import <Metal/Metal.h>

namespace mbgl {
namespace skia {

sk_sp<GrDirectContext> makeDefaultGaneshContext() {
#if MLN_SKIA_ENABLE_GPU
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
        return nullptr;
    }

    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (!queue) {
        [device release];
        return nullptr;
    }

    GrMtlBackendContext backendContext;
    backendContext.fDevice.reset((__bridge GrMTLHandle)device);
    backendContext.fQueue.reset((__bridge GrMTLHandle)queue);
    return GrDirectContexts::MakeMetal(backendContext);
#else
    return nullptr;
#endif
}

} // namespace skia
} // namespace mbgl
