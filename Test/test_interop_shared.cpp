//
// test_interop_shared.cpp - v1.8.0 Round 1 shared handle/texture tests
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <stdexcept>
#include <string>

using namespace D3D11CoreLib;

namespace {

void ExpectThrows(const char* label, const std::function<void()>& fn) {
    bool threw = false;
    try {
        fn();
    } catch (...) {
        threw = true;
    }
    if (!threw) {
        throw std::runtime_error(std::string(label) + " did not throw");
    }
}

D3D11SharedTexture2DDesc MakeDesc(
    UINT width = 64,
    UINT height = 64,
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM) {
    D3D11SharedTexture2DDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.format = format;
    return desc;
}

D3D11_TEXTURE2D_DESC GetDesc(ID3D11Texture2D* texture) {
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    return desc;
}

bool HasKeyedMutex(ID3D11Resource* resource) {
    ComPtr<IDXGIKeyedMutex> mutex;
    return SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&mutex)));
}

} // namespace

int main() {
    auto core = TestUtil::MakeCore();
    ID3D11Device* device = core->GetDevice();

    TEST_RUN("D3D11SharedHandle ownership and move", {
        HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!eventHandle) {
            throw std::runtime_error("CreateEventW failed");
        }

        D3D11SharedHandle owned(eventHandle);
        if (!owned) {
            throw std::runtime_error("owned event handle is invalid");
        }

        auto duplicated = owned.Duplicate();
        if (!duplicated) {
            throw std::runtime_error("duplicated handle is invalid");
        }

        HANDLE rawDuplicate = duplicated.Release();
        if (!rawDuplicate) {
            throw std::runtime_error("released duplicate is null");
        }
        CloseHandle(rawDuplicate);
        if (duplicated) {
            throw std::runtime_error("released handle still reports valid");
        }

        D3D11SharedHandle moved(std::move(owned));
        if (owned) {
            throw std::runtime_error("moved-from handle still valid");
        }
        if (!moved) {
            throw std::runtime_error("moved-to handle invalid");
        }

        moved.Reset();
        if (moved) {
            throw std::runtime_error("reset handle still valid");
        }
    });

    TEST_RUN("SharedFence texture creation and owned handle open", {
        auto desc = MakeDesc(64, 32, DXGI_FORMAT_R8G8B8A8_UNORM);
        desc.syncMode = D3D11SharedTextureSyncMode::SharedFence;

        auto shared = CreateSharedTexture2DWithHandle(device, desc);
        if (!shared) {
            throw std::runtime_error("shared texture object is invalid");
        }

        const auto createdDesc = GetDesc(shared.Get());
        if ((createdDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) == 0 ||
            (createdDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED) == 0 ||
            (createdDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0) {
            throw std::runtime_error("unexpected SharedFence misc flags");
        }

        auto opened = OpenSharedTexture2D(device, shared.handle);
        const auto openedDesc = GetDesc(opened.Get());
        if (openedDesc.Width != 64 || openedDesc.Height != 32 || openedDesc.Format != desc.format) {
            throw std::runtime_error("opened texture desc mismatch");
        }
    });

    TEST_RUN("D3D11SharedResource owned handle compatibility", {
        auto desc = MakeDesc(48, 48, DXGI_FORMAT_B8G8R8A8_UNORM);
        auto texture = CreateSharedTexture2D(device, desc);
        auto owned = D3D11SharedResource::CreateSharedHandleOwned(texture.Get());
        if (!owned) {
            throw std::runtime_error("owned handle is invalid");
        }

        auto opened = D3D11SharedResource::OpenSharedTexture2D(device, owned.Get());
        const auto openedDesc = GetDesc(opened.Get());
        if (openedDesc.Width != 48 || openedDesc.Height != 48) {
            throw std::runtime_error("compat open texture size mismatch");
        }
    });

    TEST_RUN("KeyedMutex shared texture creation", {
        auto desc = MakeDesc(32, 32, DXGI_FORMAT_R8G8B8A8_UNORM);
        desc.syncMode = D3D11SharedTextureSyncMode::KeyedMutex;

        auto texture = CreateSharedTexture2D(device, desc);
        const auto textureDesc = GetDesc(texture.Get());
        if ((textureDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) == 0 ||
            (textureDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) == 0 ||
            (textureDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED) != 0) {
            throw std::runtime_error("unexpected KeyedMutex misc flags");
        }
        if (!HasKeyedMutex(texture.Get())) {
            throw std::runtime_error("texture does not expose IDXGIKeyedMutex");
        }
    });

    TEST_RUN("Existing D3D11Gpu CreateSharedTexture2D helper", {
        auto fenceTexture = CreateSharedTexture2D(
            *core,
            24,
            24,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            D3D11SharedTextureSyncMode::SharedFence);
        auto fenceDesc = fenceTexture.GetTexture2DDesc();
        if ((fenceDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED) == 0) {
            throw std::runtime_error("SharedFence helper missing SHARED flag");
        }

        auto keyedTexture = CreateSharedTexture2D(
            *core,
            24,
            24,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            D3D11SharedTextureSyncMode::KeyedMutex);
        auto keyedDesc = keyedTexture.GetTexture2DDesc();
        if ((keyedDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) == 0) {
            throw std::runtime_error("KeyedMutex helper missing keyed mutex flag");
        }
        if (!HasKeyedMutex(keyedTexture.Get())) {
            throw std::runtime_error("KeyedMutex helper texture has no IDXGIKeyedMutex");
        }
    });

    TEST_RUN("Shared texture validation", {
        ExpectThrows("zero width", [&] {
            auto desc = MakeDesc();
            desc.width = 0;
            (void)CreateSharedTexture2D(device, desc);
        });
        ExpectThrows("unknown format", [&] {
            auto desc = MakeDesc();
            desc.format = DXGI_FORMAT_UNKNOWN;
            (void)CreateSharedTexture2D(device, desc);
        });
        ExpectThrows("invalid misc sharing flags", [&] {
            auto desc = MakeDesc();
            desc.miscFlags = D3D11_RESOURCE_MISC_SHARED;
            (void)CreateSharedTexture2D(device, desc);
        });
        ExpectThrows("multisample mip chain", [&] {
            auto desc = MakeDesc();
            desc.sampleCount = 2;
            desc.mipLevels = 2;
            (void)CreateSharedTexture2D(device, desc);
        });
        ExpectThrows("invalid handle open", [&] {
            D3D11SharedHandle invalid;
            (void)OpenSharedTexture2D(device, invalid);
        });
    });

    return TestUtil::Result("InteropShared");
}
