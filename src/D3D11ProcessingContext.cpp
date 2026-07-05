#include "D3D11Processing/D3D11ProcessingContext.hpp"
#include "D3D11Core/ThrowIfFailed.hpp"

#include <cstring>
#include <stdexcept>
#include <utility>

namespace D3D11CoreLib {
namespace Processing {
namespace {

bool HasSupport(UINT value, UINT flag) noexcept {
    return (value & flag) == flag;
}

UINT QueryFormatSupport(ID3D11Device* device, DXGI_FORMAT format) {
    UINT support = 0;
    if (!device || FAILED(device->CheckFormatSupport(format, &support))) {
        return 0;
    }
    return support;
}

constexpr UINT kProcessingConstantBufferBytes = 256;

} // namespace

void D3D11ProcessingContext::Initialize(D3D11Core& core, std::filesystem::path shaderDirectory) {
    if (!core.GetDevice()) {
        throw ValidationError("D3D11ProcessingContext::Initialize: core has no device");
    }
    m_core = &core;
    if (shaderDirectory.empty()) {
        shaderDirectory = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    }
    m_shaderDirectory = std::move(shaderDirectory);
    m_caps = QueryCaps(core.GetDevice());
    CreateConstantBuffer();
}

D3D11Core& D3D11ProcessingContext::Core() const {
    if (!m_core) {
        throw ValidationError("D3D11ProcessingContext::Core: context is not initialized");
    }
    return *m_core;
}

ID3D11Device* D3D11ProcessingContext::GetDevice() const {
    return Core().GetDevice();
}

void D3D11ProcessingContext::CreateConstantBuffer() {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = kProcessingConstantBufferBytes;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11CORE_THROW_IF_FAILED(GetDevice()->CreateBuffer(&desc, nullptr, &m_constantBuffer));
}

void D3D11ProcessingContext::UpdateConstants(ID3D11DeviceContext* context, const void* data, UINT sizeBytes) const {
    if (!context) {
        throw ValidationError("D3D11ProcessingContext::UpdateConstants: null device context");
    }
    if (!data || sizeBytes == 0 || sizeBytes > kProcessingConstantBufferBytes) {
        throw ValidationError("D3D11ProcessingContext::UpdateConstants: invalid constant data");
    }
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    D3D11CORE_THROW_IF_FAILED(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
    std::memcpy(mapped.pData, data, sizeBytes);
    context->Unmap(m_constantBuffer.Get(), 0);
}

D3D11ProcessingCaps D3D11ProcessingContext::QueryCaps(ID3D11Device* device) const {
    D3D11ProcessingCaps caps = {};
    const UINT rgba = QueryFormatSupport(device, DXGI_FORMAT_R8G8B8A8_UNORM);
    const UINT bgra = QueryFormatSupport(device, DXGI_FORMAT_B8G8R8A8_UNORM);
    const UINT rgba16 = QueryFormatSupport(device, DXGI_FORMAT_R16G16B16A16_FLOAT);
    caps.rgba8Uav = HasSupport(rgba, D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW);
    caps.bgra8Uav = HasSupport(bgra, D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW);
    caps.rgba16FloatUav = HasSupport(rgba16, D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW);

    const UINT nv12 = QueryFormatSupport(device, DXGI_FORMAT_NV12);
    const UINT p010 = QueryFormatSupport(device, DXGI_FORMAT_P010);
    const UINT r8 = QueryFormatSupport(device, DXGI_FORMAT_R8_UNORM);
    const UINT rg8 = QueryFormatSupport(device, DXGI_FORMAT_R8G8_UNORM);
    const UINT r16 = QueryFormatSupport(device, DXGI_FORMAT_R16_UNORM);
    const UINT rg16 = QueryFormatSupport(device, DXGI_FORMAT_R16G16_UNORM);

    caps.nv12Srv = HasSupport(nv12, D3D11_FORMAT_SUPPORT_TEXTURE2D) &&
                   HasSupport(r8, D3D11_FORMAT_SUPPORT_SHADER_LOAD) &&
                   HasSupport(rg8, D3D11_FORMAT_SUPPORT_SHADER_LOAD);
    caps.nv12Uav = HasSupport(nv12, D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW);
    caps.p010Srv = HasSupport(p010, D3D11_FORMAT_SUPPORT_TEXTURE2D) &&
                   HasSupport(r16, D3D11_FORMAT_SUPPORT_SHADER_LOAD) &&
                   HasSupport(rg16, D3D11_FORMAT_SUPPORT_SHADER_LOAD);
    caps.p010Uav = HasSupport(p010, D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW);
    caps.r8Uav = HasSupport(r8, D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW);
    caps.rg8Uav = HasSupport(rg8, D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW);
    caps.r16Uav = HasSupport(r16, D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW);
    caps.rg16Uav = HasSupport(rg16, D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW);

    caps.nv12Uav = caps.nv12Uav && caps.r8Uav && caps.rg8Uav;
    caps.p010Uav = caps.p010Uav && caps.r16Uav && caps.rg16Uav;
    return caps;
}

} // namespace Processing
} // namespace D3D11CoreLib
