//
// D3D11FenceSupport.cpp
//
#include <D3D11Helper/D3D11Interop/D3D11FenceSupport.hpp>

namespace D3D11CoreLib {

ComPtr<ID3D11Device5> TryGetD3D11Device5(ID3D11Device* device) noexcept {
    ComPtr<ID3D11Device5> device5;
    if (!device) {
        return nullptr;
    }
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device5)))) {
        return nullptr;
    }
    return device5;
}

ComPtr<ID3D11DeviceContext4> TryGetD3D11DeviceContext4(ID3D11DeviceContext* context) noexcept {
    ComPtr<ID3D11DeviceContext4> context4;
    if (!context) {
        return nullptr;
    }
    if (FAILED(context->QueryInterface(IID_PPV_ARGS(&context4)))) {
        return nullptr;
    }
    return context4;
}

D3D11FenceSupportInfo CheckD3D11FenceSupport(
    ID3D11Device* device,
    ID3D11DeviceContext* context) noexcept {

    D3D11FenceSupportInfo info{};
    info.hasDevice5 = TryGetD3D11Device5(device) != nullptr;
    info.hasContext4 = context ? (TryGetD3D11DeviceContext4(context) != nullptr) : true;

    if (!device) {
        info.supported = false;
        info.reason = "ID3D11Device is null";
        return info;
    }
    if (!info.hasDevice5) {
        info.supported = false;
        info.reason = "ID3D11Device5 is unavailable";
        return info;
    }
    if (context && !info.hasContext4) {
        info.supported = false;
        info.reason = "ID3D11DeviceContext4 is unavailable";
        return info;
    }

    info.supported = true;
    info.reason = context ? "ID3D11Device5 and ID3D11DeviceContext4 are available"
                          : "ID3D11Device5 is available";
    return info;
}

bool IsD3D11FenceSupported(
    ID3D11Device* device,
    ID3D11DeviceContext* context) noexcept {
    return CheckD3D11FenceSupport(device, context).supported;
}

} // namespace D3D11CoreLib
