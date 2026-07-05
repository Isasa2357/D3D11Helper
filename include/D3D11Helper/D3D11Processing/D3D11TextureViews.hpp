#pragma once
//
// D3D11TextureViews.hpp
// Processing-specific SRV/UAV view sets built on D3D11 view objects.
//
#include "D3D11ProcessingContext.hpp"
#include "../D3D11Framework/D3D11Helpers.hpp"
#include "../D3D11Framework/D3D11Resource.hpp"

namespace D3D11CoreLib {
namespace Processing {

struct D3D11TextureViewSet {
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11UnorderedAccessView> uav;
    ComPtr<ID3D11ShaderResourceView> ySrv;
    ComPtr<ID3D11ShaderResourceView> uvSrv;
    ComPtr<ID3D11UnorderedAccessView> yUav;
    ComPtr<ID3D11UnorderedAccessView> uvUav;

    bool HasSrv() const noexcept { return srv != nullptr; }
    bool HasUav() const noexcept { return uav != nullptr; }
    bool HasYuv420Srv() const noexcept { return ySrv != nullptr && uvSrv != nullptr; }
    bool HasYuv420Uav() const noexcept { return yUav != nullptr && uvUav != nullptr; }
};

D3D11TextureViewSet CreateRgbaTextureViewSet(
    D3D11ProcessingContext& context,
    const D3D11Resource& texture,
    bool createSrv,
    bool createUav,
    DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN);

D3D11TextureViewSet CreateYuv420SrvViewSet(
    D3D11ProcessingContext& context,
    const D3D11Resource& texture);

D3D11TextureViewSet CreateYuv420UavViewSet(
    D3D11ProcessingContext& context,
    const D3D11Resource& texture);

D3D11TextureViewSet CreateYuv420SrvUavViewSet(
    D3D11ProcessingContext& context,
    const D3D11Resource& texture,
    bool createSrv,
    bool createUav);

} // namespace Processing
} // namespace D3D11CoreLib
