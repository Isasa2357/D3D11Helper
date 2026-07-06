#pragma once
//
// D3D11ComputePipeline.hpp
//
// Compute Shader をまとめる。
//
// D3D12 との違い:
//   D3D12 では Root Signature + PSO が必要だが、D3D11 ではリソースを直接バインドする。
//   本クラスは ID3D11ComputeShader を保持し、Bind / Dispatch の便利メソッドを提供する.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11ShaderCompiler.hpp>

namespace D3D11CoreLib {

class D3D11ComputePipeline {
public:
    D3D11ComputePipeline() = default;
    ~D3D11ComputePipeline() = default;

    D3D11ComputePipeline(const D3D11ComputePipeline&)            = delete;
    D3D11ComputePipeline& operator=(const D3D11ComputePipeline&) = delete;
    D3D11ComputePipeline(D3D11ComputePipeline&&)                 = default;
    D3D11ComputePipeline& operator=(D3D11ComputePipeline&&)      = default;

    // ShaderBytecode から ID3D11ComputeShader を作成する。
    void Initialize(ID3D11Device* device, const ShaderBytecode& cs);

    // パイプラインをバインドする（CSSetShader）。
    void Bind(ID3D11DeviceContext* ctx) const;

    // バインド解除する。
    void Unbind(ID3D11DeviceContext* ctx) const;

    // Bind + Dispatch + Unbind をまとめた便利関数。
    // SRV / UAV / CBV は呼び出し側が事前にバインドしていること。
    void Dispatch(ID3D11DeviceContext* ctx,
                  UINT threadGroupCountX,
                  UINT threadGroupCountY = 1,
                  UINT threadGroupCountZ = 1) const;

    ID3D11ComputeShader* GetShader() const noexcept { return m_shader.Get(); }

private:
    ComPtr<ID3D11ComputeShader> m_shader;
};

} // namespace D3D11CoreLib
