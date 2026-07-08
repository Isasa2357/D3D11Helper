#include "TestUtil.hpp"
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace D3D11CoreLib;

namespace {

const char* kVsPsHlsl = R"(
struct V { float4 p : SV_POSITION; float3 c : COLOR; };
V VSMain(float3 pos : POSITION, float3 col : COLOR)
{
    V o;
    o.p = float4(pos, 1);
    o.c = col;
    return o;
}
float4 PSMain(V i) : SV_TARGET
{
    return float4(i.c, 1);
}
)";

const char* kVertexIdHlsl = R"(
float4 VSMain(uint vid : SV_VertexID) : SV_POSITION
{
    float2 p[3] = { float2(-1.0f, -1.0f), float2(3.0f, -1.0f), float2(-1.0f, 3.0f) };
    return float4(p[vid], 0.0f, 1.0f);
}
float4 PSMain() : SV_TARGET
{
    return float4(0.0f, 1.0f, 0.0f, 1.0f);
}
)";

void ExpectThrows(const char* label, const std::function<void()>& fn)
{
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

void FillBasicInputLayout(GraphicsPipelineDesc& gd)
{
    gd.inputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
}

ComPtr<ID3D11Texture2D> CreateRenderTargetTexture(ID3D11Device* device, UINT width, UINT height)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;

    ComPtr<ID3D11Texture2D> tex;
    D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, nullptr, &tex));
    return tex;
}

std::vector<uint8_t> ReadTexture2D(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* texture,
    UINT width,
    UINT height)
{
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    ComPtr<ID3D11Texture2D> staging;
    D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, nullptr, &staging));
    context->CopyResource(staging.Get(), texture);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    D3D11CORE_THROW_IF_FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<uint8_t> out(static_cast<size_t>(width) * height * 4u);
    for (UINT y = 0; y < height; ++y) {
        std::memcpy(
            out.data() + static_cast<size_t>(y) * width * 4u,
            static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch,
            static_cast<size_t>(width) * 4u);
    }
    context->Unmap(staging.Get(), 0);
    return out;
}

} // namespace

int main()
{
    auto core = TestUtil::MakeCore();
    auto* dev = core->GetDevice();
    auto* ctx = core->GetImmediateContext();

    TestUtil::Run("PipelineDefaults", [&] {
        auto r = PipelineDefaults::Rasterizer();
        if (r.CullMode != D3D11_CULL_BACK) throw std::runtime_error("unexpected cull mode");
        if (r.FillMode != D3D11_FILL_SOLID) throw std::runtime_error("unexpected fill mode");
        if (!r.DepthClipEnable) throw std::runtime_error("depth clip disabled");

        auto wire = PipelineDefaults::Rasterizer(D3D11_CULL_FRONT, D3D11_FILL_WIREFRAME, true);
        if (wire.CullMode != D3D11_CULL_FRONT) throw std::runtime_error("front cull preset failed");
        if (wire.FillMode != D3D11_FILL_WIREFRAME) throw std::runtime_error("wire preset failed");
        if (!wire.FrontCounterClockwise) throw std::runtime_error("front ccw preset failed");

        auto bo = PipelineDefaults::BlendOpaque();
        if (bo.RenderTarget[0].BlendEnable) throw std::runtime_error("opaque blend enabled");
        if (bo.RenderTarget[0].RenderTargetWriteMask != D3D11_COLOR_WRITE_ENABLE_ALL) throw std::runtime_error("write mask failed");

        auto ba = PipelineDefaults::BlendAlpha();
        if (!ba.RenderTarget[0].BlendEnable) throw std::runtime_error("alpha blend disabled");
        if (ba.RenderTarget[0].SrcBlend != D3D11_BLEND_SRC_ALPHA) throw std::runtime_error("alpha src blend failed");
        if (ba.RenderTarget[0].DestBlend != D3D11_BLEND_INV_SRC_ALPHA) throw std::runtime_error("alpha dst blend failed");

        auto dd = PipelineDefaults::DepthDisabled();
        if (dd.DepthEnable) throw std::runtime_error("depth disabled preset failed");
        if (dd.StencilEnable) throw std::runtime_error("stencil should be disabled");

        auto de = PipelineDefaults::DepthDefault();
        if (!de.DepthEnable) throw std::runtime_error("depth default preset failed");
        if (de.DepthWriteMask != D3D11_DEPTH_WRITE_MASK_ALL) throw std::runtime_error("depth write default failed");
        if (de.DepthFunc != D3D11_COMPARISON_LESS) throw std::runtime_error("depth func default failed");

        auto ro = PipelineDefaults::DepthDefault(false, D3D11_COMPARISON_GREATER_EQUAL);
        if (ro.DepthWriteMask != D3D11_DEPTH_WRITE_MASK_ZERO) throw std::runtime_error("read-only depth failed");
        if (ro.DepthFunc != D3D11_COMPARISON_GREATER_EQUAL) throw std::runtime_error("custom depth func failed");
    });

    TestUtil::Run("GraphicsPipeline invalid initialization", [&] {
        auto vs = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "VSMain", "vs_5_0");
        auto ps = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "PSMain", "ps_5_0");

        GraphicsPipelineDesc gd;
        gd.vs = vs;
        gd.ps = ps;
        FillBasicInputLayout(gd);

        D3D11GraphicsPipeline pipeline;
        ExpectThrows("null device", [&] { pipeline.Initialize(nullptr, gd); });

        GraphicsPipelineDesc emptyVs = gd;
        emptyVs.vs = ShaderBytecode{};
        ExpectThrows("empty vs", [&] { pipeline.Initialize(dev, emptyVs); });

        GraphicsPipelineDesc badLayout = gd;
        badLayout.inputLayout = {
            {"MISSING", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        ExpectThrows("bad input layout", [&] { pipeline.Initialize(dev, badLayout); });
    });

    TestUtil::Run("GraphicsPipeline bind and unbind null/uninitialized", [&] {
        D3D11GraphicsPipeline pipeline;
        pipeline.Bind(nullptr);
        pipeline.Unbind(nullptr);
        pipeline.Bind(ctx);

        ComPtr<ID3D11VertexShader> vs;
        ctx->VSGetShader(&vs, nullptr, nullptr);
        if (vs) throw std::runtime_error("uninitialized bind should not set VS");

        pipeline.Unbind(ctx);
        ctx->VSGetShader(&vs, nullptr, nullptr);
        if (vs) throw std::runtime_error("unbind should clear VS");
    });

    TestUtil::Run("GraphicsPipeline Initialize", [&] {
        auto vs = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "VSMain", "vs_5_0");
        auto ps = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "PSMain", "ps_5_0");

        GraphicsPipelineDesc gd;
        gd.vs = std::move(vs);
        gd.ps = std::move(ps);
        FillBasicInputLayout(gd);

        D3D11GraphicsPipeline pipeline;
        pipeline.Initialize(dev, gd);

        if (!pipeline.GetVertexShader() || !pipeline.GetPixelShader() || !pipeline.GetInputLayout()) {
            throw std::runtime_error("pipeline object is incomplete");
        }
    });

    TestUtil::Run("GraphicsPipeline bind sets context state", [&] {
        auto vs = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "VSMain", "vs_5_0");
        auto ps = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "PSMain", "ps_5_0");

        GraphicsPipelineDesc gd;
        gd.vs = std::move(vs);
        gd.ps = std::move(ps);
        FillBasicInputLayout(gd);
        gd.topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        gd.sampleMask = 0x00FFFFFFu;
        gd.stencilRef = 7;
        const float blendFactor[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
        gd.blendFactor = blendFactor;

        auto noCull = PipelineDefaults::Rasterizer(D3D11_CULL_NONE);
        auto alpha = PipelineDefaults::BlendAlpha();
        auto depth = PipelineDefaults::DepthDefault(false, D3D11_COMPARISON_LESS_EQUAL);
        gd.rasterizer = &noCull;
        gd.blend = &alpha;
        gd.depthStencil = &depth;

        D3D11GraphicsPipeline pipeline;
        pipeline.Initialize(dev, gd);
        pipeline.Bind(ctx);

        ComPtr<ID3D11VertexShader> boundVs;
        ComPtr<ID3D11PixelShader> boundPs;
        ComPtr<ID3D11InputLayout> boundLayout;
        ComPtr<ID3D11RasterizerState> boundRs;
        ComPtr<ID3D11BlendState> boundBlend;
        ComPtr<ID3D11DepthStencilState> boundDepth;
        D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        float actualBlendFactor[4] = {};
        UINT actualSampleMask = 0;
        UINT actualStencilRef = 0;

        ctx->VSGetShader(&boundVs, nullptr, nullptr);
        ctx->PSGetShader(&boundPs, nullptr, nullptr);
        ctx->IAGetInputLayout(&boundLayout);
        ctx->IAGetPrimitiveTopology(&topology);
        ctx->RSGetState(&boundRs);
        ctx->OMGetBlendState(&boundBlend, actualBlendFactor, &actualSampleMask);
        ctx->OMGetDepthStencilState(&boundDepth, &actualStencilRef);

        if (boundVs.Get() != pipeline.GetVertexShader()) throw std::runtime_error("VS binding mismatch");
        if (boundPs.Get() != pipeline.GetPixelShader()) throw std::runtime_error("PS binding mismatch");
        if (boundLayout.Get() != pipeline.GetInputLayout()) throw std::runtime_error("input layout binding mismatch");
        if (topology != D3D11_PRIMITIVE_TOPOLOGY_LINELIST) throw std::runtime_error("topology binding mismatch");
        if (boundRs.Get() != pipeline.GetRasterizerState()) throw std::runtime_error("rasterizer binding mismatch");
        if (boundBlend.Get() != pipeline.GetBlendState()) throw std::runtime_error("blend binding mismatch");
        if (boundDepth.Get() != pipeline.GetDepthStencilState()) throw std::runtime_error("depth binding mismatch");
        if (actualSampleMask != gd.sampleMask) throw std::runtime_error("sample mask mismatch");
        if (actualStencilRef != gd.stencilRef) throw std::runtime_error("stencil ref mismatch");
        for (int i = 0; i < 4; ++i) {
            if (actualBlendFactor[i] != blendFactor[i]) throw std::runtime_error("blend factor mismatch");
        }

        pipeline.Unbind(ctx);
        ctx->VSGetShader(&boundVs, nullptr, nullptr);
        ctx->PSGetShader(&boundPs, nullptr, nullptr);
        ctx->IAGetInputLayout(&boundLayout);
        if (boundVs || boundPs || boundLayout) throw std::runtime_error("unbind did not clear shader/layout state");
    });

    TestUtil::Run("GraphicsPipeline Tier2 override states", [&] {
        auto vs = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "VSMain", "vs_5_0");
        auto ps = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "PSMain", "ps_5_0");

        GraphicsPipelineDesc gd;
        gd.vs = std::move(vs);
        gd.ps = std::move(ps);
        FillBasicInputLayout(gd);

        D3D11_RASTERIZER_DESC noCull = PipelineDefaults::Rasterizer(D3D11_CULL_NONE);
        D3D11_BLEND_DESC alphaBlend = PipelineDefaults::BlendAlpha();
        D3D11_DEPTH_STENCIL_DESC depthOn = PipelineDefaults::DepthDefault();
        gd.rasterizer = &noCull;
        gd.blend = &alphaBlend;
        gd.depthStencil = &depthOn;

        D3D11GraphicsPipeline pipeline;
        pipeline.Initialize(dev, gd);

        D3D11_RASTERIZER_DESC actualR = {};
        pipeline.GetRasterizerState()->GetDesc(&actualR);
        if (actualR.CullMode != D3D11_CULL_NONE) throw std::runtime_error("rasterizer override failed");

        D3D11_BLEND_DESC actualB = {};
        pipeline.GetBlendState()->GetDesc(&actualB);
        if (!actualB.RenderTarget[0].BlendEnable) throw std::runtime_error("blend override failed");

        D3D11_DEPTH_STENCIL_DESC actualD = {};
        pipeline.GetDepthStencilState()->GetDesc(&actualD);
        if (!actualD.DepthEnable) throw std::runtime_error("depth override failed");
    });

    TestUtil::Run("GraphicsPipeline enableDepthByDefault", [&] {
        auto vs = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "VSMain", "vs_5_0");
        auto ps = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "PSMain", "ps_5_0");

        GraphicsPipelineDesc gd;
        gd.vs = std::move(vs);
        gd.ps = std::move(ps);
        FillBasicInputLayout(gd);
        gd.enableDepthByDefault = true;

        D3D11GraphicsPipeline pipeline;
        pipeline.Initialize(dev, gd);

        D3D11_DEPTH_STENCIL_DESC desc = {};
        pipeline.GetDepthStencilState()->GetDesc(&desc);
        if (!desc.DepthEnable) throw std::runtime_error("enableDepthByDefault failed");
    });

    TestUtil::Run("GraphicsPipeline optional pixel shader", [&] {
        auto vs = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "VSMain", "vs_5_0");
        GraphicsPipelineDesc gd;
        gd.vs = std::move(vs);
        FillBasicInputLayout(gd);

        D3D11GraphicsPipeline pipeline;
        pipeline.Initialize(dev, gd);
        if (pipeline.GetPixelShader() != nullptr) throw std::runtime_error("expected null pixel shader");
        pipeline.Bind(ctx);
        ComPtr<ID3D11PixelShader> boundPs;
        ctx->PSGetShader(&boundPs, nullptr, nullptr);
        if (boundPs) throw std::runtime_error("PS should remain null");
        pipeline.Unbind(ctx);
    });

    TestUtil::Run("GraphicsPipeline InitializeRaw", [&] {
        auto vs = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "VSMain", "vs_5_0");
        auto ps = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "PSMain", "ps_5_0");

        ComPtr<ID3D11VertexShader> vsObj;
        ComPtr<ID3D11PixelShader> psObj;
        D3D11CORE_THROW_IF_FAILED(dev->CreateVertexShader(vs.Data(), vs.Size(), nullptr, &vsObj));
        D3D11CORE_THROW_IF_FAILED(dev->CreatePixelShader(ps.Data(), ps.Size(), nullptr, &psObj));

        D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        ComPtr<ID3D11InputLayout> layout;
        D3D11CORE_THROW_IF_FAILED(dev->CreateInputLayout(layoutDesc, 2, vs.Data(), vs.Size(), &layout));

        D3D11_RASTERIZER_DESC rd = PipelineDefaults::Rasterizer(D3D11_CULL_FRONT);
        ComPtr<ID3D11RasterizerState> rs;
        D3D11CORE_THROW_IF_FAILED(dev->CreateRasterizerState(&rd, &rs));

        D3D11_BLEND_DESC bd = PipelineDefaults::BlendOpaque();
        ComPtr<ID3D11BlendState> bs;
        D3D11CORE_THROW_IF_FAILED(dev->CreateBlendState(&bd, &bs));

        D3D11_DEPTH_STENCIL_DESC dsd = PipelineDefaults::DepthDisabled();
        ComPtr<ID3D11DepthStencilState> dss;
        D3D11CORE_THROW_IF_FAILED(dev->CreateDepthStencilState(&dsd, &dss));

        const float blendFactor[4] = { 0.1f, 0.2f, 0.3f, 0.4f };
        D3D11GraphicsPipeline pipeline;
        pipeline.InitializeRaw(vsObj, psObj, layout, rs, bs, dss,
                               D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
                               blendFactor,
                               0x0FFFFFFFu,
                               3);

        if (pipeline.GetVertexShader() != vsObj.Get()) throw std::runtime_error("vertex shader mismatch");
        if (pipeline.GetPixelShader() != psObj.Get()) throw std::runtime_error("pixel shader mismatch");
        if (pipeline.GetInputLayout() != layout.Get()) throw std::runtime_error("input layout mismatch");
        if (pipeline.GetRasterizerState() != rs.Get()) throw std::runtime_error("rasterizer mismatch");
        if (pipeline.GetBlendState() != bs.Get()) throw std::runtime_error("blend state mismatch");
        if (pipeline.GetDepthStencilState() != dss.Get()) throw std::runtime_error("depth state mismatch");
        if (pipeline.GetTopology() != D3D11_PRIMITIVE_TOPOLOGY_POINTLIST) throw std::runtime_error("topology mismatch");

        pipeline.Bind(ctx);
        D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        float actualBlendFactor[4] = {};
        UINT sampleMask = 0;
        UINT stencilRef = 0;
        ComPtr<ID3D11BlendState> boundBlend;
        ComPtr<ID3D11DepthStencilState> boundDepth;
        ctx->IAGetPrimitiveTopology(&topology);
        ctx->OMGetBlendState(&boundBlend, actualBlendFactor, &sampleMask);
        ctx->OMGetDepthStencilState(&boundDepth, &stencilRef);
        if (topology != D3D11_PRIMITIVE_TOPOLOGY_POINTLIST) throw std::runtime_error("raw topology bind mismatch");
        if (sampleMask != 0x0FFFFFFFu) throw std::runtime_error("raw sample mask mismatch");
        if (stencilRef != 3) throw std::runtime_error("raw stencil ref mismatch");
        for (int i = 0; i < 4; ++i) {
            if (actualBlendFactor[i] != blendFactor[i]) throw std::runtime_error("raw blend factor mismatch");
        }
        pipeline.Unbind(ctx);
    });

    TestUtil::Run("GraphicsPipeline offscreen draw with input layout", [&] {
        const UINT size = 32;
        auto rt = CreateRenderTargetTexture(dev, size, size);
        ComPtr<ID3D11RenderTargetView> rtv;
        D3D11CORE_THROW_IF_FAILED(dev->CreateRenderTargetView(rt.Get(), nullptr, &rtv));

        auto vs = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "VSMain", "vs_5_0");
        auto ps = CompileShaderFromSource_D3DCompile(kVsPsHlsl, "PSMain", "ps_5_0");
        GraphicsPipelineDesc gd;
        gd.vs = std::move(vs);
        gd.ps = std::move(ps);
        FillBasicInputLayout(gd);
        auto noCull = PipelineDefaults::Rasterizer(D3D11_CULL_NONE);
        gd.rasterizer = &noCull;

        D3D11GraphicsPipeline pipeline;
        pipeline.Initialize(dev, gd);

        struct V { float p[3]; float c[3]; };
        const V verts[] = {
            {{-1.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 3.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{-1.0f,  3.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        };
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(verts);
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = verts;
        ComPtr<ID3D11Buffer> vb;
        D3D11CORE_THROW_IF_FAILED(dev->CreateBuffer(&bd, &init, &vb));

        const float clear[4] = { 0, 0, 0, 1 };
        ctx->ClearRenderTargetView(rtv.Get(), clear);
        ID3D11RenderTargetView* rtvs[] = { rtv.Get() };
        ctx->OMSetRenderTargets(1, rtvs, nullptr);
        D3D11_VIEWPORT vp{};
        vp.Width = static_cast<float>(size);
        vp.Height = static_cast<float>(size);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);
        pipeline.Bind(ctx);
        UINT stride = sizeof(V);
        UINT offset = 0;
        ID3D11Buffer* vbs[] = { vb.Get() };
        ctx->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
        ctx->Draw(3, 0);
        pipeline.Unbind(ctx);
        ctx->OMSetRenderTargets(0, nullptr, nullptr);

        const auto pixels = ReadTexture2D(dev, ctx, rt.Get(), size, size);
        const uint8_t* center = pixels.data() + static_cast<size_t>((size / 2) * size + (size / 2)) * 4u;
        if (!(center[0] > 200 && center[1] < 60 && center[2] < 60)) {
            throw std::runtime_error("expected red center pixel");
        }
    });

    TestUtil::Run("GraphicsPipeline offscreen draw without input layout", [&] {
        const UINT size = 32;
        auto rt = CreateRenderTargetTexture(dev, size, size);
        ComPtr<ID3D11RenderTargetView> rtv;
        D3D11CORE_THROW_IF_FAILED(dev->CreateRenderTargetView(rt.Get(), nullptr, &rtv));

        auto vs = CompileShaderFromSource_D3DCompile(kVertexIdHlsl, "VSMain", "vs_5_0");
        auto ps = CompileShaderFromSource_D3DCompile(kVertexIdHlsl, "PSMain", "ps_5_0");
        GraphicsPipelineDesc gd;
        gd.vs = std::move(vs);
        gd.ps = std::move(ps);
        auto noCull = PipelineDefaults::Rasterizer(D3D11_CULL_NONE);
        gd.rasterizer = &noCull;

        D3D11GraphicsPipeline pipeline;
        pipeline.Initialize(dev, gd);
        if (pipeline.GetInputLayout() != nullptr) throw std::runtime_error("expected null input layout");

        const float clear[4] = { 0, 0, 0, 1 };
        ctx->ClearRenderTargetView(rtv.Get(), clear);
        ID3D11RenderTargetView* rtvs[] = { rtv.Get() };
        ctx->OMSetRenderTargets(1, rtvs, nullptr);
        D3D11_VIEWPORT vp{};
        vp.Width = static_cast<float>(size);
        vp.Height = static_cast<float>(size);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);
        pipeline.Bind(ctx);
        ctx->Draw(3, 0);
        pipeline.Unbind(ctx);
        ctx->OMSetRenderTargets(0, nullptr, nullptr);

        const auto pixels = ReadTexture2D(dev, ctx, rt.Get(), size, size);
        const uint8_t* center = pixels.data() + static_cast<size_t>((size / 2) * size + (size / 2)) * 4u;
        if (!(center[0] < 60 && center[1] > 200 && center[2] < 60)) {
            throw std::runtime_error("expected green center pixel");
        }
    });

    return TestUtil::Result("test_graphics");
}
