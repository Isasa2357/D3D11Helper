//
// 19_PresentationWindow / main.cpp
//
// D3D11Presentation の D3D11SwapChain wrapper を使った最小ウィンドウ描画サンプル。
//
//   - Win32 window
//   - D3D11SwapChain による backbuffer RTV / optional DSV 管理
//   - WM_SIZE に応じた Resize
//   - D3D11GraphicsPipeline で三角形を描画
//
// 既存の 03_HelloTriangle は低レベル helper を直接使う例です。
// このサンプルは v1.3.0 Presentation layer の上位 API を使う例です。
//
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace D3D11CoreLib;

namespace {

constexpr UINT kInitialWidth  = 1280;
constexpr UINT kInitialHeight = 720;
constexpr UINT kBufferCount   = 2;
constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

bool g_quit = false;
bool g_minimized = false;
bool g_pendingResize = false;
UINT g_pendingWidth = kInitialWidth;
UINT g_pendingHeight = kInitialHeight;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY:
        g_quit = true;
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) {
            g_minimized = true;
            return 0;
        }

        g_minimized = false;
        g_pendingWidth = static_cast<UINT>(LOWORD(lp));
        g_pendingHeight = static_cast<UINT>(HIWORD(lp));
        g_pendingResize = (g_pendingWidth != 0 && g_pendingHeight != 0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

struct Vertex {
    float position[3];
    float color[3];
};

const char* kTriangleHlsl = R"(
struct VSInput
{
    float3 pos   : POSITION;
    float3 color : COLOR;
};

struct PSInput
{
    float4 pos   : SV_POSITION;
    float3 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.pos = float4(input.pos, 1.0);
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(input.color, 1.0);
}
)";

HWND CreateSampleWindow(HINSTANCE hInst) {
    const wchar_t* className = L"D3D11HelperPresentationWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));

    if (!RegisterClassW(&wc)) {
        throw std::runtime_error("RegisterClassW failed");
    }

    RECT rc = { 0, 0, static_cast<LONG>(kInitialWidth), static_cast<LONG>(kInitialHeight) };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"D3D11Helper - Presentation Window (resize, ESC to quit)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        hInst,
        nullptr);

    if (!hwnd) {
        UnregisterClassW(className, hInst);
        throw std::runtime_error("CreateWindowExW failed");
    }

    ShowWindow(hwnd, SW_SHOW);
    return hwnd;
}

D3D11GraphicsPipeline CreateTrianglePipeline(ID3D11Device* device) {
    auto vs = CompileShaderFromSource_D3DCompile(kTriangleHlsl, "VSMain", "vs_5_0");
    auto ps = CompileShaderFromSource_D3DCompile(kTriangleHlsl, "PSMain", "ps_5_0");

    GraphicsPipelineDesc desc;
    desc.vs = std::move(vs);
    desc.ps = std::move(ps);
    desc.inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3D11_RASTERIZER_DESC noCull = PipelineDefaults::Rasterizer(D3D11_CULL_NONE);
    desc.rasterizer = &noCull;

    D3D11GraphicsPipeline pipeline;
    pipeline.Initialize(device, desc);
    return pipeline;
}

} // namespace

int main() {
    const wchar_t* className = L"D3D11HelperPresentationWindow";
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    HWND hwnd = nullptr;

    try {
        hwnd = CreateSampleWindow(hInst);

        D3D11CoreConfig config;
        config.enableDebugLayer = true;
        config.enableInfoQueue = true;
        auto core = D3D11Core::CreateShared(config);

        auto* ctx = core->GetImmediateContext();
        auto* device = core->GetDevice();

        D3D11SwapChainDesc swapDesc = {};
        swapDesc.hwnd = hwnd;
        swapDesc.width = kInitialWidth;
        swapDesc.height = kInitialHeight;
        swapDesc.bufferCount = kBufferCount;
        swapDesc.format = kBackBufferFormat;
        swapDesc.createDepthStencil = true;
        swapDesc.clearColor[0] = 0.06f;
        swapDesc.clearColor[1] = 0.08f;
        swapDesc.clearColor[2] = 0.12f;
        swapDesc.clearColor[3] = 1.0f;

        D3D11SwapChain swapChain = CreateSwapChain(*core, swapDesc);

        D3D11GraphicsPipeline pipeline = CreateTrianglePipeline(device);

        const Vertex vertices[] = {
            { {  0.0f,  0.65f, 0.0f }, { 1.0f, 0.25f, 0.20f } },
            { {  0.65f, -0.55f, 0.0f }, { 0.20f, 1.0f, 0.35f } },
            { { -0.65f, -0.55f, 0.0f }, { 0.30f, 0.45f, 1.0f } },
        };

        auto vertexBuffer = CreateBuffer(
            *core,
            static_cast<UINT>(sizeof(vertices)),
            D3D11_USAGE_DEFAULT,
            D3D11_BIND_VERTEX_BUFFER,
            0,
            0,
            vertices);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        ID3D11Buffer* vertexBufferPtr = vertexBuffer.AsBuffer();

        std::cout << "D3D11Helper Presentation Window sample" << std::endl;
        std::cout << "  - Drag the window border to test Resize()." << std::endl;
        std::cout << "  - Press ESC to quit." << std::endl;

        while (!g_quit) {
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (g_quit) {
                break;
            }

            if (g_pendingResize) {
                g_pendingResize = false;
                if (g_pendingWidth > 0 && g_pendingHeight > 0 &&
                    (g_pendingWidth != swapChain.Width() || g_pendingHeight != swapChain.Height())) {
                    swapChain.Resize(g_pendingWidth, g_pendingHeight);
                    std::cout << "Resized to " << g_pendingWidth << " x " << g_pendingHeight << std::endl;
                }
            }

            if (g_minimized) {
                Sleep(16);
                continue;
            }

            swapChain.Clear(ctx);
            swapChain.BindAndSetViewport(ctx);

            pipeline.Bind(ctx);
            ctx->IASetVertexBuffers(0, 1, &vertexBufferPtr, &stride, &offset);
            ctx->Draw(3, 0);
            pipeline.Unbind(ctx);

            swapChain.Present(1, 0);
        }

        core->Flush();

        if (hwnd && IsWindow(hwnd)) {
            DestroyWindow(hwnd);
        }
        UnregisterClassW(className, hInst);
        return 0;
    }
    catch (const std::exception& e) {
        std::string message = std::string("Error: ") + e.what();
        MessageBoxA(hwnd, message.c_str(), "19_PresentationWindow", MB_OK | MB_ICONERROR);

        if (hwnd && IsWindow(hwnd)) {
            DestroyWindow(hwnd);
        }
        UnregisterClassW(className, hInst);
        return 1;
    }
}
