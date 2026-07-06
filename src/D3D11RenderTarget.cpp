//
// D3D11RenderTarget.cpp
//
#include <D3D11Helper/D3D11Presentation/D3D11RenderTarget.hpp>
#include <D3D11Helper/D3D11Foundation/D3D11FormatUtil.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Helpers.hpp>

#include <stdexcept>
#include <string>

namespace D3D11CoreLib {
namespace {

void ThrowIfNull(ID3D11DeviceContext* context, const char* name) {
    if (!context) {
        throw std::runtime_error(std::string(name) + ": null context");
    }
}

bool HasStencilComponent(DXGI_FORMAT format) noexcept {
    switch (format) {
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return true;
    default:
        return false;
    }
}

void ValidateRenderTargetDesc(const D3D11RenderTargetDesc& desc) {
    if (desc.width == 0 || desc.height == 0) {
        throw std::runtime_error("D3D11RenderTarget: width and height must be > 0");
    }
    if (desc.colorFormat == DXGI_FORMAT_UNKNOWN) {
        throw std::runtime_error("D3D11RenderTarget: colorFormat must not be UNKNOWN");
    }
    if (FormatUtil::IsDepthFormat(desc.colorFormat)) {
        throw std::runtime_error("D3D11RenderTarget: colorFormat must not be a depth format");
    }
    if (FormatUtil::IsBlockCompressedFormat(desc.colorFormat) ||
        FormatUtil::IsPlanarFormat(desc.colorFormat)) {
        throw std::runtime_error("D3D11RenderTarget: colorFormat must be render-target compatible single-plane format");
    }
    if (desc.depthFormat != DXGI_FORMAT_UNKNOWN &&
        !FormatUtil::IsDepthFormat(desc.depthFormat)) {
        throw std::runtime_error("D3D11RenderTarget: depthFormat must be a depth/stencil format or UNKNOWN");
    }
    if (desc.sampleCount != 1 || desc.sampleQuality != 0) {
        throw std::runtime_error("D3D11RenderTarget: v1.3.0 step 1 supports only non-MSAA render targets");
    }
}

} // unnamed namespace

D3D11_VIEWPORT MakeViewport(UINT width,
                            UINT height,
                            FLOAT topLeftX,
                            FLOAT topLeftY,
                            FLOAT minDepth,
                            FLOAT maxDepth) noexcept {
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = topLeftX;
    vp.TopLeftY = topLeftY;
    vp.Width = static_cast<FLOAT>(width);
    vp.Height = static_cast<FLOAT>(height);
    vp.MinDepth = minDepth;
    vp.MaxDepth = maxDepth;
    return vp;
}

void SetViewport(ID3D11DeviceContext* context,
                 UINT width,
                 UINT height,
                 FLOAT topLeftX,
                 FLOAT topLeftY,
                 FLOAT minDepth,
                 FLOAT maxDepth) {
    ThrowIfNull(context, "SetViewport");
    const D3D11_VIEWPORT vp = MakeViewport(width, height, topLeftX, topLeftY, minDepth, maxDepth);
    context->RSSetViewports(1, &vp);
}

void UnbindRenderTargets(ID3D11DeviceContext* context, UINT renderTargetCount) {
    ThrowIfNull(context, "UnbindRenderTargets");

    if (renderTargetCount == 0) {
        context->OMSetRenderTargets(0, nullptr, nullptr);
        return;
    }
    if (renderTargetCount > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT) {
        throw std::runtime_error("UnbindRenderTargets: renderTargetCount exceeds D3D11 limit");
    }

    ID3D11RenderTargetView* nullRtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    context->OMSetRenderTargets(renderTargetCount, nullRtvs, nullptr);
}

void D3D11RenderTarget::Initialize(D3D11Core& core, const D3D11RenderTargetDesc& desc) {
    ValidateRenderTargetDesc(desc);
    Reset();
    m_desc = desc;

    UINT colorBindFlags = D3D11_BIND_RENDER_TARGET;
    if (desc.createShaderResource) {
        colorBindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }

    m_colorTexture = CreateTexture2D(
        core,
        desc.width,
        desc.height,
        desc.colorFormat,
        colorBindFlags,
        D3D11_USAGE_DEFAULT,
        0,
        1,
        1);

    m_rtv = CreateTexture2DRtv(core, m_colorTexture, desc.colorFormat);

    if (desc.createShaderResource) {
        m_srv = CreateTexture2DSrv(core, m_colorTexture, desc.colorFormat);
    }

    if (desc.depthFormat != DXGI_FORMAT_UNKNOWN) {
        m_depthTexture = CreateTexture2D(
            core,
            desc.width,
            desc.height,
            desc.depthFormat,
            D3D11_BIND_DEPTH_STENCIL,
            D3D11_USAGE_DEFAULT,
            0,
            1,
            1);
        m_dsv = CreateTexture2DDsv(core, m_depthTexture, desc.depthFormat);
    }
}

void D3D11RenderTarget::Reset() noexcept {
    m_dsv.Reset();
    m_srv.Reset();
    m_rtv.Reset();
    m_depthTexture = D3D11Resource{};
    m_colorTexture = D3D11Resource{};
    m_desc = D3D11RenderTargetDesc{};
}

D3D11_VIEWPORT D3D11RenderTarget::Viewport(FLOAT topLeftX,
                                           FLOAT topLeftY,
                                           FLOAT minDepth,
                                           FLOAT maxDepth) const noexcept {
    return MakeViewport(m_desc.width, m_desc.height, topLeftX, topLeftY, minDepth, maxDepth);
}

void D3D11RenderTarget::Bind(ID3D11DeviceContext* context) const {
    ThrowIfNull(context, "D3D11RenderTarget::Bind");
    if (!m_rtv) {
        throw std::runtime_error("D3D11RenderTarget::Bind: render target is not initialized");
    }

    ID3D11RenderTargetView* rtv = m_rtv.Get();
    context->OMSetRenderTargets(1, &rtv, m_dsv.Get());
}

void D3D11RenderTarget::SetViewport(ID3D11DeviceContext* context) const {
    ThrowIfNull(context, "D3D11RenderTarget::SetViewport");
    if (!IsValid()) {
        throw std::runtime_error("D3D11RenderTarget::SetViewport: render target is not initialized");
    }

    const D3D11_VIEWPORT vp = Viewport();
    context->RSSetViewports(1, &vp);
}

void D3D11RenderTarget::BindAndSetViewport(ID3D11DeviceContext* context) const {
    Bind(context);
    SetViewport(context);
}

void D3D11RenderTarget::ClearColor(ID3D11DeviceContext* context) const {
    ThrowIfNull(context, "D3D11RenderTarget::ClearColor");
    if (!m_rtv) {
        throw std::runtime_error("D3D11RenderTarget::ClearColor: render target is not initialized");
    }

    context->ClearRenderTargetView(m_rtv.Get(), m_desc.clearColor);
}

void D3D11RenderTarget::ClearDepthStencil(ID3D11DeviceContext* context) const {
    ThrowIfNull(context, "D3D11RenderTarget::ClearDepthStencil");
    if (!m_dsv) {
        return;
    }

    UINT clearFlags = D3D11_CLEAR_DEPTH;
    if (HasStencilComponent(m_desc.depthFormat)) {
        clearFlags |= D3D11_CLEAR_STENCIL;
    }

    context->ClearDepthStencilView(m_dsv.Get(), clearFlags, m_desc.clearDepth, m_desc.clearStencil);
}

void D3D11RenderTarget::Clear(ID3D11DeviceContext* context) const {
    ClearColor(context);
    ClearDepthStencil(context);
}

D3D11RenderTarget CreateRenderTarget(D3D11Core& core, const D3D11RenderTargetDesc& desc) {
    D3D11RenderTarget target;
    target.Initialize(core, desc);
    return target;
}

D3D11RenderTarget CreateRenderTarget(D3D11Core& core,
                                     UINT width,
                                     UINT height,
                                     DXGI_FORMAT colorFormat,
                                     bool createDepth,
                                     DXGI_FORMAT depthFormat,
                                     bool createShaderResource) {
    D3D11RenderTargetDesc desc = {};
    desc.width = width;
    desc.height = height;
    desc.colorFormat = colorFormat;
    desc.depthFormat = createDepth ? depthFormat : DXGI_FORMAT_UNKNOWN;
    desc.createShaderResource = createShaderResource;
    return CreateRenderTarget(core, desc);
}

} // namespace D3D11CoreLib
