#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <utility>

namespace D3D11CoreLib {
namespace Processing {
namespace {

using ResourceAdapter = Detail::D3D11ScopedResourceViewAdapter;

D3D11Resource MakeTemporaryOwnedResource(D3D11ResourceView view) {
    ComPtr<ID3D11Resource> resource;
    if (view.Get()) {
        // The view itself remains non-owning. This temporary reference only
        // protects borrowed workspace entries for the duration of a dispatch.
        view.Get()->AddRef();
        resource.Attach(view.Get());
    }
    return D3D11Resource(std::move(resource));
}

D3D11PyramidBlurWorkspace MakeTemporaryWorkspace(
    const D3D11PyramidBlurWorkspaceView& view) {

    D3D11PyramidBlurWorkspace workspace;
    workspace.sourceWidth = view.sourceWidth;
    workspace.sourceHeight = view.sourceHeight;
    workspace.levels = view.levels;
    workspace.format = view.format;

    workspace.downTextures.reserve(view.downTextures.size());
    for (D3D11ResourceView resource : view.downTextures) {
        workspace.downTextures.push_back(MakeTemporaryOwnedResource(resource));
    }

    workspace.blurScratch = MakeTemporaryOwnedResource(view.blurScratch);
    workspace.blurredLow = MakeTemporaryOwnedResource(view.blurredLow);

    workspace.upTextures.reserve(view.upTextures.size());
    for (D3D11ResourceView resource : view.upTextures) {
        workspace.upTextures.push_back(MakeTemporaryOwnedResource(resource));
    }
    return workspace;
}

D3D11PyramidRegionBlurWorkspace MakeTemporaryWorkspace(
    const D3D11PyramidRegionBlurWorkspaceView& view) {

    D3D11PyramidRegionBlurWorkspace workspace;
    workspace.sourceWidth = view.sourceWidth;
    workspace.sourceHeight = view.sourceHeight;
    workspace.levels = view.levels;
    workspace.format = view.format;
    workspace.blurWorkspace = MakeTemporaryWorkspace(view.blurWorkspace);
    workspace.blurred = MakeTemporaryOwnedResource(view.blurred);
    return workspace;
}

} // namespace

D3D11TextureViewSet CreateRgbaTextureViewSetView(
    D3D11ProcessingContext& context,
    D3D11ResourceView texture,
    bool createSrv,
    bool createUav,
    DXGI_FORMAT viewFormat) {

    ResourceAdapter adapter(texture);
    return CreateRgbaTextureViewSet(
        context, adapter.Resource(), createSrv, createUav, viewFormat);
}

D3D11TextureViewSet CreateYuv420SrvViewSetView(
    D3D11ProcessingContext& context,
    D3D11ResourceView texture) {

    ResourceAdapter adapter(texture);
    return CreateYuv420SrvViewSet(context, adapter.Resource());
}

D3D11TextureViewSet CreateYuv420UavViewSetView(
    D3D11ProcessingContext& context,
    D3D11ResourceView texture) {

    ResourceAdapter adapter(texture);
    return CreateYuv420UavViewSet(context, adapter.Resource());
}

D3D11TextureViewSet CreateYuv420SrvUavViewSetView(
    D3D11ProcessingContext& context,
    D3D11ResourceView texture,
    bool createSrv,
    bool createUav) {

    ResourceAdapter adapter(texture);
    return CreateYuv420SrvUavViewSet(
        context, adapter.Resource(), createSrv, createUav);
}

void D3D11FormatConverter::DispatchConvertView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const FormatConvertDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchConvert(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11Resizer::DispatchResizeView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const ResizeDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchResize(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11Remapper::DispatchRemapView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView map,
    D3D11ResourceView dst,
    const RemapDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter mapAdapter(map);
    ResourceAdapter dstAdapter(dst);
    DispatchRemap(
        deviceContext,
        srcAdapter.Resource(),
        mapAdapter.Resource(),
        dstAdapter.Resource(),
        desc);
}

void D3D11Compositor::DispatchCompositeView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView base,
    D3D11ResourceView overlay,
    D3D11ResourceView dst,
    const CompositeDesc& desc) {

    ResourceAdapter baseAdapter(base);
    ResourceAdapter overlayAdapter(overlay);
    ResourceAdapter dstAdapter(dst);
    DispatchComposite(
        deviceContext,
        baseAdapter.Resource(),
        overlayAdapter.Resource(),
        dstAdapter.Resource(),
        desc);
}

void D3D11FusedProcessor::DispatchConvertResizeView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const FusedConvertResizeDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchConvertResize(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11Blurrer::DispatchBlurView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView scratch,
    D3D11ResourceView dst,
    const BlurDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter scratchAdapter(scratch);
    ResourceAdapter dstAdapter(dst);
    DispatchBlur(
        deviceContext,
        srcAdapter.Resource(),
        scratchAdapter.Resource(),
        dstAdapter.Resource(),
        desc);
}

void D3D11RegionEffect::DispatchRegionEffectView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const RegionEffectDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchRegionEffect(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11RegionBlur::DispatchRegionBlurView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView blurScratch,
    D3D11ResourceView blurred,
    D3D11ResourceView dst,
    const RegionBlurDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter scratchAdapter(blurScratch);
    ResourceAdapter blurredAdapter(blurred);
    ResourceAdapter dstAdapter(dst);
    DispatchRegionBlur(
        deviceContext,
        srcAdapter.Resource(),
        scratchAdapter.Resource(),
        blurredAdapter.Resource(),
        dstAdapter.Resource(),
        desc);
}

void D3D11ColorAdjuster::DispatchColorAdjustView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const ColorAdjustDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchColorAdjust(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11KernelFilter::DispatchKernelFilterView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const KernelFilterDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchKernelFilter(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11MaskProcessor::DispatchApplyMaskView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView mask,
    D3D11ResourceView dst,
    const MaskApplyDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter maskAdapter(mask);
    ResourceAdapter dstAdapter(dst);
    DispatchApplyMask(
        deviceContext,
        srcAdapter.Resource(),
        maskAdapter.Resource(),
        dstAdapter.Resource(),
        desc);
}

void D3D11MaskProcessor::DispatchBlendByMaskView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView base,
    D3D11ResourceView overlay,
    D3D11ResourceView mask,
    D3D11ResourceView dst,
    const MaskBlendDesc& desc) {

    ResourceAdapter baseAdapter(base);
    ResourceAdapter overlayAdapter(overlay);
    ResourceAdapter maskAdapter(mask);
    ResourceAdapter dstAdapter(dst);
    DispatchBlendByMask(
        deviceContext,
        baseAdapter.Resource(),
        overlayAdapter.Resource(),
        maskAdapter.Resource(),
        dstAdapter.Resource(),
        desc);
}

void D3D11MaskProcessor::DispatchCombineMasksView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView maskA,
    D3D11ResourceView maskB,
    D3D11ResourceView dst,
    const MaskCombineDesc& desc) {

    ResourceAdapter maskAAdapter(maskA);
    ResourceAdapter maskBAdapter(maskB);
    ResourceAdapter dstAdapter(dst);
    DispatchCombineMasks(
        deviceContext,
        maskAAdapter.Resource(),
        maskBAdapter.Resource(),
        dstAdapter.Resource(),
        desc);
}

void D3D11MaskProcessor::DispatchInvertMaskView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView mask,
    D3D11ResourceView dst,
    const MaskInvertDesc& desc) {

    ResourceAdapter maskAdapter(mask);
    ResourceAdapter dstAdapter(dst);
    DispatchInvertMask(
        deviceContext, maskAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11ThresholdProcessor::DispatchThresholdView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const ThresholdDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchThreshold(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11ThresholdProcessor::DispatchRangeThresholdView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const RangeThresholdDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchRangeThreshold(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11ThresholdProcessor::DispatchConfidenceHeatmapView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const ConfidenceHeatmapDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchConfidenceHeatmap(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11ThresholdProcessor::DispatchClassColorMapView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const ClassColorMapDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchClassColorMap(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11ThresholdProcessor::DispatchMaskOverlayView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView mask,
    D3D11ResourceView dst,
    const MaskOverlayDesc& desc) {

    ResourceAdapter maskAdapter(mask);
    ResourceAdapter dstAdapter(dst);
    DispatchMaskOverlay(
        deviceContext, maskAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11PyramidProcessor::DispatchDownsample2xView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const PyramidDownsampleDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchDownsample2x(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11PyramidProcessor::DispatchUpsample2xView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const PyramidUpsampleDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchUpsample2x(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11PyramidBlur::DispatchPyramidBlurView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    const D3D11PyramidBlurWorkspaceView& workspace,
    D3D11ResourceView dst,
    const PyramidBlurDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    auto temporaryWorkspace = MakeTemporaryWorkspace(workspace);
    DispatchPyramidBlur(
        deviceContext,
        srcAdapter.Resource(),
        temporaryWorkspace,
        dstAdapter.Resource(),
        desc);
}

void D3D11PyramidRegionBlur::DispatchPyramidRegionBlurView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    const D3D11PyramidRegionBlurWorkspaceView& workspace,
    D3D11ResourceView dst,
    const PyramidRegionBlurDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    auto temporaryWorkspace = MakeTemporaryWorkspace(workspace);
    DispatchPyramidRegionBlur(
        deviceContext,
        srcAdapter.Resource(),
        temporaryWorkspace,
        dstAdapter.Resource(),
        desc);
}

void D3D11AdvancedProcessor::DispatchAffineTransformView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const AffineTransformDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchAffineTransform(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11AdvancedProcessor::DispatchPerspectiveTransformView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView dst,
    const PerspectiveTransformDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter dstAdapter(dst);
    DispatchPerspectiveTransform(
        deviceContext, srcAdapter.Resource(), dstAdapter.Resource(), desc);
}

void D3D11AdvancedProcessor::DispatchApplyLut3DView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView lut,
    D3D11ResourceView dst,
    const Lut3DDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter lutAdapter(lut);
    ResourceAdapter dstAdapter(dst);
    DispatchApplyLut3D(
        deviceContext,
        srcAdapter.Resource(),
        lutAdapter.Resource(),
        dstAdapter.Resource(),
        desc);
}

void D3D11AdvancedProcessor::DispatchApplyUndistortMapView(
    ID3D11DeviceContext* deviceContext,
    D3D11ResourceView src,
    D3D11ResourceView map,
    D3D11ResourceView dst,
    const RemapDesc& desc) {

    ResourceAdapter srcAdapter(src);
    ResourceAdapter mapAdapter(map);
    ResourceAdapter dstAdapter(dst);
    DispatchApplyUndistortMap(
        deviceContext,
        srcAdapter.Resource(),
        mapAdapter.Resource(),
        dstAdapter.Resource(),
        desc);
}

} // namespace Processing
} // namespace D3D11CoreLib
