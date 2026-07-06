#pragma once
//
// D3D11BindingSet.hpp - compute-stage resource binding helpers
//
// Lightweight helpers for binding compute-stage SRV/UAV/CB/sampler ranges.
// Binding sets store non-owning raw D3D11 pointers. Scoped bindings capture
// previous state with ComPtr and restore it on destruction.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

#include <array>

namespace D3D11CoreLib {

class D3D11ComputeBindingSet {
public:
    static constexpr UINT ShaderResourceSlotCount = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
    static constexpr UINT UnorderedAccessSlotCount = D3D11_1_UAV_SLOT_COUNT;
    static constexpr UINT ConstantBufferSlotCount = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
    static constexpr UINT SamplerSlotCount = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
    static constexpr UINT KeepUavInitialCount = UINT(-1);

    void Clear() noexcept;
    bool Empty() const noexcept;

    void SetShaderResource(UINT slot, ID3D11ShaderResourceView* srv);
    void SetUnorderedAccess(UINT slot,
                            ID3D11UnorderedAccessView* uav,
                            UINT initialCount = KeepUavInitialCount);
    void SetConstantBuffer(UINT slot, ID3D11Buffer* buffer);
    void SetSampler(UINT slot, ID3D11SamplerState* sampler);

    void Bind(ID3D11DeviceContext* context) const;
    void Unbind(ID3D11DeviceContext* context) const;

private:
    struct SlotRange {
        UINT first = 0;
        UINT count = 0;

        bool Empty() const noexcept { return count == 0; }
        void Clear() noexcept { first = 0; count = 0; }
        void Include(UINT slot) noexcept;
    };

    std::array<ID3D11ShaderResourceView*, ShaderResourceSlotCount> srvs_ = {};
    std::array<ID3D11UnorderedAccessView*, UnorderedAccessSlotCount> uavs_ = {};
    std::array<UINT, UnorderedAccessSlotCount> uavInitialCounts_ = {};
    std::array<ID3D11Buffer*, ConstantBufferSlotCount> constantBuffers_ = {};
    std::array<ID3D11SamplerState*, SamplerSlotCount> samplers_ = {};

    SlotRange srvRange_;
    SlotRange uavRange_;
    SlotRange constantBufferRange_;
    SlotRange samplerRange_;

    friend class D3D11ScopedComputeBindings;
};

void BindComputeShaderResources(
    ID3D11DeviceContext* context,
    UINT startSlot,
    UINT count,
    ID3D11ShaderResourceView* const* srvs);

void BindComputeUnorderedAccessViews(
    ID3D11DeviceContext* context,
    UINT startSlot,
    UINT count,
    ID3D11UnorderedAccessView* const* uavs,
    const UINT* initialCounts = nullptr);

void BindComputeConstantBuffers(
    ID3D11DeviceContext* context,
    UINT startSlot,
    UINT count,
    ID3D11Buffer* const* buffers);

void BindComputeSamplers(
    ID3D11DeviceContext* context,
    UINT startSlot,
    UINT count,
    ID3D11SamplerState* const* samplers);

void UnbindComputeShaderResources(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void UnbindComputeUnorderedAccessViews(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void UnbindComputeConstantBuffers(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void UnbindComputeSamplers(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void D3D11UnbindComputeResources(ID3D11DeviceContext* context);

class D3D11ScopedComputeBindings {
public:
    D3D11ScopedComputeBindings(ID3D11DeviceContext* context, const D3D11ComputeBindingSet& bindings);
    ~D3D11ScopedComputeBindings() noexcept;

    D3D11ScopedComputeBindings(const D3D11ScopedComputeBindings&) = delete;
    D3D11ScopedComputeBindings& operator=(const D3D11ScopedComputeBindings&) = delete;

    D3D11ScopedComputeBindings(D3D11ScopedComputeBindings&& other) noexcept;
    D3D11ScopedComputeBindings& operator=(D3D11ScopedComputeBindings&& other) noexcept;

    // Restores captured UAV view bindings. D3D11 does not expose current UAV
    // append/consume counters, so Restore() preserves counters by passing
    // nullptr initial counts when rebinding UAVs.
    void Restore() noexcept;

private:
    using SlotRange = D3D11ComputeBindingSet::SlotRange;

    void Capture(const D3D11ComputeBindingSet& bindings);
    void MoveFrom(D3D11ScopedComputeBindings& other) noexcept;
    void ClearCapturedState() noexcept;

    ID3D11DeviceContext* context_ = nullptr;
    bool restored_ = true;

    SlotRange srvRange_;
    SlotRange uavRange_;
    SlotRange constantBufferRange_;
    SlotRange samplerRange_;

    std::array<ComPtr<ID3D11ShaderResourceView>, D3D11ComputeBindingSet::ShaderResourceSlotCount> srvs_ = {};
    std::array<ComPtr<ID3D11UnorderedAccessView>, D3D11ComputeBindingSet::UnorderedAccessSlotCount> uavs_ = {};
    std::array<ComPtr<ID3D11Buffer>, D3D11ComputeBindingSet::ConstantBufferSlotCount> constantBuffers_ = {};
    std::array<ComPtr<ID3D11SamplerState>, D3D11ComputeBindingSet::SamplerSlotCount> samplers_ = {};
};

} // namespace D3D11CoreLib

