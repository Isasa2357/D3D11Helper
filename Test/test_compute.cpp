#include "TestUtil.hpp"
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
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

const char* kTinyCs = R"(RWStructuredBuffer<uint> Buf : register(u0);
[numthreads(1,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){ Buf[0] = 7; })";

ComPtr<ID3D11ComputeShader> GetBoundComputeShader(ID3D11DeviceContext* ctx) {
    ComPtr<ID3D11ComputeShader> shader;
    ctx->CSGetShader(&shader, nullptr, nullptr);
    return shader;
}

} // namespace

int main() {
    auto core = TestUtil::MakeCore();
    auto* dev = core->GetDevice();
    auto* ctx = core->GetImmediateContext();

    TEST_RUN("ComputePipeline init", {
        const char* cs = R"(RWBuffer<float> B:register(u0);
[numthreads(1,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){B[0]=42.0;})";
        auto bc = CompileShaderFromSource_D3DCompile(cs, "CSMain", "cs_5_0");
        D3D11ComputePipeline p; p.Initialize(dev, bc);
        if (!p.GetShader()) throw std::runtime_error("null");
    });

    TEST_RUN("ComputePipeline invalid initialization", {
        auto bc = CompileShaderFromSource_D3DCompile(kTinyCs, "CSMain", "cs_5_0");
        ShaderBytecode empty;
        D3D11ComputePipeline p;
        ExpectThrows("null device", [&] { p.Initialize(nullptr, bc); });
        ExpectThrows("empty shader", [&] { p.Initialize(dev, empty); });
    });

    TEST_RUN("ComputePipeline bind and unbind state", {
        auto bc = CompileShaderFromSource_D3DCompile(kTinyCs, "CSMain", "cs_5_0");
        D3D11ComputePipeline p;

        p.Bind(nullptr);
        p.Unbind(nullptr);
        p.Bind(ctx); // uninitialized bind is intentionally a no-op.
        if (GetBoundComputeShader(ctx).Get() != nullptr) throw std::runtime_error("uninitialized bind should not set CS");

        p.Initialize(dev, bc);
        p.Bind(ctx);
        if (GetBoundComputeShader(ctx).Get() != p.GetShader()) throw std::runtime_error("bound CS mismatch");
        p.Unbind(ctx);
        if (GetBoundComputeShader(ctx).Get() != nullptr) throw std::runtime_error("CS should be unbound");
    });

    TEST_RUN("ComputePipeline dispatch rejects invalid state", {
        auto bc = CompileShaderFromSource_D3DCompile(kTinyCs, "CSMain", "cs_5_0");
        D3D11ComputePipeline uninitialized;
        ExpectThrows("uninitialized dispatch", [&] { uninitialized.Dispatch(ctx, 1, 1, 1); });

        D3D11ComputePipeline initialized;
        initialized.Initialize(dev, bc);
        ExpectThrows("null context dispatch", [&] { initialized.Dispatch(nullptr, 1, 1, 1); });
    });

    TEST_RUN("ComputePipeline GPU execution + readback", {
        const char* cs = R"(
RWStructuredBuffer<uint> Buf : register(u0);
[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){ Buf[id.x] = Buf[id.x] + 10; })";
        auto bc = CompileShaderFromSource_D3DCompile(cs, "CSMain", "cs_5_0");
        D3D11ComputePipeline pipeline; pipeline.Initialize(dev, bc);

        const UINT N = 64;
        std::vector<uint32_t> input(N);
        for (UINT i=0;i<N;++i) input[i]=i;

        D3D11_BUFFER_DESC bd{}; bd.ByteWidth=N*4; bd.Usage=D3D11_USAGE_DEFAULT;
        bd.BindFlags=D3D11_BIND_UNORDERED_ACCESS; bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride=4;
        D3D11_SUBRESOURCE_DATA srd{}; srd.pSysMem=input.data();
        ComPtr<ID3D11Buffer> buf; D3D11CORE_THROW_IF_FAILED(dev->CreateBuffer(&bd, &srd, &buf));

        D3D11_UNORDERED_ACCESS_VIEW_DESC ud{}; ud.Format=DXGI_FORMAT_UNKNOWN;
        ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements=N;
        ComPtr<ID3D11UnorderedAccessView> uav;
        D3D11CORE_THROW_IF_FAILED(dev->CreateUnorderedAccessView(buf.Get(), &ud, &uav));

        ID3D11UnorderedAccessView* uavs[]={uav.Get()};
        ctx->CSSetUnorderedAccessViews(0,1,uavs,nullptr);
        pipeline.Dispatch(ctx, 1, 1, 1);
        if (GetBoundComputeShader(ctx).Get() != nullptr) throw std::runtime_error("Dispatch should unbind compute shader");
        ID3D11UnorderedAccessView* n[]={nullptr}; ctx->CSSetUnorderedAccessViews(0,1,n,nullptr);

        D3D11StagingBuffer st; st.InitializeAsBuffer(dev, N*4);
        ctx->CopyResource(st.Get(), buf.Get());
        auto* r = static_cast<const uint32_t*>(st.Map(ctx));
        int err=0; for (UINT i=0;i<N;++i) if (r[i]!=i+10) ++err;
        const uint32_t first = r[0];
        st.Unmap(ctx);
        TestUtil::Log("  r[0]=" + std::to_string(first) + " errors=" + std::to_string(err));
        if (err) throw std::runtime_error(std::to_string(err) + " mismatches");
    });

    TEST_RUN("Flush after compute", {
        // 注意: RWBuffer<float>（typed UAV）と RAW ビュー（R32_TYPELESS + FLAG_RAW）は
        // 組み合わせてはいけない。RWBuffer<float> は typed UAV として作る必要がある。
        // ここでは値の解釈が一意な RWStructuredBuffer<float> を使って
        // Dispatch -> Flush -> Readback の順序と Flush の同期を検証する。
        const char* cs=R"(RWStructuredBuffer<float> B:register(u0);
[numthreads(1,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){ B[0]=42.0; })";
        auto bc = CompileShaderFromSource_D3DCompile(cs, "CSMain", "cs_5_0");
        D3D11ComputePipeline p; p.Initialize(dev, bc);

        D3D11_BUFFER_DESC bd{}; bd.ByteWidth=sizeof(float); bd.Usage=D3D11_USAGE_DEFAULT;
        bd.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
        bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; bd.StructureByteStride=sizeof(float);
        ComPtr<ID3D11Buffer> buf; D3D11CORE_THROW_IF_FAILED(dev->CreateBuffer(&bd, nullptr, &buf));

        D3D11_UNORDERED_ACCESS_VIEW_DESC ud{}; ud.Format=DXGI_FORMAT_UNKNOWN;
        ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements=1;
        ComPtr<ID3D11UnorderedAccessView> uav;
        D3D11CORE_THROW_IF_FAILED(dev->CreateUnorderedAccessView(buf.Get(), &ud, &uav));

        ID3D11UnorderedAccessView* uavs[]={uav.Get()};
        ctx->CSSetUnorderedAccessViews(0,1,uavs,nullptr);
        p.Dispatch(ctx,1,1,1);
        ID3D11UnorderedAccessView* n[]={nullptr}; ctx->CSSetUnorderedAccessViews(0,1,n,nullptr);
        core->Flush();

        D3D11StagingBuffer st; st.InitializeAsBuffer(dev, sizeof(float));
        ctx->CopyResource(st.Get(), buf.Get());
        auto* r = static_cast<const float*>(st.Map(ctx));
        float got = r[0];
        st.Unmap(ctx);
        if (got != 42.0f) throw std::runtime_error("expected 42, got " + std::to_string(got));
    });

    return TestUtil::Result("Compute");
}
