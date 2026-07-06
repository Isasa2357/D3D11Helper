//
// test_module_headers.cpp
//
// v1.1.0 architecture smoke test.
//
// This test intentionally includes both the new module umbrella headers and the
// legacy compatibility headers.  It does not create a D3D device; it only checks
// that the public header graph is self-contained and that common public symbols
// remain reachable through the new module layout.
//

#include <D3D11Helper/D3D11Foundation/D3D11Foundation.hpp>
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>

// Legacy include paths must remain valid during the v1.x compatibility period.
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>
#include <D3D11Helper/D3D11Core/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Core/D3D11FormatUtil.hpp>
#include <D3D11Helper/D3D11Core/D3D11Debug.hpp>
#include <D3D11Helper/D3D11Core/D3D11Fence.hpp>
#include <D3D11Helper/D3D11Core/D3D11SharedResource.hpp>
#include <D3D11Helper/D3D11Framework/D3D11SwapChainHelper.hpp>

#include <iostream>
#include <type_traits>

int main() {
    using namespace D3D11CoreLib;

    static_assert(std::is_move_constructible<D3D11Fence>::value,
                  "D3D11Fence must remain move-constructible.");
    static_assert(!std::is_copy_constructible<D3D11Fence>::value,
                  "D3D11Fence must remain move-only.");

    D3D11CoreConfig config = {};
    (void)config;

    D3D11Resource resource;
    D3D11StagingBuffer staging;
    D3D11ComputePipeline computePipeline;
    D3D11ComputeBindingSet computeBindings;
    D3D11GraphicsPipeline graphicsPipeline;
    D3D11Fence fence;
    Processing::ProcessingRect rect = {};
    (void)resource;
    (void)staging;
    (void)computePipeline;
    computeBindings.SetShaderResource(0, nullptr);
    computeBindings.SetUnorderedAccess(0, nullptr);
    computeBindings.SetConstantBuffer(0, nullptr);
    computeBindings.SetSampler(0, nullptr);
    computeBindings.Clear();
    (void)graphicsPipeline;
    (void)fence;
    (void)rect;

    auto linearClamp = MakeLinearClampSamplerDesc();
    auto pointClamp = MakePointClampSamplerDesc();
    (void)linearClamp;
    (void)pointClamp;

    if (!FormatUtil::IsYuvFormat(DXGI_FORMAT_NV12)) {
        std::cerr << "Expected NV12 to be classified as a YUV format.\n";
        return 1;
    }

    if (!FormatUtil::IsDepthFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)) {
        std::cerr << "Expected D24_UNORM_S8_UINT to be classified as a depth format.\n";
        return 1;
    }

    if (FormatUtil::BytesPerPixel(DXGI_FORMAT_R8G8B8A8_UNORM) != 4) {
        std::cerr << "Unexpected bytes-per-pixel for R8G8B8A8_UNORM.\n";
        return 1;
    }

    std::cout << "RESULT: OK - module headers and compatibility wrappers compile\n";
    return 0;
}
