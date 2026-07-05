#include "D3D11Processing/D3D11TextureViews.hpp"

#include <sstream>

namespace D3D11CoreLib {
namespace Processing {
namespace {

void ValidateTexture(const D3D11Resource& texture, const char* functionName) {
    if (!texture.Get()) {
        std::ostringstream os;
        os << functionName << ": null texture";
        throw ValidationError(os.str());
    }
    const auto desc = texture.GetTexture2DDesc();
    if (desc.Width == 0 || desc.Height == 0) {
        std::ostringstream os;
        os << functionName << ": resource is not Texture2D or has zero size";
        throw ValidationError(os.str());
    }
}

void ValidateUavFlag(const D3D11Resource& texture, const char* functionName) {
    const auto desc = texture.GetTexture2DDesc();
    if ((desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) == 0) {
        std::ostringstream os;
        os << functionName << ": texture was not created with D3D11_BIND_UNORDERED_ACCESS";
        throw ValidationError(os.str());
    }
}

DXGI_FORMAT ResolveViewFormat(const D3D11Resource& texture, DXGI_FORMAT viewFormat) {
    return viewFormat == DXGI_FORMAT_UNKNOWN ? texture.GetFormat() : viewFormat;
}

void CheckRgbaUavSupport(D3D11ProcessingContext& context, DXGI_FORMAT format, const char* fn) {
    if (format == DXGI_FORMAT_R8G8B8A8_UNORM && !context.SupportsRgba8Uav()) {
        throw UnsupportedFeatureError(std::string(fn) + ": R8G8B8A8 UAV is not supported");
    }
    if (format == DXGI_FORMAT_B8G8R8A8_UNORM && !context.SupportsBgra8Uav()) {
        throw UnsupportedFeatureError(std::string(fn) + ": B8G8R8A8 UAV is not supported");
    }
    if (format == DXGI_FORMAT_R16G16B16A16_FLOAT && !context.SupportsRgba16FloatUav()) {
        throw UnsupportedFeatureError(std::string(fn) + ": R16G16B16A16_FLOAT UAV is not supported");
    }
}

} // namespace

D3D11TextureViewSet CreateRgbaTextureViewSet(
    D3D11ProcessingContext& context,
    const D3D11Resource& texture,
    bool createSrv,
    bool createUav,
    DXGI_FORMAT viewFormat) {

    constexpr const char* fn = "CreateRgbaTextureViewSet";
    ValidateTexture(texture, fn);
    if (!createSrv && !createUav) {
        throw ValidationError("CreateRgbaTextureViewSet: no view requested");
    }

    const DXGI_FORMAT format = ResolveViewFormat(texture, viewFormat);
    if (!IsRgbaLikeFormat(format)) {
        throw UnsupportedFormatError("CreateRgbaTextureViewSet: unsupported RGBA-like format");
    }
    if (createUav) {
        ValidateUavFlag(texture, fn);
        CheckRgbaUavSupport(context, format, fn);
    }

    D3D11TextureViewSet views;
    if (createSrv) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = format;
        srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MostDetailedMip = 0;
        srv.Texture2D.MipLevels = 1;
        views.srv = CreateSrv(context.Core(), texture.Get(), srv);
    }
    if (createUav) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uav = {};
        uav.Format = format;
        uav.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uav.Texture2D.MipSlice = 0;
        views.uav = CreateUav(context.Core(), texture.Get(), uav);
    }
    return views;
}

D3D11TextureViewSet CreateYuv420SrvViewSet(D3D11ProcessingContext& context, const D3D11Resource& texture) {
    return CreateYuv420SrvUavViewSet(context, texture, true, false);
}

D3D11TextureViewSet CreateYuv420UavViewSet(D3D11ProcessingContext& context, const D3D11Resource& texture) {
    return CreateYuv420SrvUavViewSet(context, texture, false, true);
}

D3D11TextureViewSet CreateYuv420SrvUavViewSet(
    D3D11ProcessingContext& context,
    const D3D11Resource& texture,
    bool createSrv,
    bool createUav) {

    constexpr const char* fn = "CreateYuv420SrvUavViewSet";
    ValidateTexture(texture, fn);
    if (!createSrv && !createUav) {
        throw ValidationError("CreateYuv420SrvUavViewSet: no view requested");
    }

    const auto desc = texture.GetTexture2DDesc();
    const DXGI_FORMAT format = desc.Format;
    if (!IsYuv420Format(format)) {
        throw UnsupportedFormatError("CreateYuv420SrvUavViewSet: texture format must be NV12 or P010");
    }
    ValidateEvenSize(desc.Width, desc.Height, format, fn);

    if (createSrv) {
        if (format == DXGI_FORMAT_NV12 && !context.SupportsNv12Srv()) {
            throw UnsupportedFeatureError("CreateYuv420SrvUavViewSet: NV12 SRV plane views are not supported");
        }
        if (format == DXGI_FORMAT_P010 && !context.SupportsP010Srv()) {
            throw UnsupportedFeatureError("CreateYuv420SrvUavViewSet: P010 SRV plane views are not supported");
        }
    }
    if (createUav) {
        ValidateUavFlag(texture, fn);
        if (format == DXGI_FORMAT_NV12 && !context.SupportsNv12Uav()) {
            throw UnsupportedFeatureError("CreateYuv420SrvUavViewSet: NV12 UAV plane views are not supported");
        }
        if (format == DXGI_FORMAT_P010 && !context.SupportsP010Uav()) {
            throw UnsupportedFeatureError("CreateYuv420SrvUavViewSet: P010 UAV plane views are not supported");
        }
    }

    const DXGI_FORMAT yFormat = (format == DXGI_FORMAT_NV12) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R16_UNORM;
    const DXGI_FORMAT uvFormat = (format == DXGI_FORMAT_NV12) ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R16G16_UNORM;

    D3D11TextureViewSet views;
    if (createSrv) {
        D3D11_SHADER_RESOURCE_VIEW_DESC y = {};
        y.Format = yFormat;
        y.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        y.Texture2D.MostDetailedMip = 0;
        y.Texture2D.MipLevels = 1;
        views.ySrv = CreateSrv(context.Core(), texture.Get(), y);

        D3D11_SHADER_RESOURCE_VIEW_DESC uv = y;
        uv.Format = uvFormat;
        views.uvSrv = CreateSrv(context.Core(), texture.Get(), uv);
    }
    if (createUav) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC y = {};
        y.Format = yFormat;
        y.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        y.Texture2D.MipSlice = 0;
        views.yUav = CreateUav(context.Core(), texture.Get(), y);

        D3D11_UNORDERED_ACCESS_VIEW_DESC uv = y;
        uv.Format = uvFormat;
        views.uvUav = CreateUav(context.Core(), texture.Get(), uv);
    }
    return views;
}

} // namespace Processing
} // namespace D3D11CoreLib
