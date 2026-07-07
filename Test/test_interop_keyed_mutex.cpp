//
// test_interop_keyed_mutex.cpp - v1.8.0 Round 2 keyed mutex tests
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

D3D11SharedTexture2DDesc MakeKeyedDesc(UINT width = 32, UINT height = 32) {
    D3D11SharedTexture2DDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.syncMode = D3D11SharedTextureSyncMode::KeyedMutex;
    return desc;
}

ComPtr<ID3D11Texture2D> CreateNormalTexture(ID3D11Device* device) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 16;
    desc.Height = 16;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, nullptr, &texture));
    return texture;
}

} // namespace

int main() {
    auto core = TestUtil::MakeCore();
    ID3D11Device* device = core->GetDevice();

    TEST_RUN("GetKeyedMutex and attach", {
        auto texture = CreateSharedTexture2D(device, MakeKeyedDesc());
        if (!HasKeyedMutex(texture.Get())) {
            throw std::runtime_error("keyed texture does not report IDXGIKeyedMutex");
        }

        auto rawMutex = GetKeyedMutex(texture.Get());
        if (!rawMutex) {
            throw std::runtime_error("GetKeyedMutex returned null");
        }

        D3D11KeyedMutex mutex(texture.Get());
        if (!mutex || !mutex.Get()) {
            throw std::runtime_error("D3D11KeyedMutex did not attach");
        }
        if (mutex.IsAcquired()) {
            throw std::runtime_error("new wrapper unexpectedly acquired");
        }
    });

    TEST_RUN("Acquire release sequence", {
        auto texture = CreateSharedTexture2D(device, MakeKeyedDesc());
        D3D11KeyedMutex mutex(texture.Get());

        if (!mutex.Acquire(0, 1000)) {
            throw std::runtime_error("initial acquire key 0 timed out");
        }
        if (!mutex.IsAcquired()) {
            throw std::runtime_error("wrapper did not mark acquired");
        }
        mutex.Release(1);
        if (mutex.IsAcquired()) {
            throw std::runtime_error("wrapper still marked acquired after release");
        }

        mutex.AcquireOrThrow(1, 1000);
        mutex.Release(0);
    });

    TEST_RUN("Scoped acquire releases requested key", {
        auto texture = CreateSharedTexture2D(device, MakeKeyedDesc());
        D3D11KeyedMutex mutex(texture.Get());

        {
            D3D11ScopedKeyedMutexAcquire scoped(mutex, 0, 7, 1000);
            if (!scoped.Acquired() || !mutex.IsAcquired()) {
                throw std::runtime_error("scoped acquire did not acquire mutex");
            }
        }

        if (mutex.IsAcquired()) {
            throw std::runtime_error("scoped acquire did not release mutex");
        }
        if (!mutex.Acquire(7, 1000)) {
            throw std::runtime_error("scoped release key was not observable");
        }
        mutex.Release(0);
    });

    TEST_RUN("Opened shared texture keyed mutex sequence", {
        auto shared = CreateSharedTexture2DWithHandle(device, MakeKeyedDesc(48, 48));
        auto opened = OpenSharedTexture2D(device, shared.handle);

        D3D11KeyedMutex original(shared.Get());
        D3D11KeyedMutex openedMutex(opened.Get());

        original.AcquireOrThrow(0, 1000);
        original.Release(11);

        openedMutex.AcquireOrThrow(11, 1000);
        openedMutex.Release(0);
    });

    TEST_RUN("Move semantics", {
        auto texture = CreateSharedTexture2D(device, MakeKeyedDesc());
        D3D11KeyedMutex first(texture.Get());
        D3D11KeyedMutex moved(std::move(first));
        if (first || !moved) {
            throw std::runtime_error("D3D11KeyedMutex move construction failed");
        }

        {
            D3D11ScopedKeyedMutexAcquire scoped(moved, 0, 3, 1000);
            D3D11ScopedKeyedMutexAcquire movedScope(std::move(scoped));
            if (scoped.Acquired() || !movedScope.Acquired()) {
                throw std::runtime_error("scoped move construction failed");
            }
        }

        moved.AcquireOrThrow(3, 1000);
        moved.Release(0);
    });

    TEST_RUN("Invalid argument checks", {
        auto normal = CreateNormalTexture(device);
        if (HasKeyedMutex(normal.Get())) {
            throw std::runtime_error("normal texture unexpectedly exposes keyed mutex");
        }
        ExpectThrows("GetKeyedMutex null", [&] {
            (void)GetKeyedMutex(nullptr);
        });
        ExpectThrows("GetKeyedMutex no interface", [&] {
            (void)GetKeyedMutex(normal.Get());
        });
        ExpectThrows("Attach no interface", [&] {
            D3D11KeyedMutex mutex(normal.Get());
            (void)mutex;
        });
        ExpectThrows("Acquire without attach", [&] {
            D3D11KeyedMutex mutex;
            (void)mutex.Acquire(0, 0);
        });
        ExpectThrows("Release without acquire", [&] {
            auto texture = CreateSharedTexture2D(device, MakeKeyedDesc());
            D3D11KeyedMutex mutex(texture.Get());
            mutex.Release(0);
        });
        ExpectThrows("Reset while acquired", [&] {
            auto texture = CreateSharedTexture2D(device, MakeKeyedDesc());
            D3D11KeyedMutex mutex(texture.Get());
            mutex.AcquireOrThrow(0, 1000);
            try {
                mutex.Reset();
            } catch (...) {
                mutex.Release(0);
                throw;
            }
            mutex.Release(0);
        });
    });

    return TestUtil::Result("InteropKeyedMutex");
}
