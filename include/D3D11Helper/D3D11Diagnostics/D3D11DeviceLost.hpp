#pragma once
//
// D3D11DeviceLost.hpp - device lost diagnostics helpers
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

struct D3D11DeviceLostInfo {
    HRESULT reason = S_OK;
    bool deviceLost = false;
    const char* name = "S_OK";
};

bool IsDeviceLost(HRESULT hr) noexcept;
const char* DeviceLostReasonName(HRESULT hr) noexcept;
D3D11DeviceLostInfo CheckDeviceLost(ID3D11Device* device);
void ThrowIfDeviceLost(ID3D11Device* device);

} // namespace D3D11CoreLib
