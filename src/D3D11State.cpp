//
// D3D11State.cpp
//
#include <D3D11Helper/D3D11Gpu/D3D11State.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace D3D11CoreLib {
namespace {

void RequireDevice(ID3D11Device* device, const char* functionName) {
    if (!device) {
        throw std::invalid_argument(std::string(functionName) + ": device is null");
    }
}

D3D11_SAMPLER_DESC MakeSampler(
    D3D11_FILTER filter,
    D3D11_TEXTURE_ADDRESS_MODE address,
    UINT maxAnisotropy = 1,
    D3D11_COMPARISON_FUNC comparison = D3D11_COMPARISON_NEVER) {
    D3D11_SAMPLER_DESC desc{};
    desc.Filter = filter;
    desc.AddressU = address;
    desc.AddressV = address;
    desc.AddressW = address;
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = std::max<UINT>(1u, std::min<UINT>(maxAnisotropy, D3D11_REQ_MAXANISOTROPY));
    desc.ComparisonFunc = comparison;
    desc.BorderColor[0] = 0.0f;
    desc.BorderColor[1] = 0.0f;
    desc.BorderColor[2] = 0.0f;
    desc.BorderColor[3] = 0.0f;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D11_FLOAT32_MAX;
    return desc;
}

D3D11_RASTERIZER_DESC MakeRasterizer(
    D3D11_FILL_MODE fill,
    D3D11_CULL_MODE cull,
    bool frontCounterClockwise,
    bool scissorEnable) {
    D3D11_RASTERIZER_DESC desc{};
    desc.FillMode = fill;
    desc.CullMode = cull;
    desc.FrontCounterClockwise = frontCounterClockwise ? TRUE : FALSE;
    desc.DepthBias = 0;
    desc.DepthBiasClamp = 0.0f;
    desc.SlopeScaledDepthBias = 0.0f;
    desc.DepthClipEnable = TRUE;
    desc.ScissorEnable = scissorEnable ? TRUE : FALSE;
    desc.MultisampleEnable = FALSE;
    desc.AntialiasedLineEnable = FALSE;
    return desc;
}

D3D11_RENDER_TARGET_BLEND_DESC MakeOpaqueRenderTargetBlend() {
    D3D11_RENDER_TARGET_BLEND_DESC rt{};
    rt.BlendEnable = FALSE;
    rt.SrcBlend = D3D11_BLEND_ONE;
    rt.DestBlend = D3D11_BLEND_ZERO;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_ZERO;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    return rt;
}

D3D11_BLEND_DESC MakeBlendBase() {
    D3D11_BLEND_DESC desc{};
    desc.AlphaToCoverageEnable = FALSE;
    desc.IndependentBlendEnable = FALSE;
    for (auto& rt : desc.RenderTarget) {
        rt = MakeOpaqueRenderTargetBlend();
    }
    return desc;
}

D3D11_DEPTH_STENCILOP_DESC MakeStencilFaceDefaults() {
    D3D11_DEPTH_STENCILOP_DESC desc{};
    desc.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    desc.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    desc.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    desc.StencilFunc = D3D11_COMPARISON_ALWAYS;
    return desc;
}

D3D11_DEPTH_STENCIL_DESC MakeDepthBase(bool enable, bool write, D3D11_COMPARISON_FUNC func) {
    D3D11_DEPTH_STENCIL_DESC desc{};
    desc.DepthEnable = enable ? TRUE : FALSE;
    desc.DepthWriteMask = write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = func;
    desc.StencilEnable = FALSE;
    desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    desc.FrontFace = MakeStencilFaceDefaults();
    desc.BackFace = MakeStencilFaceDefaults();
    return desc;
}

} // namespace

ComPtr<ID3D11SamplerState> CreateSamplerState(ID3D11Device* device, const D3D11_SAMPLER_DESC& desc) {
    RequireDevice(device, "CreateSamplerState");
    ComPtr<ID3D11SamplerState> state;
    D3D11CORE_THROW_IF_FAILED(device->CreateSamplerState(&desc, &state));
    return state;
}

ComPtr<ID3D11RasterizerState> CreateRasterizerState(ID3D11Device* device, const D3D11_RASTERIZER_DESC& desc) {
    RequireDevice(device, "CreateRasterizerState");
    ComPtr<ID3D11RasterizerState> state;
    D3D11CORE_THROW_IF_FAILED(device->CreateRasterizerState(&desc, &state));
    return state;
}

ComPtr<ID3D11BlendState> CreateBlendState(ID3D11Device* device, const D3D11_BLEND_DESC& desc) {
    RequireDevice(device, "CreateBlendState");
    ComPtr<ID3D11BlendState> state;
    D3D11CORE_THROW_IF_FAILED(device->CreateBlendState(&desc, &state));
    return state;
}

ComPtr<ID3D11DepthStencilState> CreateDepthStencilState(ID3D11Device* device, const D3D11_DEPTH_STENCIL_DESC& desc) {
    RequireDevice(device, "CreateDepthStencilState");
    ComPtr<ID3D11DepthStencilState> state;
    D3D11CORE_THROW_IF_FAILED(device->CreateDepthStencilState(&desc, &state));
    return state;
}

namespace StatePresets {

D3D11_SAMPLER_DESC SamplerPointClamp() {
    return MakeSampler(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
}

D3D11_SAMPLER_DESC SamplerPointWrap() {
    return MakeSampler(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_WRAP);
}

D3D11_SAMPLER_DESC SamplerLinearClamp() {
    return MakeSampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
}

D3D11_SAMPLER_DESC SamplerLinearWrap() {
    return MakeSampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
}

D3D11_SAMPLER_DESC SamplerAnisotropicWrap(UINT maxAnisotropy) {
    return MakeSampler(D3D11_FILTER_ANISOTROPIC, D3D11_TEXTURE_ADDRESS_WRAP, maxAnisotropy);
}

D3D11_SAMPLER_DESC SamplerComparisonLinearClamp(D3D11_COMPARISON_FUNC comparison) {
    return MakeSampler(D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
                       D3D11_TEXTURE_ADDRESS_CLAMP,
                       1,
                       comparison);
}

D3D11_RASTERIZER_DESC RasterizerCullBack(bool frontCounterClockwise) {
    return MakeRasterizer(D3D11_FILL_SOLID, D3D11_CULL_BACK, frontCounterClockwise, false);
}

D3D11_RASTERIZER_DESC RasterizerCullNone() {
    return MakeRasterizer(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, false);
}

D3D11_RASTERIZER_DESC RasterizerWireframe() {
    return MakeRasterizer(D3D11_FILL_WIREFRAME, D3D11_CULL_BACK, false, false);
}

D3D11_RASTERIZER_DESC RasterizerScissorCullBack(bool frontCounterClockwise) {
    return MakeRasterizer(D3D11_FILL_SOLID, D3D11_CULL_BACK, frontCounterClockwise, true);
}

D3D11_BLEND_DESC BlendOpaque() {
    return MakeBlendBase();
}

D3D11_BLEND_DESC BlendAlpha() {
    auto desc = MakeBlendBase();
    auto& rt = desc.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    return desc;
}

D3D11_BLEND_DESC BlendPremultipliedAlpha() {
    auto desc = MakeBlendBase();
    auto& rt = desc.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_ONE;
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    return desc;
}

D3D11_BLEND_DESC BlendAdditive() {
    auto desc = MakeBlendBase();
    auto& rt = desc.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D11_BLEND_ONE;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_ONE;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    return desc;
}

D3D11_DEPTH_STENCIL_DESC DepthDisabled() {
    return MakeDepthBase(false, false, D3D11_COMPARISON_ALWAYS);
}

D3D11_DEPTH_STENCIL_DESC DepthDefault(bool depthWrite, D3D11_COMPARISON_FUNC depthFunc) {
    return MakeDepthBase(true, depthWrite, depthFunc);
}

D3D11_DEPTH_STENCIL_DESC DepthReadOnly(D3D11_COMPARISON_FUNC depthFunc) {
    return MakeDepthBase(true, false, depthFunc);
}

D3D11_DEPTH_STENCIL_DESC DepthReverseZ(bool depthWrite) {
    return MakeDepthBase(true, depthWrite, D3D11_COMPARISON_GREATER_EQUAL);
}

} // namespace StatePresets
} // namespace D3D11CoreLib
