#pragma once
//
// D3D11ProcessingContext.hpp
// Device-owned shared state for D3D11 Processing dispatches.
//
#include "D3D11ProcessingTypes.hpp"
#include "../D3D11Core/D3D11Core.hpp"

#include <filesystem>

namespace D3D11CoreLib {
namespace Processing {

class D3D11ProcessingContext {
public:
    void Initialize(D3D11Core& core, std::filesystem::path shaderDirectory = {});

    D3D11Core& Core() const;
    ID3D11Device* GetDevice() const;

    const std::filesystem::path& ShaderDirectory() const noexcept { return m_shaderDirectory; }
    const D3D11ProcessingCaps& Caps() const noexcept { return m_caps; }

    bool SupportsRgba8Uav() const noexcept { return m_caps.rgba8Uav; }
    bool SupportsBgra8Uav() const noexcept { return m_caps.bgra8Uav; }
    bool SupportsRgba16FloatUav() const noexcept { return m_caps.rgba16FloatUav; }
    bool SupportsNv12Srv() const noexcept { return m_caps.nv12Srv; }
    bool SupportsNv12Uav() const noexcept { return m_caps.nv12Uav; }
    bool SupportsP010Srv() const noexcept { return m_caps.p010Srv; }
    bool SupportsP010Uav() const noexcept { return m_caps.p010Uav; }

    ID3D11Buffer* ConstantBuffer() const noexcept { return m_constantBuffer.Get(); }
    void UpdateConstants(ID3D11DeviceContext* context, const void* data, UINT sizeBytes) const;

private:
    D3D11ProcessingCaps QueryCaps(ID3D11Device* device) const;
    void CreateConstantBuffer();

    D3D11Core* m_core = nullptr;
    std::filesystem::path m_shaderDirectory;
    D3D11ProcessingCaps m_caps = {};
    ComPtr<ID3D11Buffer> m_constantBuffer;
};

} // namespace Processing
} // namespace D3D11CoreLib
