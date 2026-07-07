#pragma once
//
// D3D11State.hpp - D3D11 state object creation and descriptor presets
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

ComPtr<ID3D11SamplerState> CreateSamplerState(
    ID3D11Device* device,
    const D3D11_SAMPLER_DESC& desc);

ComPtr<ID3D11RasterizerState> CreateRasterizerState(
    ID3D11Device* device,
    const D3D11_RASTERIZER_DESC& desc);

ComPtr<ID3D11BlendState> CreateBlendState(
    ID3D11Device* device,
    const D3D11_BLEND_DESC& desc);

ComPtr<ID3D11DepthStencilState> CreateDepthStencilState(
    ID3D11Device* device,
    const D3D11_DEPTH_STENCIL_DESC& desc);

namespace StatePresets {

D3D11_SAMPLER_DESC SamplerPointClamp();
D3D11_SAMPLER_DESC SamplerPointWrap();
D3D11_SAMPLER_DESC SamplerLinearClamp();
D3D11_SAMPLER_DESC SamplerLinearWrap();
D3D11_SAMPLER_DESC SamplerAnisotropicWrap(UINT maxAnisotropy = 16);
D3D11_SAMPLER_DESC SamplerComparisonLinearClamp(
    D3D11_COMPARISON_FUNC comparison = D3D11_COMPARISON_LESS_EQUAL);

D3D11_RASTERIZER_DESC RasterizerCullBack(
    bool frontCounterClockwise = false);
D3D11_RASTERIZER_DESC RasterizerCullNone();
D3D11_RASTERIZER_DESC RasterizerWireframe();
D3D11_RASTERIZER_DESC RasterizerScissorCullBack(
    bool frontCounterClockwise = false);

D3D11_BLEND_DESC BlendOpaque();
D3D11_BLEND_DESC BlendAlpha();
D3D11_BLEND_DESC BlendPremultipliedAlpha();
D3D11_BLEND_DESC BlendAdditive();

D3D11_DEPTH_STENCIL_DESC DepthDisabled();
D3D11_DEPTH_STENCIL_DESC DepthDefault(
    bool depthWrite = true,
    D3D11_COMPARISON_FUNC depthFunc = D3D11_COMPARISON_LESS);
D3D11_DEPTH_STENCIL_DESC DepthReadOnly(
    D3D11_COMPARISON_FUNC depthFunc = D3D11_COMPARISON_LESS_EQUAL);
D3D11_DEPTH_STENCIL_DESC DepthReverseZ(bool depthWrite = true);

} // namespace StatePresets
} // namespace D3D11CoreLib
