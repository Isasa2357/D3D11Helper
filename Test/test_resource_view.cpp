//
// test_resource_view.cpp - non-owning D3D11 resource-view coverage.
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Gpu/D3D11ResourceValidation.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ULONG ProbeReferenceCount(IUnknown* object) {
    if (!object) {
        throw std::runtime_error("ProbeReferenceCount: null object");
    }
    const ULONG value = object->AddRef();
    object->Release();
    return value;
}

std::filesystem::path ProcessingShaderDir() {
    const auto namespaced = std::filesystem::current_path() /
        "D3D11Helper" / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(namespaced / "ConvertRgbToRgb.hlsl")) {
        return namespaced;
    }

    const auto legacy = std::filesystem::current_path() /
        "shaders" / "D3D11Processing";
    if (std::filesystem::exists(legacy / "ConvertRgbToRgb.hlsl")) {
        return legacy;
    }

#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto source = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR)
        .parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(source / "ConvertRgbToRgb.hlsl")) {
        return source;
    }
#endif

    return namespaced;
}

struct Fixture {
    std::shared_ptr<D3D11Core> core;
    D3D11ProcessingContext processing;

    Fixture()
        : core(TestUtil::MakeCore()) {
        processing.Initialize(*core, ProcessingShaderDir());
    }
};

std::vector<uint8_t> ReadbackRgba8(D3D11Core& core, D3D11Resource& texture) {
    const D3D11CpuImage image = ReadbackTexture2DToCpuImage(core, texture);
    Require(image.width == 2 && image.height == 2, "readback size mismatch");
    Require(image.format == DXGI_FORMAT_R8G8B8A8_UNORM, "readback format mismatch");
    return image.pixels;
}

} // namespace

int main() {
    TEST_RUN("Resource view construction and validation preserve ownership", {
        Fixture fixture;
        const std::vector<uint8_t> pixels = {
            10, 20, 30, 255, 40, 50, 60, 255,
            70, 80, 90, 255, 100, 110, 120, 255,
        };
        auto texture = CreateTexture2DFromRGBA(
            *fixture.core,
            pixels.data(),
            2,
            2,
            D3D11_BIND_SHADER_RESOURCE);

        ID3D11Resource* raw = texture.Get();
        const ULONG before = ProbeReferenceCount(raw);
        {
            D3D11ResourceView view(raw);
            D3D11ResourceView copy = view;
            Require(view.Get() == raw && copy.Get() == raw, "resource view pointer mismatch");
        }
        Require(ProbeReferenceCount(raw) == before,
            "D3D11ResourceView changed the COM reference count");

        D3D11Texture2DRequirement requirement;
        requirement.device = fixture.core->GetDevice();
        requirement.width = 2;
        requirement.height = 2;
        requirement.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        requirement.mipLevels = 1;
        requirement.arraySize = 1;
        requirement.sampleCount = 1;
        requirement.requiredBindFlags = D3D11_BIND_SHADER_RESOURCE;

        const auto valid = ValidateTexture2DView(D3D11ResourceView(raw), requirement);
        Require(valid.IsValid(), valid.Message());
        Require(ProbeReferenceCount(raw) == before,
            "resource validation retained a COM reference");

        requirement.width = 4;
        requirement.forbiddenBindFlags = D3D11_BIND_SHADER_RESOURCE;
        const auto invalid = ValidateTexture2DView(D3D11ResourceView(raw), requirement);
        Require(!invalid.IsValid(), "invalid requirement unexpectedly passed");
        Require(invalid.errors.size() >= 2, "aggregate validation did not retain all errors");
    });

    TEST_RUN("Processing view helper releases temporary COM references", {
        Fixture fixture;
        const std::vector<uint8_t> pixels(2u * 2u * 4u, 64u);
        auto texture = CreateTexture2DFromRGBA(
            *fixture.core,
            pixels.data(),
            2,
            2,
            D3D11_BIND_SHADER_RESOURCE);

        ID3D11Resource* raw = texture.Get();
        const ULONG before = ProbeReferenceCount(raw);
        {
            auto views = CreateRgbaTextureViewSetView(
                fixture.processing,
                D3D11ResourceView(raw),
                true,
                false,
                DXGI_FORMAT_R8G8B8A8_UNORM);
            Require(views.HasSrv(), "view helper did not create an SRV");
        }
        Require(ProbeReferenceCount(raw) == before,
            "Processing view helper retained a COM reference");
    });

    TEST_RUN("Format conversion dispatch view executes without retained ownership", {
        Fixture fixture;
        if (!fixture.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV is not supported");
            return;
        }

        const std::vector<uint8_t> pixels = {
            10, 20, 30, 255, 40, 50, 60, 255,
            70, 80, 90, 255, 100, 110, 120, 255,
        };
        auto src = CreateTexture2DFromRGBA(
            *fixture.core,
            pixels.data(),
            2,
            2,
            D3D11_BIND_SHADER_RESOURCE);

        D3D11FormatConverter converter;
        converter.Initialize(fixture.processing);
        auto dst = converter.CreateOutputTexture(
            *fixture.core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);

        ID3D11Resource* srcRaw = src.Get();
        ID3D11Resource* dstRaw = dst.Get();
        const ULONG srcBefore = ProbeReferenceCount(srcRaw);
        const ULONG dstBefore = ProbeReferenceCount(dstRaw);

        FormatConvertDesc desc{};
        desc.srcFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        converter.DispatchConvertView(
            fixture.core->GetImmediateContext(),
            D3D11ResourceView(srcRaw),
            D3D11ResourceView(dstRaw),
            desc);

        Require(ProbeReferenceCount(srcRaw) == srcBefore,
            "source dispatch view retained a COM reference");
        Require(ProbeReferenceCount(dstRaw) == dstBefore,
            "destination dispatch view retained a COM reference");
        Require(ReadbackRgba8(*fixture.core, dst) == pixels,
            "DispatchConvertView output mismatch");
    });

    TEST_RUN("All Processing resource-view entry points are addressable", {
        (void)&D3D11FormatConverter::DispatchConvertView;
        (void)&D3D11Resizer::DispatchResizeView;
        (void)&D3D11Remapper::DispatchRemapView;
        (void)&D3D11Compositor::DispatchCompositeView;
        (void)&D3D11FusedProcessor::DispatchConvertResizeView;
        (void)&D3D11Blurrer::DispatchBlurView;
        (void)&D3D11RegionEffect::DispatchRegionEffectView;
        (void)&D3D11RegionBlur::DispatchRegionBlurView;
        (void)&D3D11ColorAdjuster::DispatchColorAdjustView;
        (void)&D3D11KernelFilter::DispatchKernelFilterView;
        (void)&D3D11MaskProcessor::DispatchApplyMaskView;
        (void)&D3D11MaskProcessor::DispatchBlendByMaskView;
        (void)&D3D11MaskProcessor::DispatchCombineMasksView;
        (void)&D3D11MaskProcessor::DispatchInvertMaskView;
        (void)&D3D11ThresholdProcessor::DispatchThresholdView;
        (void)&D3D11ThresholdProcessor::DispatchRangeThresholdView;
        (void)&D3D11ThresholdProcessor::DispatchConfidenceHeatmapView;
        (void)&D3D11ThresholdProcessor::DispatchClassColorMapView;
        (void)&D3D11ThresholdProcessor::DispatchMaskOverlayView;
        (void)&D3D11PyramidProcessor::DispatchDownsample2xView;
        (void)&D3D11PyramidProcessor::DispatchUpsample2xView;
        (void)&D3D11PyramidBlur::DispatchPyramidBlurView;
        (void)&D3D11PyramidRegionBlur::DispatchPyramidRegionBlurView;
        (void)&D3D11AdvancedProcessor::DispatchAffineTransformView;
        (void)&D3D11AdvancedProcessor::DispatchPerspectiveTransformView;
        (void)&D3D11AdvancedProcessor::DispatchApplyLut3DView;
        (void)&D3D11AdvancedProcessor::DispatchApplyUndistortMapView;
    });

    return TestUtil::Result("D3D11 ResourceView");
}
