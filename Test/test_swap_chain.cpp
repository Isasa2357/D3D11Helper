//
// test_swap_chain.cpp - D3D11SwapChain presentation helper tests
//
#include "TestUtil.hpp"
#include <D3D11Helper/D3D11Presentation/D3D11SwapChain.hpp>

#include <stdexcept>
#include <string>

using namespace D3D11CoreLib;

namespace {

const wchar_t* kWindowClassName = L"D3D11HelperSwapChainTestWindow";

void RegisterTestWindowClass() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClassName;

    const ATOM atom = RegisterClassW(&wc);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        throw std::runtime_error("RegisterClassW failed");
    }
}

class TestWindow {
public:
    TestWindow(int width, int height) {
        RegisterTestWindowClass();

        m_hwnd = CreateWindowExW(
            0,
            kWindowClassName,
            L"D3D11Helper SwapChain Test",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            width,
            height,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        if (!m_hwnd) {
            throw std::runtime_error("CreateWindowExW failed");
        }
    }

    ~TestWindow() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
        }
    }

    TestWindow(const TestWindow&) = delete;
    TestWindow& operator=(const TestWindow&) = delete;

    HWND Hwnd() const noexcept { return m_hwnd; }

private:
    HWND m_hwnd = nullptr;
};

} // unnamed namespace

int main() {
    TEST_RUN("D3D11SwapChain initialize and bind", {
        auto core = TestUtil::MakeCore();
        TestWindow window(160, 120);

        D3D11SwapChainDesc desc = {};
        desc.hwnd = window.Hwnd();
        desc.width = 96;
        desc.height = 64;
        desc.bufferCount = 2;
        desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.createDepthStencil = true;

        D3D11SwapChain swapChain;
        swapChain.Initialize(*core, desc);

        if (!swapChain) throw std::runtime_error("swapChain is invalid");
        if (!swapChain.Get()) throw std::runtime_error("native swap chain is null");
        if (swapChain.Width() != 96 || swapChain.Height() != 64) {
            throw std::runtime_error("unexpected swap chain size");
        }
        if (swapChain.BufferCount() != 2) {
            throw std::runtime_error("unexpected native buffer count");
        }
        if (!swapChain.CurrentRtv()) {
            throw std::runtime_error("current RTV is null");
        }
        if (!swapChain.HasDepthStencil() || !swapChain.Dsv()) {
            throw std::runtime_error("DSV was not created");
        }
        if (!swapChain.CurrentBackBuffer()) {
            throw std::runtime_error("current back buffer is null");
        }

        bool threwBackBuffer1 = false;
        try {
            (void)swapChain.BackBuffer(1);
        } catch (...) {
            threwBackBuffer1 = true;
        }
        if (!threwBackBuffer1) {
            throw std::runtime_error("BackBuffer(1) should not be cached for D3D11 flip-model swap chains");
        }

        auto* ctx = core->GetImmediateContext();
        swapChain.BindAndSetViewport(ctx);
        swapChain.Clear(ctx);
        core->Flush();
    });

    TEST_RUN("D3D11SwapChain resize", {
        auto core = TestUtil::MakeCore();
        TestWindow window(160, 120);

        D3D11SwapChainDesc desc = {};
        desc.hwnd = window.Hwnd();
        desc.width = 64;
        desc.height = 48;
        desc.bufferCount = 2;
        desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.createDepthStencil = true;

        auto swapChain = CreateSwapChain(*core, desc);
        swapChain.Resize(128, 72);

        if (swapChain.Width() != 128 || swapChain.Height() != 72) {
            throw std::runtime_error("resize did not update desc size");
        }
        if (swapChain.BufferCount() != 2) {
            throw std::runtime_error("native back buffer count changed unexpectedly");
        }
        if (!swapChain.CurrentRtv()) {
            throw std::runtime_error("current RTV is null after resize");
        }
        if (!swapChain.HasDepthStencil()) {
            throw std::runtime_error("depth target missing after resize");
        }

        auto* ctx = core->GetImmediateContext();
        swapChain.BindAndSetViewport(ctx);
        swapChain.Clear(ctx);
        core->Flush();
    });

    TEST_RUN("D3D11SwapChain invalid arguments", {
        auto core = TestUtil::MakeCore();
        TestWindow window(160, 120);

        bool threw = false;
        try {
            D3D11SwapChainDesc desc = {};
            desc.hwnd = nullptr;
            desc.width = 64;
            desc.height = 64;
            D3D11SwapChain swapChain;
            swapChain.Initialize(*core, desc);
        } catch (...) {
            threw = true;
        }
        if (!threw) throw std::runtime_error("null HWND did not throw");

        threw = false;
        try {
            D3D11SwapChainDesc desc = {};
            desc.hwnd = window.Hwnd();
            desc.width = 0;
            desc.height = 64;
            D3D11SwapChain swapChain;
            swapChain.Initialize(*core, desc);
        } catch (...) {
            threw = true;
        }
        if (!threw) throw std::runtime_error("zero size did not throw");

        threw = false;
        try {
            D3D11SwapChainDesc desc = {};
            desc.hwnd = window.Hwnd();
            desc.width = 64;
            desc.height = 64;
            desc.createDepthStencil = true;
            desc.depthFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            D3D11SwapChain swapChain;
            swapChain.Initialize(*core, desc);
        } catch (...) {
            threw = true;
        }
        if (!threw) throw std::runtime_error("invalid depth format did not throw");
    });

    return TestUtil::Result("D3D11SwapChain");
}
