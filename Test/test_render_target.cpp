//
// test_render_target.cpp - D3D11Presentation render target tests
//
#include "TestUtil.hpp"
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>

#include <stdexcept>

using namespace D3D11CoreLib;

int main() {
    auto core = TestUtil::MakeCore();
    ID3D11DeviceContext* ctx = core->GetImmediateContext();

    TEST_RUN("MakeViewport", {
        const auto vp = MakeViewport(128, 72, 1.0f, 2.0f, 0.25f, 0.75f);
        if (vp.Width != 128.0f || vp.Height != 72.0f) throw std::runtime_error("size mismatch");
        if (vp.TopLeftX != 1.0f || vp.TopLeftY != 2.0f) throw std::runtime_error("origin mismatch");
        if (vp.MinDepth != 0.25f || vp.MaxDepth != 0.75f) throw std::runtime_error("depth mismatch");
    });

    TEST_RUN("Create color-only render target", {
        D3D11RenderTargetDesc desc = {};
        desc.width = 64;
        desc.height = 32;
        desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.depthFormat = DXGI_FORMAT_UNKNOWN;
        desc.createShaderResource = true;

        auto target = CreateRenderTarget(*core, desc);
        if (!target) throw std::runtime_error("target is invalid");
        if (target.Width() != 64 || target.Height() != 32) throw std::runtime_error("size mismatch");
        if (!target.ColorTexture()) throw std::runtime_error("missing color texture");
        if (target.DepthTexture()) throw std::runtime_error("unexpected depth texture");
        if (!target.Rtv()) throw std::runtime_error("missing RTV");
        if (!target.Srv()) throw std::runtime_error("missing SRV");
        if (target.Dsv()) throw std::runtime_error("unexpected DSV");

        target.BindAndSetViewport(ctx);
        target.Clear(ctx);
        UnbindRenderTargets(ctx);
    });

    TEST_RUN("Create color + depth render target", {
        auto target = CreateRenderTarget(
            *core,
            80,
            48,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            true,
            DXGI_FORMAT_D24_UNORM_S8_UINT,
            false);

        if (!target) throw std::runtime_error("target is invalid");
        if (!target.ColorTexture()) throw std::runtime_error("missing color texture");
        if (!target.DepthTexture()) throw std::runtime_error("missing depth texture");
        if (!target.Rtv()) throw std::runtime_error("missing RTV");
        if (target.Srv()) throw std::runtime_error("unexpected SRV");
        if (!target.Dsv()) throw std::runtime_error("missing DSV");

        auto vp = target.Viewport();
        if (vp.Width != 80.0f || vp.Height != 48.0f) throw std::runtime_error("viewport mismatch");

        target.Bind(ctx);
        target.SetViewport(ctx);
        target.Clear(ctx);
        UnbindRenderTargets(ctx, 1);
    });

    TEST_RUN("Reset render target", {
        auto target = CreateRenderTarget(*core, 16, 16);
        if (!target) throw std::runtime_error("target is invalid before reset");
        target.Reset();
        if (target) throw std::runtime_error("target is still valid after reset");
        if (target.Rtv() || target.Srv() || target.Dsv()) throw std::runtime_error("views were not released");
    });

    TEST_RUN("Invalid descriptors throw", {
        bool ok = false;
        try {
            D3D11RenderTargetDesc desc = {};
            desc.width = 0;
            desc.height = 16;
            CreateRenderTarget(*core, desc);
        } catch (const std::exception&) {
            ok = true;
        }
        if (!ok) throw std::runtime_error("zero width did not throw");

        ok = false;
        try {
            D3D11RenderTargetDesc desc = {};
            desc.width = 16;
            desc.height = 16;
            desc.colorFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
            CreateRenderTarget(*core, desc);
        } catch (const std::exception&) {
            ok = true;
        }
        if (!ok) throw std::runtime_error("depth color format did not throw");

        ok = false;
        try {
            D3D11RenderTargetDesc desc = {};
            desc.width = 16;
            desc.height = 16;
            desc.sampleCount = 4;
            CreateRenderTarget(*core, desc);
        } catch (const std::exception&) {
            ok = true;
        }
        if (!ok) throw std::runtime_error("MSAA descriptor did not throw");
    });

    core->Flush();
    return TestUtil::Result("RenderTarget");
}
