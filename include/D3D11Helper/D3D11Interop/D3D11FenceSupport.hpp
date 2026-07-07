#pragma once
//
// D3D11FenceSupport.hpp - D3D11.4 fence capability helpers.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

struct D3D11FenceSupportInfo {
    bool hasDevice5 = false;
    bool hasContext4 = false;
    bool supported = false;
    const char* reason = "not checked";
};

ComPtr<ID3D11Device5> TryGetD3D11Device5(ID3D11Device* device) noexcept;
ComPtr<ID3D11DeviceContext4> TryGetD3D11DeviceContext4(ID3D11DeviceContext* context) noexcept;

D3D11FenceSupportInfo CheckD3D11FenceSupport(
    ID3D11Device* device,
    ID3D11DeviceContext* context = nullptr) noexcept;

bool IsD3D11FenceSupported(
    ID3D11Device* device,
    ID3D11DeviceContext* context = nullptr) noexcept;

} // namespace D3D11CoreLib
