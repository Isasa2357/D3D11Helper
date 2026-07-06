#pragma once
//
// D3D11BindingSet.hpp
//
// Lightweight compute-stage resource binding helpers.
//
// D3D11ComputeBindingSet intentionally stores raw view/state pointers. It does
// not own resources; callers keep resources alive for the duration of binding.
// D3D11ScopedComputeBindings owns only the previously-bound state it restores.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

#include <array>

namespace D3D11CoreLib {

struct D3D11ComputeBindingSet {
    static constexpr UINT ShaderResourceSlotCount = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
    static constexpr UINT UnorderedAccessSlotCount = D3D11_1_UAV_SLOT_COUNT;
    static constexpr UINT ConstantBufferSlotCount = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
    static constexpr UINT SamplerSlotCount = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;

    std::array<ID3D11ShaderResourceView*, ShaderResourceSlotCount> shaderResources{};
    std::array<ID3D11UnorderedAccessView*, UnorderedAccessSlotCount> unorderedAccesses{};
    std::array<UINT, UnorderedAccessSlotCount> uavInitialCounts{};
    std::array<ID3D11Buffer*, ConstantBufferSlotCount> constantBuffers{};
    std::array<ID3D11SamplerState*, SamplerSlotCount> samplers{};

    UINT firstShaderResourceSlot = 0;
    UINT shaderResourceCount = 0;
    UINT firstUnorderedAccessSlot = 0;
    UINT unorderedAccessCount = 0;
    UINT firstConstantBufferSlot = 0;
    UINT constantBufferCount = 0;
    UINT firstSamplerSlot = 0;
    UINT samplerCount = 0;

    void SetShaderResource(UINT slot, ID3D11ShaderResourceView* srv) noexcept;
    void SetUnorderedAccess(UINT slot,
                            ID3D11UnorderedAccessView* uav,
                            UINT initialCount = UINT(-1)) noexcept;
    void SetConstantBuffer(UINT slot, ID3D11Buffer* buffer) noexcept;
    void SetSampler(UINT slot, ID3D11SamplerState* sampler) noexcept;

    void Clear() noexcept;
    void Bind(ID3D11DeviceContext* ctx) const;
    void Unbind(ID3D11DeviceContext* ctx) const;
};

void BindComputeShaderResources(ID3D11DeviceContext* ctx,
                                UINT startSlot,
                                UINT count,
                                ID3D11ShaderResourceView* const* srvs);
void BindComputeUnorderedAccessViews(ID3D11DeviceContext* ctx,
                                     UINT startSlot,
                                     UINT count,
                                     ID3D11UnorderedAccessView* const* uavs,
                                     const UINT* initialCounts = nullptr);
void BindComputeConstantBuffers(ID3D11DeviceContext* ctx,
                                UINT startSlot,
                                UINT count,
                                ID3D11Buffer* const* buffers);
void BindComputeSamplers(ID3D11DeviceContext* ctx,
                         UINT startSlot,
                         UINT count,
                         ID3D11SamplerState* const* samplers);

void UnbindComputeShaderResources(ID3D11DeviceContext* ctx, UINT startSlot, UINT count);
void UnbindComputeUnorderedAccessViews(ID3D11DeviceContext* ctx, UINT startSlot, UINT count);
void UnbindComputeConstantBuffers(ID3D11DeviceContext* ctx, UINT startSlot, UINT count);
void UnbindComputeSamplers(ID3D11DeviceContext* ctx, UINT startSlot, UINT count);
void D3D11UnbindComputeResources(ID3D11DeviceContext* ctx);

class D3D11ScopedComputeBindings {
public:
    D3D11ScopedComputeBindings(ID3D11DeviceContext* ctx, const D3D11ComputeBindingSet& bindings);
    ~D3D11ScopedComputeBindings();

    D3D11ScopedComputeBindings(const D3D11ScopedComputeBindings&) = delete;
    D3D11ScopedComputeBindings& operator=(const D3D11ScopedComputeBindings&) = delete;
    D3D11ScopedComputeBindings(D3D11ScopedComputeBindings&&) = delete;
    D3D11ScopedComputeBindings& operator=(D3D11ScopedComputeBindings&&) = delete;

    void Restore();

private:
    ID3D11DeviceContext* m_ctx = nullptr;
    D3D11ComputeBindingSet m_ranges{};
    std::array<ComPtr<ID3D11ShaderResourceView>, D3D11ComputeBindingSet::ShaderResourceSlotCount> m_shaderResources{};
    std::array<ComPtr<ID3D11UnorderedAccessView>, D3D11ComputeBindingSet::UnorderedAccessSlotCount> m_unorderedAccesses{};
    std::array<ComPtr<ID3D11Buffer>, D3D11ComputeBindingSet::ConstantBufferSlotCount> m_constantBuffers{};
    std::array<ComPtr<ID3D11SamplerState>, D3D11ComputeBindingSet::SamplerSlotCount> m_samplers{};
};

} // namespace D3D11CoreLib
