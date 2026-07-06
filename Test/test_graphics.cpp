#include "TestUtil.hpp"
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <stdexcept>

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

void FillBasicInputLayout(GraphicsPipelineDesc& gd)
{
    gd.inputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
}

} // namespace

int main()
{
    auto core = TestUtil::MakeCore();
    auto* dev = core->GetDevice();

    TestUtil::Run("PipelineDefaults", [&] {
        auto r = PipelineDefaults::Rasterizer();
        if (r.CullMode != D3D11_CULL_BACK) throw std::runtime_error("unexpected cull mode");

        auto bo = PipelineDefaults::BlendOpaque();
        if (bo.RenderTarget[0].BlendEnable) throw std::runtime_error("opaque blend enabled");

        auto ba = PipelineDefaults::BlendAlpha();
        if (!ba.RenderTarget[0].BlendEnable) throw std::runtime_error("alpha blend disabled");

        auto dd = PipelineDefaults::DepthDisabled();
        if (dd.DepthEnable) throw std::runtime_error("depth disabled preset failed");

        auto de = PipelineDefaults::DepthDefault();
        if (!de.DepthEnable) throw std::runtime_error("depth default preset failed");
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

        D3D11GraphicsPipeline pipeline;
        pipeline.InitializeRaw(vsObj, psObj, layout, rs, bs, dss,
                               D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        if (pipeline.GetVertexShader() != vsObj.Get()) throw std::runtime_error("vertex shader mismatch");
        if (pipeline.GetPixelShader() != psObj.Get()) throw std::runtime_error("pixel shader mismatch");
        if (pipeline.GetInputLayout() != layout.Get()) throw std::runtime_error("input layout mismatch");
        if (pipeline.GetRasterizerState() != rs.Get()) throw std::runtime_error("rasterizer mismatch");
        if (pipeline.GetBlendState() != bs.Get()) throw std::runtime_error("blend state mismatch");
        if (pipeline.GetDepthStencilState() != dss.Get()) throw std::runtime_error("depth state mismatch");
    });

    return TestUtil::Result("test_graphics");
}
