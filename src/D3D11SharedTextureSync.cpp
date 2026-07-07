//
// D3D11SharedTextureSync.cpp
//
// Compatibility D3D11Gpu helper that creates a D3D11Resource shared Texture2D
// while selecting the sharing synchronization primitive.
//
#include <D3D11Helper/D3D11Gpu/D3D11Helpers.hpp>
#include <D3D11Helper/D3D11Interop/D3D11SharedTexture.hpp>

#include <stdexcept>

namespace D3D11CoreLib {

D3D11Resource CreateSharedTexture2D(D3D11Core& core,
                                     UINT width, UINT height, DXGI_FORMAT format,
                                     UINT bindFlags,
                                     D3D11SharedTextureSyncMode syncMode) {

    D3D11SharedTexture2DDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.bindFlags = bindFlags;
    desc.syncMode = syncMode;

    return D3D11Resource(CreateSharedTexture2D(core.GetDevice(), desc));
}

} // namespace D3D11CoreLib
