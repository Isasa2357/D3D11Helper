#pragma once
//
// D3D11GraphicsPipeline.hpp
//
// グラフィクス用の VS / PS / InputLayout / RasterizerState / BlendState /
// DepthStencilState をまとめる。
// D3D12GraphicsPipeline と対称的に、次の3段のカスタマイズ経路を提供する:
//
//   1. かんたん経路   : GraphicsPipelineDesc に vs/ps/入力レイアウト だけ指定。
//                       blend / rasterizer / depth は未指定なら PipelineDefaults を使う。
//   2. 上書き経路     : GraphicsPipelineDesc の rasterizer/blend/depthStencil ポインタに
//                       自前 desc（または PipelineDefaults の戻り値を書き換えたもの）を指す。
//   3. フル制御経路   : InitializeRaw に個別の State Object を渡す。
//
// D3D12 との違い:
//   - D3D11 には PSO（Pipeline State Object）が無い。
//     代わりに ID3D11RasterizerState / ID3D11BlendState / ID3D11DepthStencilState を
//     個別に作成し、Bind 時にまとめてセットする。
//   - D3D11 には Root Signature が無い。リソースは直接バインドする。
//   - InputLayout は VS のバイトコードから生成する.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11ShaderCompiler.hpp>

#include <vector>

namespace D3D11CoreLib {

// --------------------------------------------------------------------------
// よく使う state のプリセット。D3D12 版と対称的な API。
// --------------------------------------------------------------------------
namespace PipelineDefaults {

D3D11_RASTERIZER_DESC Rasterizer(
    D3D11_CULL_MODE cull = D3D11_CULL_BACK,
    D3D11_FILL_MODE fill = D3D11_FILL_SOLID,
    bool frontCounterClockwise = false);

D3D11_BLEND_DESC BlendOpaque();
D3D11_BLEND_DESC BlendAlpha();

D3D11_DEPTH_STENCIL_DESC DepthDisabled();
D3D11_DEPTH_STENCIL_DESC DepthDefault(
    bool depthWrite = true,
    D3D11_COMPARISON_FUNC depthFunc = D3D11_COMPARISON_LESS);

} // namespace PipelineDefaults

// --------------------------------------------------------------------------
// かんたん経路 / 上書き経路で使う記述子。
// --------------------------------------------------------------------------
struct GraphicsPipelineDesc {
    ShaderBytecode vs;
    ShaderBytecode ps;   // 空でも可（深度プリパスなど）

    // 入力レイアウト。空なら SV_VertexID 等で頂点を生成するシェーダを想定。
    std::vector<D3D11_INPUT_ELEMENT_DESC> inputLayout;

    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // state 上書き（nullptr のままなら既定値）。
    const D3D11_RASTERIZER_DESC*    rasterizer   = nullptr;
    const D3D11_BLEND_DESC*         blend        = nullptr;
    const D3D11_DEPTH_STENCIL_DESC* depthStencil = nullptr;

    // depthStencil 未指定時に自動選択に使うフラグ。
    // true なら DepthDefault()、false なら DepthDisabled()。
    bool enableDepthByDefault = false;

    // BlendState の BlendFactor。nullptr ならデフォルト（1,1,1,1）。
    const float* blendFactor = nullptr;
    UINT         sampleMask  = 0xFFFFFFFF;

    // ステンシル参照値（DepthStencilState で stencil を有効にした場合）。
    UINT stencilRef = 0;
};

class D3D11GraphicsPipeline {
public:
    D3D11GraphicsPipeline() = default;
    ~D3D11GraphicsPipeline() = default;

    D3D11GraphicsPipeline(const D3D11GraphicsPipeline&)            = delete;
    D3D11GraphicsPipeline& operator=(const D3D11GraphicsPipeline&) = delete;
    D3D11GraphicsPipeline(D3D11GraphicsPipeline&&)                 = default;
    D3D11GraphicsPipeline& operator=(D3D11GraphicsPipeline&&)      = default;

    // かんたん経路 / 上書き経路。
    void Initialize(ID3D11Device* device, const GraphicsPipelineDesc& desc);

    // フル制御経路。既に作成済みの State Object を渡す。
    void InitializeRaw(
        ComPtr<ID3D11VertexShader>      vs,
        ComPtr<ID3D11PixelShader>       ps,
        ComPtr<ID3D11InputLayout>       inputLayout,
        ComPtr<ID3D11RasterizerState>   rasterizerState,
        ComPtr<ID3D11BlendState>        blendState,
        ComPtr<ID3D11DepthStencilState> depthStencilState,
        D3D11_PRIMITIVE_TOPOLOGY        topology    = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        const float*                    blendFactor = nullptr,
        UINT                            sampleMask  = 0xFFFFFFFF,
        UINT                            stencilRef  = 0);

    // パイプラインをコンテキストにバインドする。
    void Bind(ID3D11DeviceContext* ctx) const;

    // バインドを解除する（シェーダと State を nullptr にする）。
    void Unbind(ID3D11DeviceContext* ctx) const;

    // --- アクセサ ---
    ID3D11VertexShader*      GetVertexShader()      const noexcept { return m_vs.Get(); }
    ID3D11PixelShader*       GetPixelShader()       const noexcept { return m_ps.Get(); }
    ID3D11InputLayout*       GetInputLayout()       const noexcept { return m_inputLayout.Get(); }
    ID3D11RasterizerState*   GetRasterizerState()   const noexcept { return m_rasterizer.Get(); }
    ID3D11BlendState*        GetBlendState()        const noexcept { return m_blend.Get(); }
    ID3D11DepthStencilState* GetDepthStencilState() const noexcept { return m_depthStencil.Get(); }
    D3D11_PRIMITIVE_TOPOLOGY GetTopology()          const noexcept { return m_topology; }

private:
    ComPtr<ID3D11VertexShader>      m_vs;
    ComPtr<ID3D11PixelShader>       m_ps;
    ComPtr<ID3D11InputLayout>       m_inputLayout;
    ComPtr<ID3D11RasterizerState>   m_rasterizer;
    ComPtr<ID3D11BlendState>        m_blend;
    ComPtr<ID3D11DepthStencilState> m_depthStencil;

    D3D11_PRIMITIVE_TOPOLOGY m_topology    = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    float                    m_blendFactor[4] = { 1, 1, 1, 1 };
    UINT                     m_sampleMask  = 0xFFFFFFFF;
    UINT                     m_stencilRef  = 0;
};

} // namespace D3D11CoreLib
