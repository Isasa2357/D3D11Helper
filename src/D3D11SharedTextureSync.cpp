//
// D3D11SharedTextureSync.cpp
//
// CreateSharedTexture2D overload that selects the sharing synchronization primitive.
// Kept as a small translation unit so the large existing D3D11Helpers.cpp does not need
// to be overwritten by Explorer-copy based updates.
//
#include <D3D11Helper/D3D11Gpu/D3D11Helpers.hpp>

#include <stdexcept>

namespace D3D11CoreLib {

D3D11Resource CreateSharedTexture2D(D3D11Core& core,
                                     UINT width, UINT height, DXGI_FORMAT format,
                                     UINT bindFlags,
                                     D3D11SharedTextureSyncMode syncMode) {

    UINT miscFlags = 0;
    switch (syncMode) {
    case D3D11SharedTextureSyncMode::SharedFence:
        miscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
        break;
    case D3D11SharedTextureSyncMode::KeyedMutex:
        miscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        break;
    default:
        throw std::runtime_error("CreateSharedTexture2D: unknown syncMode");
    }

    return CreateTexture2D(core, width, height, format,
                           bindFlags,
                           D3D11_USAGE_DEFAULT,
                           miscFlags);
}

} // namespace D3D11CoreLib
