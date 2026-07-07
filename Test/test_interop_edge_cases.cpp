//
// test_interop_edge_cases.cpp - D3D11-only interop edge/timeout/validation tests.
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <functional>
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

D3D11SharedTexture2DDesc MakeSharedDesc(
    UINT width = 32,
    UINT height = 32,
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM) {

    D3D11SharedTexture2DDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.syncMode = D3D11SharedTextureSyncMode::SharedFence;
    return desc;
}

ComPtr<ID3D11Texture2D> CreatePlainTexture(ID3D11Device* device) {
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

    TEST_RUN("SharedHandle invalid and reset behavior", {
        D3D11SharedHandle empty;
        if (empty) {
            throw std::runtime_error("default shared handle unexpectedly valid");
        }
        ExpectThrows("Duplicate empty handle", [&] {
            (void)empty.Duplicate();
        });

        D3D11SharedHandle invalidValue(INVALID_HANDLE_VALUE);
        if (invalidValue) {
            throw std::runtime_error("INVALID_HANDLE_VALUE reported valid");
        }
        if (invalidValue.Release() != INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Release did not return stored INVALID_HANDLE_VALUE");
        }
        if (invalidValue) {
            throw std::runtime_error("released invalid handle still reports valid");
        }

        HANDLE first = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        HANDLE second = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!first || !second) {
            if (first) CloseHandle(first);
            if (second) CloseHandle(second);
            throw std::runtime_error("CreateEventW failed");
        }

        D3D11SharedHandle a(first);
        D3D11SharedHandle b(second);
        b = std::move(a);
        if (a) {
            throw std::runtime_error("move-assigned-from handle still valid");
        }
        if (!b) {
            throw std::runtime_error("move-assigned handle is invalid");
        }

        auto duplicate = b.Duplicate(SYNCHRONIZE);
        if (!duplicate) {
            throw std::runtime_error("duplicate with SYNCHRONIZE access is invalid");
        }

        HANDLE released = b.Release();
        if (!released) {
            throw std::runtime_error("Release returned null for valid handle");
        }
        CloseHandle(released);
    });

    TEST_RUN("SharedTexture misc flag construction", {
        const UINT fenceFlags = MakeSharedTextureMiscFlags(
            D3D11SharedTextureSyncMode::SharedFence,
            D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX);
        if ((fenceFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) == 0 ||
            (fenceFlags & D3D11_RESOURCE_MISC_SHARED) == 0 ||
            (fenceFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0 ||
            (fenceFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) == 0) {
            throw std::runtime_error("unexpected SharedFence misc flags");
        }

        const UINT keyedFlags = MakeSharedTextureMiscFlags(
            D3D11SharedTextureSyncMode::KeyedMutex,
            D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_TEXTURECUBE);
        if ((keyedFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) == 0 ||
            (keyedFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) == 0 ||
            (keyedFlags & D3D11_RESOURCE_MISC_SHARED) != 0 ||
            (keyedFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) == 0) {
            throw std::runtime_error("unexpected KeyedMutex misc flags");
        }
    });

    TEST_RUN("SharedTexture validation edge cases", {
        ExpectThrows("null device", [&] {
            (void)CreateSharedTexture2D(nullptr, MakeSharedDesc());
        });
        ExpectThrows("zero height", [&] {
            auto desc = MakeSharedDesc();
            desc.height = 0;
            (void)CreateSharedTexture2D(device, desc);
        });
        ExpectThrows("zero mip levels", [&] {
            auto desc = MakeSharedDesc();
            desc.mipLevels = 0;
            (void)CreateSharedTexture2D(device, desc);
        });
        ExpectThrows("zero array size", [&] {
            auto desc = MakeSharedDesc();
            desc.arraySize = 0;
            (void)CreateSharedTexture2D(device, desc);
        });
        ExpectThrows("zero sample count", [&] {
            auto desc = MakeSharedDesc();
            desc.sampleCount = 0;
            (void)CreateSharedTexture2D(device, desc);
        });
        ExpectThrows("dynamic shared texture", [&] {
            auto desc = MakeSharedDesc();
            desc.usage = D3D11_USAGE_DYNAMIC;
            desc.cpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
            (void)CreateSharedTexture2D(device, desc);
        });
        ExpectThrows("cpu access shared texture", [&] {
            auto desc = MakeSharedDesc();
            desc.cpuAccessFlags = D3D11_CPU_ACCESS_READ;
            (void)CreateSharedTexture2D(device, desc);
        });
        ExpectThrows("sharing flags in desc misc", [&] {
            auto desc = MakeSharedDesc();
            desc.miscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
            (void)CreateSharedTexture2D(device, desc);
        });
    });

    TEST_RUN("KeyedMutex timeout and scoped ReleaseNow", {
        auto desc = MakeSharedDesc();
        desc.syncMode = D3D11SharedTextureSyncMode::KeyedMutex;
        auto texture = CreateSharedTexture2D(device, desc);

        D3D11KeyedMutex mutex(texture.Get());

        // A fresh keyed mutex is initially available only for key 0.  Using a
        // different key with zero timeout checks the wrapper's timeout path
        // without relying on same-device contention behavior, which can return
        // DXGI_ERROR_INVALID_CALL on some drivers.
        if (mutex.Acquire(5, 0)) {
            mutex.Release(0);
            throw std::runtime_error("wrong initial key unexpectedly acquired");
        }

        mutex.AcquireOrThrow(0, 1000);
        mutex.Release(3);

        {
            D3D11ScopedKeyedMutexAcquire scoped(mutex, 3, 7, 1000);
            if (!scoped.Acquired() || !mutex.IsAcquired()) {
                throw std::runtime_error("scoped acquire did not acquire mutex");
            }
            scoped.ReleaseNow();
            if (scoped.Acquired() || mutex.IsAcquired()) {
                throw std::runtime_error("ReleaseNow did not release mutex");
            }
            scoped.ReleaseNow();
        }

        mutex.AcquireOrThrow(7, 1000);
        mutex.Release(0);
    });

    TEST_RUN("KeyedMutex invalid resource and self-acquire", {
        auto plain = CreatePlainTexture(device);
        if (HasKeyedMutex(plain.Get())) {
            throw std::runtime_error("plain texture unexpectedly has keyed mutex");
        }
        ExpectThrows("Attach null resource", [&] {
            D3D11KeyedMutex mutex(nullptr);
            (void)mutex;
        });
        ExpectThrows("Attach non-keyed resource", [&] {
            D3D11KeyedMutex mutex(plain.Get());
            (void)mutex;
        });

        auto desc = MakeSharedDesc();
        desc.syncMode = D3D11SharedTextureSyncMode::KeyedMutex;
        auto texture = CreateSharedTexture2D(device, desc);
        D3D11KeyedMutex mutex(texture.Get());
        mutex.AcquireOrThrow(0, 1000);
        try {
            ExpectThrows("Acquire while already acquired", [&] {
                (void)mutex.Acquire(0, 0);
            });
        } catch (...) {
            mutex.Release(0);
            throw;
        }
        mutex.Release(0);
    });

    TEST_RUN("Fence support null checks", {
        if (TryGetD3D11Device5(nullptr) != nullptr) {
            throw std::runtime_error("TryGetD3D11Device5(null) returned non-null");
        }
        if (TryGetD3D11DeviceContext4(nullptr) != nullptr) {
            throw std::runtime_error("TryGetD3D11DeviceContext4(null) returned non-null");
        }

        const auto nullInfo = CheckD3D11FenceSupport(nullptr, nullptr);
        if (nullInfo.supported || nullInfo.hasDevice5 || nullInfo.reason == nullptr || nullInfo.reason[0] == '\0') {
            throw std::runtime_error("unexpected null-device fence support info");
        }
        if (IsD3D11FenceSupported(nullptr, nullptr)) {
            throw std::runtime_error("null-device fence support reported true");
        }
    });

    return TestUtil::Result("InteropEdgeCases");
}
