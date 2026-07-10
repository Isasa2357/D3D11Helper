#include "TestUtil.hpp"
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>
using namespace D3D11CoreLib;

int main() {
    static_assert(!std::is_copy_constructible_v<D3D11MappedSubresource>);
    static_assert(!std::is_copy_assignable_v<D3D11MappedSubresource>);
    static_assert(std::is_nothrow_move_constructible_v<D3D11MappedSubresource>);
    static_assert(std::is_nothrow_move_assignable_v<D3D11MappedSubresource>);

    auto core = TestUtil::MakeCore();
    auto* ctx = core->GetImmediateContext();

    TEST_RUN("StagingBuffer buffer readback", {
        float src[]{1.f,2.f,3.f,4.f};
        auto buf = CreateBuffer(*core, sizeof(src), D3D11_USAGE_DEFAULT,
                                D3D11_BIND_VERTEX_BUFFER, 0, 0, src);
        D3D11StagingBuffer st;
        st.InitializeAsBuffer(core->GetDevice(), sizeof(src));
        ctx->CopyResource(st.Get(), buf.Get());
        auto* p = static_cast<const float*>(st.Map(ctx));
        if (p[0]!=1.f || p[1]!=2.f || p[2]!=3.f || p[3]!=4.f)
            throw std::runtime_error("data mismatch");
        st.Unmap(ctx);
    });
    TEST_RUN("StagingBuffer texture readback", {
        const UINT W=4, H=4;
        std::vector<uint8_t> px(W*H*4);
        for (UINT i=0;i<W*H;++i){ px[i*4]=uint8_t(i); px[i*4+1]=uint8_t(i*2); px[i*4+2]=uint8_t(i*3); px[i*4+3]=255; }
        auto tex = CreateTexture2DFromRGBA(*core, px.data(), W, H);
        D3D11StagingBuffer st;
        st.InitializeAsTexture2D(core->GetDevice(), W, H, DXGI_FORMAT_R8G8B8A8_UNORM);
        ctx->CopyResource(st.Get(), tex.Get());
        auto* p = static_cast<const uint8_t*>(st.Map(ctx));
        if (p[0]!=0 || p[3]!=255) throw std::runtime_error("pixel mismatch");
        st.Unmap(ctx);
    });
    TEST_RUN("StagingBuffer double Map same pointer", {
        D3D11StagingBuffer st;
        st.InitializeAsBuffer(core->GetDevice(), 64);
        float d[]{1.f};
        auto buf = CreateBuffer(*core, sizeof(d), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, d);
        ctx->CopyResource(st.Get(), buf.Get());
        auto* p1 = st.Map(ctx); auto* p2 = st.Map(ctx);
        if (p1 != p2) throw std::runtime_error("different pointers");
        st.Unmap(ctx);
    });
    TEST_RUN("Scoped buffer map unmaps on destruction", {
        float src[]{5.f,6.f,7.f,8.f};
        auto buf = CreateBuffer(*core, sizeof(src), D3D11_USAGE_DEFAULT,
                                D3D11_BIND_VERTEX_BUFFER, 0, 0, src);
        D3D11StagingBuffer st;
        st.InitializeAsBuffer(core->GetDevice(), sizeof(src));
        ctx->CopyResource(st.Get(), buf.Get());
        {
            auto mapped = st.MapScoped(ctx);
            const float* p = mapped.DataAs<float>();
            if (!mapped || p[0] != 5.f || p[3] != 8.f)
                throw std::runtime_error("scoped buffer data mismatch");
            if (!st.IsMapped()) throw std::runtime_error("staging buffer not marked mapped");
        }
        if (st.IsMapped()) throw std::runtime_error("scoped map did not unmap");
        auto mappedAgain = st.MapScoped(ctx);
        if (!mappedAgain) throw std::runtime_error("remap after scope failed");
    });
    TEST_RUN("Scoped texture map exposes pitches", {
        const UINT W=4, H=4;
        std::vector<uint8_t> px(W*H*4, 17u);
        auto tex = CreateTexture2DFromRGBA(*core, px.data(), W, H);
        D3D11StagingBuffer st;
        st.InitializeAsTexture2D(core->GetDevice(), W, H, DXGI_FORMAT_R8G8B8A8_UNORM);
        ctx->CopyResource(st.Get(), tex.Get());
        auto mapped = st.MapScoped(ctx);
        if (mapped.RowPitch() < W * 4u) throw std::runtime_error("invalid row pitch");
        if (mapped.RowPitch() != st.GetMappedRowPitch() ||
            mapped.DepthPitch() != st.GetMappedDepthPitch())
            throw std::runtime_error("mapped pitch accessor mismatch");
        if (mapped.DataAs<uint8_t>()[0] != 17u) throw std::runtime_error("texture data mismatch");
    });
    TEST_RUN("Scoped map move transfers unmap ownership", {
        float src[]{9.f};
        auto buf = CreateBuffer(*core, sizeof(src), D3D11_USAGE_DEFAULT,
                                D3D11_BIND_VERTEX_BUFFER, 0, 0, src);
        D3D11StagingBuffer st;
        st.InitializeAsBuffer(core->GetDevice(), sizeof(src));
        ctx->CopyResource(st.Get(), buf.Get());
        D3D11MappedSubresource moved;
        {
            auto original = st.MapScoped(ctx);
            moved = std::move(original);
            if (original) throw std::runtime_error("moved-from guard still owns mapping");
            if (!moved) throw std::runtime_error("move did not transfer mapping");
        }
        if (!st.IsMapped()) throw std::runtime_error("mapping ended before moved guard reset");
        moved.Reset();
        if (st.IsMapped()) throw std::runtime_error("moved guard did not unmap");
    });
    TEST_RUN("Scoped map rejects competing ownership", {
        D3D11StagingBuffer st;
        st.InitializeAsBuffer(core->GetDevice(), 64);
        float d[]{1.f};
        auto buf = CreateBuffer(*core, sizeof(d), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, d);
        ctx->CopyResource(st.Get(), buf.Get());
        auto first = st.MapScoped(ctx);
        bool threw = false;
        try { (void)st.MapScoped(ctx); }
        catch (const std::runtime_error&) { threw = true; }
        if (!threw) throw std::runtime_error("second scoped map was accepted");
    });

    return TestUtil::Result("Staging");
}
