//
// D3D11DeviceLost.cpp
//
#include <D3D11Helper/D3D11Diagnostics/D3D11DeviceLost.hpp>

#include <stdexcept>

namespace D3D11CoreLib {

bool IsDeviceLost(HRESULT hr) noexcept {
    switch (hr) {
    case DXGI_ERROR_DEVICE_HUNG:
    case DXGI_ERROR_DEVICE_REMOVED:
    case DXGI_ERROR_DEVICE_RESET:
    case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
    case DXGI_ERROR_INVALID_CALL:
    case E_OUTOFMEMORY:
        return true;
    default:
        return false;
    }
}

const char* DeviceLostReasonName(HRESULT hr) noexcept {
    switch (hr) {
    case S_OK: return "S_OK";
    case DXGI_ERROR_DEVICE_HUNG: return "DXGI_ERROR_DEVICE_HUNG";
    case DXGI_ERROR_DEVICE_REMOVED: return "DXGI_ERROR_DEVICE_REMOVED";
    case DXGI_ERROR_DEVICE_RESET: return "DXGI_ERROR_DEVICE_RESET";
    case DXGI_ERROR_DRIVER_INTERNAL_ERROR: return "DXGI_ERROR_DRIVER_INTERNAL_ERROR";
    case DXGI_ERROR_INVALID_CALL: return "DXGI_ERROR_INVALID_CALL";
    case E_OUTOFMEMORY: return "E_OUTOFMEMORY";
    default: return "UNKNOWN_HRESULT";
    }
}

D3D11DeviceLostInfo CheckDeviceLost(ID3D11Device* device) {
    if (!device) {
        throw std::invalid_argument("CheckDeviceLost requires a non-null device");
    }

    D3D11DeviceLostInfo info{};
    info.reason = device->GetDeviceRemovedReason();
    info.deviceLost = IsDeviceLost(info.reason);
    info.name = DeviceLostReasonName(info.reason);
    return info;
}

void ThrowIfDeviceLost(ID3D11Device* device) {
    const auto info = CheckDeviceLost(device);
    if (info.deviceLost) {
        throw std::runtime_error(info.name);
    }
}

} // namespace D3D11CoreLib
